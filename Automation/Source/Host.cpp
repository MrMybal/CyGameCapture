// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: MIT
#include "CyGameCaptureAutomation.h"
#include "OutputSchemas.hpp"
#include <Windows.h>
#include <cymcp/ai.hpp>
#include <cymcp/changes.hpp>
#include <cymcp/mcp.hpp>
#include <cymcp/performance.hpp>
#include <fstream>
#include <set>
#include <sstream>
using namespace cymcp;
namespace
{
thread_local std::string last_error;
struct Task
{
    std::string name;
    Json args;
    CallContext context;
    std::promise<Json> result;
    cygc_operation_handler handler;
    void *owner;
};
struct Host
{
    std::atomic_bool stopping{false}, writable{false};
    std::mutex queue_mutex;
    std::deque<std::shared_ptr<Task>> queue;
    std::set<std::string> reads;
    std::mutex reads_mutex;
    bool IsRead(const std::string &name)
    {
        std::lock_guard lock(reads_mutex);
        return reads.count(name) != 0;
    }
    std::shared_ptr<Registry> registry;
    std::shared_ptr<ResourceRegistry> resources;
    std::shared_ptr<ChangeFeed> changes;
    std::shared_ptr<PerformanceMonitor> performance;
    std::unique_ptr<ToolGateway> gateway;
    std::unique_ptr<McpServer> mcp;
    std::unique_ptr<LocalHttpServer> http;
    std::filesystem::path connection;
    std::string connection_utf8;
    std::filesystem::path chat_directory;
    cygc_operation_handler status_handler = nullptr;
    void *status_owner = nullptr;
    Clock::time_point next_status{};
    std::string component;
    void TextResource(const std::string &uri, const std::string &name, std::string text)
    {
        auto snapshot = std::make_shared<std::string>(std::move(text));
        Resource resource;
        resource.uri = uri;
        resource.name = name;
        resource.description = name;
        resource.size = snapshot->size();
        resource.cache_ttl = std::chrono::milliseconds(1000);
        resource.read = [snapshot](size_t offset, size_t length, const CallContext &ctx) {
            ctx.cancellation.check();
            return snapshot->substr(std::min(offset, snapshot->size()), length);
        };
        resources->replace(std::move(resource));
    }
    std::mutex chat_mutex;
    std::shared_ptr<Conversation> chat;
    Cancellation chat_cancellation;
    std::future<void> chat_work;
    std::atomic_bool chat_busy{false};
    EventQueue chat_events;
    std::string event_json, chat_profile;
    void CancelChat()
    {
        std::lock_guard lock(chat_mutex);
        chat_cancellation.cancel();
        if (chat)
            chat->cancel();
    }
    void Chat(const Json &request)
    {
        auto profile = request;
        profile.erase("text");
        profile.erase("attachments");
        profile.erase("context_bytes"); // A context budget can change without discarding this session.
        {
            std::lock_guard lock(chat_mutex);
            if (chat && chat_profile != profile.dump())
                throw Error("ConversationConfigurationChanged",
                            "Start a new conversation before changing its provider or model settings");
            chat_profile = profile.dump();
        }
        if (chat_busy.exchange(true))
            throw Error("Busy", "A conversation turn is already running");
        {
            std::lock_guard lock(chat_mutex);
            chat_cancellation = Cancellation();
        }
        chat_work = std::async(std::launch::async, [this, request] {
            std::shared_ptr<Conversation> active;
            try
            {
                ProviderRegistry providers;
                ProviderConfig config;
                const auto id = request.value("provider", std::string("codex"));
                if (request.contains("executable"))
                    config.process.executable = std::filesystem::u8path(request.at("executable").get<std::string>());
                const auto model = request.value("model", std::string());
                if (id == "openai-compatible")
                    config.settings["base_url"] = request.value("endpoint", std::string("http://127.0.0.1:1234/v1"));
                if (request.contains("key_env"))
                {
                    const char *key = std::getenv(request.at("key_env").get<std::string>().c_str());
                    if (key)
                        config.api_key = key;
                }
                config.tools = registry;
                config.approvals = Policy::deny_all();
                if (id == "claude-code" || id == "opencode")
                    throw Error("Unsupported", "Use Codex or an OpenAI-compatible endpoint in this native panel; the "
                                               "ReShade companion additionally supports Claude and OpenCode over MCP");
                auto provider = providers.create(id, config);
                {
                    std::lock_guard lock(chat_mutex);
                    if (!chat)
                        chat = std::make_shared<Conversation>(
                            provider, model,
                            "You are the CyGameCapture assistant. Use application tools for live state and follow its "
                            "permissions. Never enable your own permissions or replay a failed mutation.");
                    active = chat;
                }
                CallContext ctx;
                ctx.cancellation = chat_cancellation;
                ctx.events = [this](const Event &event) { chat_events.push(event); };
                if (active->status().value("state", std::string()) == "disconnected")
                {
                    ContextOptions context;
                    context.max_bytes = request.value("context_bytes", 0U);
                    if (provider->uses_host_history())
                        active->configure_context(context);
                    ConversationStore store(chat_directory);
                    if (std::filesystem::exists(chat_directory / L"current.json"))
                        active->restore(store.load("current"), ctx);
                    else
                        active->connect({}, ctx, provider->local_service());
                }
                auto content = Json::array({text_block(request.at("text"))});
                if (provider->uses_host_history())
                {
                    ContextOptions context;
                    context.max_bytes = request.value("context_bytes", 0U);
                    active->configure_context(context);
                }
                for (const auto &file : request.value("attachments", Json::array()))
                    content.push_back(attachment_from_file(std::filesystem::u8path(file.get<std::string>())));
                auto result = active->send(content, ctx);
                ConversationStore(chat_directory).save("current", *active);
                {
                    auto profile = request;
                    profile.erase("text");
                    profile.erase("attachments");
                    const auto bytes = profile.dump();
                    auto output = open_private_output_file(chat_directory / L"profile.json");
                    fwrite(bytes.data(), 1, bytes.size(), output.get());
                }
                chat_events.push({"HostReply", {{"text", result.text}}});
            }
            catch (const Error &error)
            {
                if (active)
                    try
                    {
                        ConversationStore(chat_directory).save("current", *active);
                    }
                    catch (...)
                    {
                    }
                chat_events.push(
                    {"HostError", {{"code", error.code}, {"message", redact_error_message(error.what())}}});
            }
            catch (const std::exception &error)
            {
                chat_events.push(
                    {"HostError", {{"code", "OperationFailed"}, {"message", redact_error_message(error.what())}}});
            }
            chat_busy = false;
        });
    }
    explicit Host(const Json &options)
    {
        wchar_t *local = nullptr;
        size_t length = 0;
        _wdupenv_s(&local, &length, L"LOCALAPPDATA");
        const auto directory = options.contains("directory")
                                   ? std::filesystem::u8path(options.at("directory").get<std::string>())
                                   : std::filesystem::path(local ? local : L".") / L"CyGameCapture" / L"Automation";
        free(local);
        private_directory(directory);
        component = options.value("component", std::string("native"));
        writable = options.value("allow_mutations", false);
        registry =
            std::make_shared<Registry>(Policy([this](const CallContext &, const std::string &name, const Json &) {
                return !stopping && (IsRead(name) || writable) ? Decision::allow : Decision::deny;
            }));
        ResourceOptions storage;
        storage.max_stored_bytes = options.value("result_memory_bytes", 8 * 1024 * 1024);
        storage.result_ttl = std::chrono::milliseconds(options.value("result_ttl_ms", 300000));
        storage.chunk_bytes = 16384;
        resources = std::make_shared<ResourceRegistry>(Policy::allow_all(), storage);
        if (options.value("notifications", true))
        {
            changes = std::make_shared<ChangeFeed>();
            registry->set_change_feed(changes);
            resources->set_change_feed(changes);
        }
        if (options.value("performance", false))
        {
            PerformanceOptions p;
            p.jsonl_file = directory /
                           ("performance-" + std::to_string(GetCurrentProcessId()) + "-" + random_token(16) + ".jsonl");
            performance = std::make_shared<PerformanceMonitor>(p);
            registry->set_performance_monitor(performance);
        }
        McpServerOptions server_options;
        server_options.resources = resources;
        server_options.changes = changes;
        server_options.performance = performance;
        server_options.cache_ttl = std::chrono::milliseconds(options.value("cache_ttl_ms", 1000));
        // Both access modes use the same authoritative registry and policy; compact mode wraps it.
        std::shared_ptr<const Registry> exposed = registry;
        if (options.value("compact", false))
        {
            ToolGatewayOptions g;
            g.inline_bytes = 16384;
            gateway = std::make_unique<ToolGateway>(registry, g, resources);
            exposed = gateway->tools();
        }
        mcp = std::make_unique<McpServer>(
            *exposed, "CyGameCapture " + options.value("component", std::string("native")), server_options);
        const auto token = random_token();
        http = std::make_unique<LocalHttpServer>(
            token,
            [this](const Json &input, const CallContext &ctx) {
                return input.contains("jsonrpc") ? mcp->handle(input, ctx) : dispatch_app(*registry, input, ctx);
            },
            // Reserve two dispatch workers beyond the two permitted long-lived subscriptions.
            4);
        const int port = http->start();
        chat_directory = directory / L"Conversations" /
                         std::filesystem::u8path(options.value("component", std::string("native")) + "-" +
                                                 sha256(options.value("scope", std::string("default"))).substr(0, 16));
        connection = directory / (options.value("component", std::string("native")) + "-" +
                                  std::to_string(GetCurrentProcessId()) + "-" + random_token(16) + ".json");
        Json descriptor = {{"version", 1},
                           {"sdk", version},
                           {"pid", GetCurrentProcessId()},
                           {"component", options.value("component", std::string("native"))},
                           {"url", "http://127.0.0.1:" + std::to_string(port) + "/mcp"},
                           {"token", token}};
        const auto bytes = descriptor.dump();
        auto file = open_private_output_file(connection);
        if (fwrite(bytes.data(), 1, bytes.size(), file.get()) != bytes.size())
            throw Error("FileError", "Cannot save instance connection");
        connection_utf8 = path_utf8(connection);
    }
    ~Host()
    {
        stopping = true;
        CancelChat();
        if (mcp)
            mcp->cancel_all();
        {
            std::lock_guard lock(queue_mutex);
            for (auto &task : queue)
                task->result.set_exception(
                    std::make_exception_ptr(Error("InstanceClosed", "Application instance closed")));
            queue.clear();
        }
        if (http)
            http->stop();
        if (chat_work.valid())
            chat_work.wait();
        std::error_code ignored;
        std::filesystem::remove(connection, ignored);
    }
    void add(const Json &spec, cygc_operation_handler handler, void *owner)
    {
        Operation op;
        op.name = spec.at("name");
        op.description = spec.at("description");
        op.input_schema = spec.at("inputSchema");
        op.output_schema = spec.value("outputSchema", OutputSchema(component, op.name));
        op.read_only = spec.value("readOnly", false);
        if (op.read_only)
        {
            std::lock_guard lock(reads_mutex);
            reads.insert(op.name);
        }
        op.handler = [this, name = op.name, handler, owner](const Json &args, const CallContext &caller) {
            auto task = std::make_shared<Task>();
            task->name = name;
            task->args = args;
            task->context = caller;
            task->context.cancellation = caller.cancellation.child();
            task->handler = handler;
            task->owner = owner;
            auto result = task->result.get_future();
            {
                std::lock_guard lock(queue_mutex);
                if (stopping)
                    throw Error("InstanceClosed", "Application closed");
                if (queue.size() >= 64)
                    throw Error("Busy", "Application command queue is full");
                queue.push_back(task);
            }
            const auto deadline = Clock::now() + std::chrono::seconds(15);
            try
            {
                while (result.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready)
                {
                    task->context.cancellation.check();
                    if (Clock::now() > deadline)
                        throw Error(
                            "InstanceNotPumping",
                            "Application did not process the command in 15 seconds; the action will not be replayed");
                }
                return result.get();
            }
            catch (...)
            {
                task->context.cancellation.cancel();
                throw;
            }
        };
        registry->add(std::move(op));
        TextResource("cygc://operations", "Available application operations", registry->describe().dump(2));
        if (spec.at("name") == "app.status" || spec.at("name") == "source.status")
        {
            status_handler = handler;
            status_owner = owner;
        }
    }
    void pump()
    {
        for (int i = 0; i < 16; i++)
        {
            std::shared_ptr<Task> task;
            {
                std::lock_guard lock(queue_mutex);
                if (queue.empty())
                    break;
                task = queue.front();
                queue.pop_front();
            }
            try
            {
                task->context.cancellation.check();
                if (!IsRead(task->name) && !writable)
                    throw Error("PermissionDenied", "Remote mutations are disabled in this application instance");
                const auto args = task->args.dump();
                const char *reply = task->handler(task->owner, task->name.c_str(), args.c_str());
                auto result = parse_json(reply ? reply : "{}");
                if (result.contains("error") && result["error"].is_object() && result["error"].contains("code"))
                    throw Error(result["error"].value("code", "OperationFailed"),
                                result["error"].value("message", "Operation failed"));
                task->result.set_value(std::move(result));
                if (changes && !IsRead(task->name))
                    changes->publish("resources");
            }
            catch (...)
            {
                task->result.set_exception(std::current_exception());
            }
        }
        if (status_handler && Clock::now() >= next_status && !stopping)
        {
            next_status = Clock::now() + std::chrono::seconds(1);
            try
            {
                const auto status =
                    status_handler(status_owner, component == "obs-source" ? "source.status" : "app.status", "{}");
                TextResource("cygc://state", "Live application state", status ? status : "{}");
            }
            catch (...)
            {
            }
        }
    }
};
} // namespace
extern "C"
{
    void *cygc_automation_create(const char *options)
    {
        try
        {
            return new Host(parse_json(options));
        }
        catch (const std::exception &error)
        {
            last_error = redact_error_message(error.what());
            return nullptr;
        }
    }
    int cygc_automation_add(void *host, const char *spec, cygc_operation_handler callback, void *owner)
    {
        try
        {
            static_cast<Host *>(host)->add(parse_json(spec), callback, owner);
            return 1;
        }
        catch (const std::exception &error)
        {
            last_error = redact_error_message(error.what());
            return 0;
        }
    }
    const char *cygc_automation_connection(void *host)
    {
        return host ? static_cast<Host *>(host)->connection_utf8.c_str() : "";
    }
    const char *cygc_automation_error()
    {
        return last_error.c_str();
    }
    void cygc_automation_pump(void *host)
    {
        if (host)
            static_cast<Host *>(host)->pump();
    }
    void cygc_automation_allow_mutations(void *host, int allow)
    {
        if (host)
            static_cast<Host *>(host)->writable = allow != 0;
    }
    void cygc_automation_destroy(void *host)
    {
        delete static_cast<Host *>(host);
    }
    int cygc_automation_chat_send(void *host, const char *request)
    {
        try
        {
            static_cast<Host *>(host)->Chat(parse_json(request));
            return 1;
        }
        catch (const std::exception &error)
        {
            last_error = redact_error_message(error.what());
            return 0;
        }
    }
    const char *cygc_automation_chat_events(void *host)
    {
        auto &h = *static_cast<Host *>(host);
        auto events = Json::array();
        for (const auto &event : h.chat_events.drain())
            events.push_back({{"type", event.type}, {"data", event.data}});
        h.event_json = Json({{"events", events}, {"busy", h.chat_busy.load()}}).dump();
        return h.event_json.c_str();
    }
    void cygc_automation_chat_cancel(void *host)
    {
        if (host)
            static_cast<Host *>(host)->CancelChat();
    }
    const char *cygc_automation_chat_history(void *host)
    {
        auto &h = *static_cast<Host *>(host);
        Json messages = Json::array();
        try
        {
            std::shared_ptr<Conversation> chat;
            {
                std::lock_guard lock(h.chat_mutex);
                chat = h.chat;
            }
            if (chat)
                messages = chat->history();
            else if (std::filesystem::exists(h.chat_directory / L"current.json"))
                messages = ConversationStore(h.chat_directory).load("current").value("messages", Json::array());
        }
        catch (...)
        {
        }
        Json settings = Json::object();
        try
        {
            std::ifstream saved(h.chat_directory / L"profile.json");
            if (saved)
                saved >> settings;
        }
        catch (...)
        {
        }
        Json display = Json::array();
        for (const auto &item : messages)
        {
            auto role = item.value("role", std::string());
            if (role != "user" && role != "assistant")
                continue;
            std::string text;
            const auto content = item.value("content", Json());
            if (content.is_string())
                text = content;
            else if (content.is_array())
                for (const auto &block : content)
                    if (block.value("type", std::string()) == "text")
                        text += block.value("text", std::string());
            if (!text.empty())
                display.push_back({{"role", role}, {"text", text}});
        }
        h.event_json = Json({{"messages", display}, {"settings", settings}}).dump();
        return h.event_json.c_str();
    }
    void cygc_automation_chat_clear(void *host)
    {
        if (!host)
            return;
        auto &h = *static_cast<Host *>(host);
        if (h.chat_busy)
            return;
        {
            std::lock_guard lock(h.chat_mutex);
            h.chat.reset();
        }
        std::error_code ignored;
        const auto path = h.chat_directory / L"current.json";
        if (std::filesystem::exists(path))
            std::filesystem::rename(
                path, h.chat_directory / (L"archived-" + std::to_wstring(GetTickCount64()) + L".json"), ignored);
    }
}
