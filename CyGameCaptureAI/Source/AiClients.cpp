// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AiClients.hpp"
#include <cymcp/ai.hpp>
namespace cygc
{
namespace
{
bool french_messages = false;
}
void SetFrenchMessages(bool french)
{
    french_messages = french;
}
const char *Msg(const char *english, const char *french)
{
    return french_messages ? french : english;
}
const char *ClientName(ClientKind kind)
{
    switch (kind)
    {
    case ClientKind::ClaudeCode:
        return "Claude Code";
    case ClientKind::Codex:
        return "Codex";
    case ClientKind::OpenCode:
        return "OpenCode";
    case ClientKind::Http:
        return "Local / API";
    }
    return "AI";
}
std::vector<std::wstring> FindClient(ClientKind kind, const std::wstring &override_path)
{
    if (kind == ClientKind::Http)
        return {L"http"};
    try
    {
        cymcp::ProviderRegistry registry;
        const std::string id = kind == ClientKind::ClaudeCode ? "claude-code"
                               : kind == ClientKind::Codex    ? "codex"
                                                              : "opencode";
        cymcp::ProviderDiscoveryHints hints;
        for (const auto &item : registry.describe())
            if (item.at("id") == id)
            { /* resolved by the built-in factory below */
            }
        hints.executables = {kind == ClientKind::ClaudeCode ? "claude"
                             : kind == ClientKind::Codex    ? "codex"
                                                            : "opencode"};
        hints.npm_packages = {kind == ClientKind::ClaudeCode ? "@anthropic-ai/claude-code"
                              : kind == ClientKind::Codex    ? "@openai/codex"
                                                             : "opencode-ai"};
        cymcp::ProcessOptions process;
        process.executable = override_path;
        process = cymcp::resolve_provider_process(hints, process);
        std::vector<std::wstring> command = {process.executable.wstring()};
        for (const auto &argument : process.arguments)
            command.push_back(std::filesystem::u8path(argument).wstring());
        return command;
    }
    catch (...)
    {
        return {};
    }
}
bool AskClient(ClientKind kind, const std::vector<std::wstring> &command, const AskRequest &request,
               std::string &answer, std::string &error)
{
    return AskSdkClient(kind, command, request, answer, error);
}
} // namespace cygc
