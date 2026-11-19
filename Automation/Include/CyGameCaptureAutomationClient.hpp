// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: MIT
#pragma once
#include <cymcp/ai.hpp>
#include <cymcp/mcp.hpp>
namespace cygc
{
std::shared_ptr<cymcp::McpClient> ConnectInstance(const std::filesystem::path &, cymcp::Json *descriptor = nullptr);
std::shared_ptr<cymcp::Registry> InstanceTools(std::shared_ptr<cymcp::McpClient>);
void AddAllowedPeerTools(cymcp::Registry &, const std::filesystem::path &, bool allow_mutations,
                         const std::filesystem::path &controlling_instance = {});
int AutomationMain(int argc, wchar_t **argv); // returns -1 for the original request.json entry point
} // namespace cygc
