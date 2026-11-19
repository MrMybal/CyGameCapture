// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureAI — finding and running the AI command line clients installed on this PC.
//
// Provider discovery, streaming, sessions and tools are owned by the source-built CyMCPSDK.
// The existing public helper entry points remain compatible.
#pragma once

#include <Windows.h>
#include <string>
#include <vector>

namespace cygc
{
	enum class ClientKind { ClaudeCode, Codex, OpenCode, Http };

	/**
	 * The messages the user reads (progress and errors, shown in the overlay's discussion) follow the
	 * language of the request; everything else, the prompt included, stays in English.
	 */
	void SetFrenchMessages(bool french);
	const char *Msg(const char *english, const char *french);

	const char *ClientName(ClientKind kind);

	/** The command that starts the client: its .exe, or node + its script. Empty when not installed. */
	std::vector<std::wstring> FindClient(ClientKind kind, const std::wstring &override_path);

	struct AskRequest
	{
		std::string prompt;                      // UTF-8
		std::vector<std::wstring> images;        // PNG files, in the order the prompt describes them
		std::string model;                       // empty: the client's default
		std::wstring work_folder;
		DWORD game_pid = 0;                      // the question is dropped if the game closes
		DWORD timeout_ms = 5 * 60 * 1000;
        std::string automation_connection, run_id;
        std::string endpoint = "http://127.0.0.1:1234/v1", key_env;
        size_t context_bytes = 0;
        std::string external_tools_file;
        bool allow_peer_mutations = false;
	};

	/** Runs the client and returns its answer text. */
	bool AskClient(ClientKind kind, const std::vector<std::wstring> &command, const AskRequest &request,
		std::string &answer, std::string &error);
    bool AskSdkClient(ClientKind, const std::vector<std::wstring>&, const AskRequest&, std::string&, std::string&);
}
