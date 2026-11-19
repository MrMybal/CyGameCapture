// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AiClients.hpp"
#include "CyGameCaptureAutomationClient.hpp"
#include <cymcp/context.hpp>
#include <fstream>
#include <sstream>
#include <thread>
using namespace cymcp;
namespace cygc
{
bool AskSdkClient(ClientKind kind, const std::vector<std::wstring> &command, const AskRequest &request,
                  std::string &answer, std::string &error)
{
    try
    {
        const std::filesystem::path folder = request.work_folder;
        private_directory(folder);
        ProviderRegistry providers;
        ProviderConfig config;
        std::string id = kind == ClientKind::ClaudeCode ? "claude-code"
                         : kind == ClientKind::Codex    ? "codex"
                         : kind == ClientKind::Http     ? "openai-compatible"
                                                        : "opencode";
        if (kind != ClientKind::Http)
        {
            if (command.empty())
                throw Error("ProviderUnavailable", "No client executable found");
            config.process.executable = command[0];
            for (size_t i = 1; i < command.size(); ++i)
                config.process.arguments.push_back(path_utf8(command[i]));
        }
        config.process.cwd = folder;
        config.approvals = Policy::deny_all();
        config.settings["allow_all_actions"] = false;
        if (!request.automation_connection.empty())
        {
            Json descriptor;
            auto remote = ConnectInstance(std::filesystem::u8path(request.automation_connection), &descriptor);
            if (kind == ClientKind::ClaudeCode)
            {
                config.settings["mcp_servers"] = {
                    {"CyGameCapture",
                     {{"type", "http"},
                      {"url", descriptor.at("url")},
                      {"headers", {{"Authorization", "Bearer " + descriptor.at("token").get<std::string>()}}}}}};
                config.settings["allowed_tools"] = {"mcp__CyGameCapture__*"};
            }
            else if (kind == ClientKind::OpenCode)
            {
                config.settings["mcp_servers"] = Json::array(
                    {{{"type", "http"},
                      {"name", "CyGameCapture"},
                      {"url", descriptor.at("url")},
                      {"headers", Json::array({{{"name", "Authorization"},
                                                {"value", "Bearer " + descriptor.at("token").get<std::string>()}}})}}});
            }
            else
            {
                auto tools = InstanceTools(remote);
                if (!request.external_tools_file.empty())
                    AddAllowedPeerTools(*tools, std::filesystem::u8path(request.external_tools_file),
                                        request.allow_peer_mutations,
                                        std::filesystem::u8path(request.automation_connection));
                config.tools = tools;
            }
        }
        if (!request.external_tools_file.empty() && (kind == ClientKind::ClaudeCode || kind == ClientKind::OpenCode))
            throw Error("Unsupported", "External per-tool allowlists currently require Codex or an API provider");
        if (kind == ClientKind::Http)
        {
            config.settings["base_url"] = request.endpoint;
            const char *key = request.key_env.empty() ? nullptr : std::getenv(request.key_env.c_str());
            if (key)
                config.api_key = key;
        }
        auto provider = providers.create(id, config);
        Conversation conversation(
            provider, request.model,
            "You are the CyGameCapture assistant. Use the supplied application tools for live state. Follow "
            "application permissions; never attempt to enable your own permissions. Preserve the user's capture and "
            "only apply changes requested by the user. Never replay a failed or cancelled mutation.");
        if (request.context_bytes && provider->uses_host_history())
        {
            ContextOptions context;
            context.max_bytes = request.context_bytes;
            conversation.configure_context(context);
        }
        const auto checkpoint = folder / L"sessions" / L"current.json";
        CallContext context;
        context.cancellation.deadline = Clock::now() + std::chrono::milliseconds(request.timeout_ms);
        std::string partial;
        context.events = [&](const Event &event) {
            if (event.type == "TextDelta")
            {
                partial += event.data.value("text", std::string());
                const auto path = folder / std::filesystem::u8path("partial-" + request.run_id + ".txt");
                auto output = open_private_output_file(path);
                fwrite(partial.data(), 1, partial.size(), output.get());
            }
        };
        std::jthread watcher([&](std::stop_token stop) {
            HANDLE game = request.game_pid ? OpenProcess(SYNCHRONIZE, FALSE, request.game_pid) : nullptr;
            while (!stop.stop_requested())
            {
                std::ifstream cancelled(folder / L"cancel.txt");
                std::string value;
                cancelled >> value;
                if (value == request.run_id || (game && WaitForSingleObject(game, 0) == WAIT_OBJECT_0))
                {
                    context.cancellation.cancel();
                    conversation.cancel();
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            if (game)
                CloseHandle(game);
        });
        if (std::filesystem::exists(checkpoint))
        {
            auto saved = ConversationStore(folder / L"sessions").load("current");
            conversation.restore(saved, context);
            if (!request.model.empty() && request.model != saved.value("model", std::string()))
                conversation.select_model(request.model);
        }
        else
            conversation.connect({}, context, kind == ClientKind::Http);
        auto content = Json::array({text_block(request.prompt)});
        for (const auto &image : request.images)
            content.push_back(attachment_from_file(image));
        Executor executor(1);
        auto future = conversation.send_async(executor, content, context);
        HANDLE game = request.game_pid ? OpenProcess(SYNCHRONIZE, FALSE, request.game_pid) : nullptr;
        while (future.wait_for(std::chrono::milliseconds(50)) != std::future_status::ready)
        {
            std::ifstream cancelled(folder / L"cancel.txt");
            std::string value;
            cancelled >> value;
            if (value == request.run_id || (game && WaitForSingleObject(game, 0) == WAIT_OBJECT_0))
                conversation.cancel();
        }
        if (game)
            CloseHandle(game);
        try
        {
            answer = future.get().text;
        }
        catch (...)
        {
            ConversationStore(folder / L"sessions").save("current", conversation);
            throw;
        }
        ConversationStore(folder / L"sessions").save("current", conversation);
        return true;
    }
    catch (const Error &failure)
    {
        error = failure.code + ": " + redact_error_message(failure.what());
        return false;
    }
    catch (const std::exception &failure)
    {
        error = redact_error_message(failure.what());
        return false;
    }
}
} // namespace cygc
