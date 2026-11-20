// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: MIT
#include "CyGameCaptureAutomationClient.hpp"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace cymcp;
namespace cygc
{
namespace
{
Json NativeCall(const std::filesystem::path &, const Json &, const Cancellation &);
Json Read(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw Error("FileError", "Cannot open the selected instance/arguments file");
    std::string bytes((std::istreambuf_iterator<char>(file)), {});
    return parse_json(bytes);
}
Json ConnectionConfig(const std::filesystem::path &input)
{
    auto address = path_utf8(input);
    if (address.starts_with("http://") || address.starts_with("https://"))
    {
        auto parsed = parse_url(address);
        if (!parsed.loopback)
            throw Error("PermissionDenied", "Expected a loopback MCP address");
        if (parsed.target == "/")
        {
            while (address.ends_with('/'))
                address.pop_back();
            address += "/mcp";
        }
        return {{"url", address}};
    }
    return Read(input);
}
} // namespace
std::shared_ptr<McpClient> ConnectInstance(const std::filesystem::path &path, Json *descriptor)
{
    auto config = ConnectionConfig(path);
    auto url = config.at("url").get<std::string>();
    if (!parse_url(url).loopback)
        throw Error("PermissionDenied", "Application connection files must address a loopback instance");
    auto client = std::make_shared<McpClient>(url, config.value("token", std::string()));
    client->connect();
    if (descriptor)
        *descriptor = std::move(config);
    return client;
}
std::shared_ptr<Registry> InstanceTools(std::shared_ptr<McpClient> remote)
{
    auto tools = std::make_shared<Registry>(Policy::allow_all());
    for (const auto &description : remote->list_tools())
    {
        Operation op;
        op.name = description.at("name");
        op.description = description.value("description", std::string());
        op.input_schema = description.at("inputSchema");
        op.output_schema = description.value("outputSchema", Json{{"type", "object"}});
        op.read_only = description.value("annotations", Json::object()).value("readOnlyHint", false);
        op.handler = [remote, name = op.name](const Json &arguments, const CallContext &ctx) {
            auto result = remote->call_tool(name, arguments, ctx.cancellation);
            if (result.value("isError", false))
            {
                Json details;
                for (const auto &item : result.value("content", Json::array()))
                    if (item.value("type", std::string()) == "text")
                    {
                        try
                        {
                            details = parse_json(item.at("text"));
                        }
                        catch (...)
                        {
                        }
                        if (details.is_object() && details.contains("code"))
                            break;
                    }
                throw Error(details.is_object() ? details.value("code", std::string("RemoteOperationFailed"))
                                                : "RemoteOperationFailed",
                            details.is_object() ? details.value("message", std::string("Remote operation failed"))
                                                : "Remote operation failed");
            }
            if (result.contains("structuredContent"))
                return result["structuredContent"];
            return Json{{"content", result.value("content", Json::array())}};
        };
        tools->add(std::move(op));
    }
    return tools;
}
void AddAllowedPeerTools(Registry &tools, const std::filesystem::path &file, bool allow_mutations,
                         const std::filesystem::path &controlling_instance)
{
    const auto peers = Read(file);
    if (!peers.is_array())
        throw Error("InvalidArguments", "External tool configuration must be an array");
    for (const auto &peer : peers)
    {
        auto client = ConnectInstance(std::filesystem::u8path(peer.at("connection").get<std::string>()));
        auto allowed = peer.at("tools").get<std::vector<std::string>>();
        const auto prefix = peer.at("prefix").get<std::string>();
        if (peer.value("notifications", false))
            client->start_notifications();
        for (const auto &name : allowed)
        {
            auto description = client->get_tool(name);
            const bool read = description.value("annotations", Json::object()).value("readOnlyHint", false);
            Operation op;
            op.name = prefix + "." + name;
            op.description = "Authorized tool from " + prefix + ": " + description.value("description", std::string());
            op.input_schema = description.at("inputSchema");
            op.output_schema = description.value("outputSchema", Json{{"type", "object"}});
            op.read_only = read;
            op.handler = [client, name, read, allow_mutations, controlling_instance,
                          peer_writes = peer.value("allow_mutations", false)](const Json &arguments,
                                                                              const CallContext &context) {
                if (!read && (!allow_mutations || !peer_writes))
                    throw Error(
                        "AccessDenied",
                        "Remote mutations require both application permission and an explicit peer allowlist grant");
                if (!read && !controlling_instance.empty() &&
                    !NativeCall(controlling_instance,
                                {{"method", "invoke"}, {"name", "app.status"}, {"arguments", Json::object()}},
                                context.cancellation)
                         .value("remote_control", false))
                    throw Error("AccessDenied", "Application remote control permission was revoked");
                auto reply = client->call_tool(name, arguments, context.cancellation);
                if (reply.value("isError", false))
                {
                    Json failure;
                    for (const auto &block : reply.value("content", Json::array()))
                        if (block.value("type", std::string()) == "text")
                        {
                            try
                            {
                                failure = parse_json(block.at("text"));
                            }
                            catch (...)
                            {
                            }
                            if (failure.is_object() && failure.contains("code"))
                                break;
                        }
                    throw Error(failure.is_object() ? failure.value("code", std::string("RemoteOperationFailed"))
                                                    : "RemoteOperationFailed",
                                failure.is_object() ? failure.value("message", std::string("Remote operation failed"))
                                                    : "The authorized remote tool reported an error");
                };
                return reply.contains("structuredContent") ? reply["structuredContent"]
                                                           : Json{{"content", reply.value("content", Json::array())}};
            };
            tools.add(std::move(op));
        }
    }
}
namespace
{
Json NativeCall(const std::filesystem::path &file, const Json &body, const Cancellation &cancel = {})
{
    auto config = ConnectionConfig(file);
    std::string url = config.at("url");
    if (!parse_url(url).loopback || !url.ends_with("/mcp"))
        throw Error("PermissionDenied", "Expected a loopback application connection");
    HttpRequest request;
    request.method = "POST";
    request.url = url.substr(0, url.size() - 3) + "v1";
    request.headers = {{"Content-Type", "application/json"}};
    if (auto token = config.value("token", std::string()); !token.empty())
        request.headers["Authorization"] = "Bearer " + token;
    request.body = body.dump();
    auto response = HttpClient().perform(request, cancel);
    auto result = parse_json(response.body);
    if (response.status != 200)
    {
        auto error = result.value("error", Json::object());
        throw Error(error.value("code", std::string("ConnectionFailed")),
                    error.value("message", std::string("Application request failed")));
    }
    return result;
}
std::shared_ptr<Registry> FullTools(const std::filesystem::path &file)
{
    auto tools = std::make_shared<Registry>(Policy::allow_all());
    const auto catalogue = NativeCall(file, {{"method", "describe"}});
    for (const auto &description : catalogue.at("operations"))
    {
        Operation op;
        op.name = description.at("name");
        op.description = description.at("description");
        op.input_schema = description.at("inputSchema");
        op.output_schema = description.value("outputSchema", Json{{"type", "object"}});
        op.read_only = description.at("annotations").value("readOnlyHint", false);
        op.handler = [file, name = op.name](const Json &arguments, const CallContext &context) {
            return NativeCall(file, {{"method", "invoke"}, {"name", name}, {"arguments", arguments}},
                              context.cancellation);
        };
        tools->add(std::move(op));
    }
    return tools;
}
} // namespace
int AutomationMain(int argc, wchar_t **argv)
{
    if (argc < 2)
        return -1;
    const std::wstring entry = argv[1];
    if (entry != L"--cli" && entry != L"--mcp" && entry != L"--help" && entry != L"--version")
        return -1;
    try
    {
        if (entry == L"--help")
        {
            std::cout
                << "Free is the default profile. Use --url http://127.0.0.1:PORT/mcp without credentials.\nOther "
                   "profiles may require a token or host operation permission.\nCyGameCaptureAI <request.json>  "
                   "(existing frame assistant)\nCyGameCaptureCLI instances | providers "
                   "[--discover]\nCyGameCaptureCLI list|describe NAME|call NAME --instance FILE [--args JSON|@FILE] "
                   "[--full]\nCyGameCaptureCLI mcp-config --instance FILE\nCyGameCaptureAI --cli list|describe "
                   "NAME|call NAME --instance CONNECTION.json [--args JSON|@FILE]\nCyGameCaptureAI --mcp --instance "
                   "CONNECTION.json\n--version returns JSON. Diagnostics go to stderr. Controlled profiles require "
                   "permission in "
                   "the running application; --confirm cannot bypass it.\n";
            return 0;
        }
        if (entry == L"--version")
        {
            std::cout << Json({{"application", "0.1.0"},
                               {"sdk", version},
                               {"sdk_revision", "34d4bcae126bc83d1a2925b5047dac36e3d09d92"}})
                             .dump()
                      << '\n';
            return 0;
        }
        std::string command = entry == L"--mcp" ? "mcp" : argc > 2 ? path_utf8(argv[2]) : "help", name;
        std::filesystem::path connection;
        Json arguments = Json::object();
        bool discover = false, full = false;
        for (int i = entry == L"--mcp" ? 2 : 3; i < argc; ++i)
        {
            std::wstring arg = argv[i];
            if (arg == L"--full")
                full = true;
            else if (arg == L"--discover")
                discover = true;
            else if ((arg == L"--instance" || arg == L"--url") && i + 1 < argc)
                connection = argv[++i];
            else if (arg == L"--args" && i + 1 < argc)
            {
                std::wstring value = argv[++i];
                arguments =
                    !value.empty() && value.front() == L'@' ? Read(value.substr(1)) : parse_json(path_utf8(value));
            }
            else if (name.empty() && (command == "call" || command == "describe"))
                name = path_utf8(arg);
            else
                throw Error("InvalidArguments", "Unknown or incomplete CLI option");
        }
        if (command == "providers")
        {
            ProviderRegistry providers;
            Json found = providers.describe();
            if (discover)
            {
                found = Json::array();
                for (const auto &item : providers.discover())
                    found.push_back({{"id", item.provider_id},
                                     {"state", item.state},
                                     {"models", item.models},
                                     {"error_code", item.error_code}});
            }
            std::cout << Json({{"providers", found}}).dump() << '\n';
            return 0;
        }
        if (command == "instances")
        {
            wchar_t *local = nullptr;
            size_t n = 0;
            _wdupenv_s(&local, &n, L"LOCALAPPDATA");
            std::filesystem::path directory =
                std::filesystem::path(local ? local : L".") / L"CyGameCapture" / L"Automation";
            free(local);
            Json instances = Json::array();
            std::error_code ec;
            for (const auto &item : std::filesystem::directory_iterator(directory, ec))
            {
                if (item.path().extension() != L".json")
                    continue;
                try
                {
                    auto config = Read(item.path());
                    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, config.at("pid"));
                    if (!process)
                        continue;
                    bool alive = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
                    CloseHandle(process);
                    if (alive)
                        instances.push_back(
                            {{"component", config.at("component")},
                             {"pid", config.at("pid")},
                             {"connection", path_utf8(item.path())},
                             {"url", config.at("url")},
                             {"access_profile", config.value("access_profile", std::string("protected_access"))}});
                }
                catch (...)
                {
                }
            }
            std::cout << Json({{"instances", instances}}).dump() << '\n';
            return 0;
        }
        if (connection.empty())
            throw Error("InvalidArguments", "Select a running application with --url ADDRESS or --instance "
                                            "CONNECTION.json; use instances to list them");
        if (command == "mcp-config")
        {
            wchar_t module[32768] = {};
            GetModuleFileNameW(nullptr, module, 32768);
            const auto selected = path_utf8(connection);
            const bool address = selected.starts_with("http://") || selected.starts_with("https://");
            std::cout << Json({{"mcpServers",
                                {{"CyGameCapture",
                                  {{"command", path_utf8(module)},
                                   {"args",
                                    {"--mcp", address ? "--url" : "--instance",
                                     address ? selected : path_utf8(std::filesystem::absolute(connection))}}}}}}})
                             .dump()
                      << '\n';
            return 0;
        }
        if (full)
        {
            auto tools = FullTools(connection);
            if (command == "list")
                std::cout << Json({{"operations", tools->describe()}}).dump() << '\n';
            else if (command == "describe" && !name.empty())
                std::cout << tools->describe(name).dump() << '\n';
            else if (command == "call" && !name.empty())
                std::cout << tools->invoke(name, arguments).dump() << '\n';
            else if (command == "mcp")
            {
                McpServer server(*tools, "CyGameCapture full catalogue");
                server.serve_stdio(std::cin, std::cout);
            }
            else
                throw Error("InvalidArguments", "Unknown full-catalogue command");
            return 0;
        }
        auto client = ConnectInstance(connection);
        if (command == "list")
        {
            std::cout << Json({{"operations", client->list_tools()}}).dump() << '\n';
            return 0;
        }
        if (command == "describe" && !name.empty())
        {
            std::cout << client->get_tool(name).dump() << '\n';
            return 0;
        }
        if (command == "call" && !name.empty())
        {
            auto tools = InstanceTools(client);
            std::cout << tools->invoke(name, arguments).dump() << '\n';
            return 0;
        }
        if (command == "mcp")
        {
            auto tools = InstanceTools(client);
            McpServer server(*tools, "CyGameCapture connected instance");
            server.serve_stdio(std::cin, std::cout);
            return 0;
        }
        throw Error("InvalidArguments", "Expected list, describe NAME, call NAME, instances or providers; see --help");
    }
    catch (const Error &e)
    {
        auto error = Json({{"error", {{"code", e.code}, {"message", redact_error_message(e.what())}}}});
        std::cout << error.dump() << '\n';
        std::cerr << e.code << ": " << redact_error_message(e.what()) << '\n';
        return (e.code == "InvalidArguments" || e.code == "InvalidParameters" || e.code == "InvalidJson" ||
                e.code == "ValidationError")
                   ? 2
               : e.code == "PermissionDenied" || e.code == "AccessDenied" ||
                       std::string(e.what()).find("PermissionDenied") != std::string::npos
                   ? 3
                   : 1;
    }
    catch (const std::exception &e)
    {
        std::cout
            << Json({{"error", {{"code", "OperationFailed"}, {"message", redact_error_message(e.what())}}}}).dump()
            << '\n';
        std::cerr << redact_error_message(e.what()) << '\n';
        return 1;
    }
}
} // namespace cygc
