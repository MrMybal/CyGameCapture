// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureAI — finding and running the AI command line clients installed on this PC.
//
// Same method as CyAICodex (methodes/local_clients.py and assist.py), which already talks to these
// clients: look for the executable where each installer puts it, never through a shell, and run it in its
// non-interactive mode with no tools, no user configuration and no saved session. The client answers with
// the user's own account; nothing here stores or sees a key.
#pragma once

#include <Windows.h>
#include <string>
#include <vector>

namespace cygc
{
	enum class ClientKind { ClaudeCode, Codex, OpenCode };

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
	};

	/** Runs the client and returns its answer text. */
	bool AskClient(ClientKind kind, const std::vector<std::wstring> &command, const AskRequest &request,
		std::string &answer, std::string &error);
}
