// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "AiClients.hpp"
#include "MiniJson.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

namespace cygc
{
	namespace
	{
		bool g_french = false;

		std::wstring Env(const wchar_t *name)
		{
			wchar_t value[MAX_PATH * 2] = {};
			const DWORD length = GetEnvironmentVariableW(name, value, static_cast<DWORD>(std::size(value)));
			return length > 0 && length < std::size(value) ? std::wstring(value, length) : std::wstring();
		}

		bool IsFile(const fs::path &path)
		{
			std::error_code ec;
			return !path.empty() && fs::is_regular_file(path, ec);
		}

		fs::path FindOnPath(const wchar_t *file)
		{
			wchar_t found[MAX_PATH] = {};
			return SearchPathW(nullptr, file, nullptr, MAX_PATH, found, nullptr) > 0 ? fs::path(found) : fs::path();
		}

		/** One argument quoted the way CommandLineToArgvW and the C runtime read it back. */
		void AppendArgument(std::wstring &line, const std::wstring &arg)
		{
			if (!line.empty())
				line += L' ';
			if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
			{
				line += arg;
				return;
			}
			line += L'"';
			for (auto it = arg.begin();; ++it)
			{
				size_t backslashes = 0;
				while (it != arg.end() && *it == L'\\')
				{
					++it;
					++backslashes;
				}
				if (it == arg.end())
				{
					line.append(backslashes * 2, L'\\');
					break;
				}
				if (*it == L'"')
				{
					line.append(backslashes * 2 + 1, L'\\');
					line += L'"';
				}
				else
				{
					line.append(backslashes, L'\\');
					line += *it;
				}
			}
			line += L'"';
		}

		std::wstring Widen(const std::string &text)
		{
			if (text.empty())
				return {};
			const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
			std::wstring wide(static_cast<size_t>(length), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
			return wide;
		}

		std::string Base64(const std::vector<uint8_t> &data)
		{
			static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			std::string out;
			out.reserve((data.size() + 2) / 3 * 4);
			size_t i = 0;
			for (; i + 2 < data.size(); i += 3)
			{
				const uint32_t v = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
				out += table[(v >> 18) & 63]; out += table[(v >> 12) & 63]; out += table[(v >> 6) & 63]; out += table[v & 63];
			}
			if (i + 1 == data.size())
			{
				const uint32_t v = data[i] << 16;
				out += table[(v >> 18) & 63]; out += table[(v >> 12) & 63]; out += "==";
			}
			else if (i + 2 == data.size())
			{
				const uint32_t v = (data[i] << 16) | (data[i + 1] << 8);
				out += table[(v >> 18) & 63]; out += table[(v >> 12) & 63]; out += table[(v >> 6) & 63]; out += '=';
			}
			return out;
		}

		std::vector<uint8_t> ReadBytes(const fs::path &path)
		{
			std::ifstream file(path, std::ios::binary);
			return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		}

		/**
		 * Runs a process with no window, feeding `input` to its stdin and collecting its stdout.
		 *
		 * It lives in a job object, so a timeout, or the game closing, ends it together with anything it
		 * started (node, a language server...). Its stderr is dropped on purpose: clients may echo the
		 * prompt or account details there, and nothing of it is shown to the user.
		 */
		bool RunProcess(const std::vector<std::wstring> &args, const std::string &input, const std::wstring &cwd,
			DWORD game_pid, DWORD timeout_ms, std::string &output, DWORD &exit_code, std::string &error)
		{
			std::wstring line;
			for (const std::wstring &arg : args)
				AppendArgument(line, arg);

			SECURITY_ATTRIBUTES inherit = { sizeof(inherit), nullptr, TRUE };
			HANDLE stdin_read = nullptr, stdin_write = nullptr, stdout_read = nullptr, stdout_write = nullptr;
			if (!CreatePipe(&stdin_read, &stdin_write, &inherit, 0) || !CreatePipe(&stdout_read, &stdout_write, &inherit, 0))
			{
				error = Msg("Could not create the pipes to the AI client", "Impossible de créer les tubes vers le client IA");
				return false;
			}
			SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);
			SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
			HANDLE null_output = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);

			STARTUPINFOW startup = { sizeof(startup) };
			startup.dwFlags = STARTF_USESTDHANDLES;
			startup.hStdInput = stdin_read;
			startup.hStdOutput = stdout_write;
			startup.hStdError = null_output;
			PROCESS_INFORMATION process = {};
			const BOOL started = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE,
				CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, cwd.empty() ? nullptr : cwd.c_str(), &startup, &process);
			CloseHandle(stdin_read);
			CloseHandle(stdout_write);
			CloseHandle(null_output);
			if (!started)
			{
				CloseHandle(stdin_write);
				CloseHandle(stdout_read);
				error = std::string(Msg("Could not start the AI client (error ", "Impossible de démarrer le client IA (erreur ")) + std::to_string(GetLastError()) + ")";
				return false;
			}

			HANDLE job = CreateJobObjectW(nullptr, nullptr);
			JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
			limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
			SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
			AssignProcessToJobObject(job, process.hProcess);
			ResumeThread(process.hThread);
			CloseHandle(process.hThread);

			std::thread writer([&]() {
				DWORD written = 0;
				size_t offset = 0;
				while (offset < input.size() &&
					WriteFile(stdin_write, input.data() + offset, static_cast<DWORD>(std::min<size_t>(input.size() - offset, 1 << 20)), &written, nullptr) && written > 0)
					offset += written;
				CloseHandle(stdin_write);
			});
			std::thread reader([&]() {
				char buffer[65536];
				DWORD read = 0;
				while (ReadFile(stdout_read, buffer, sizeof(buffer), &read, nullptr) && read > 0)
					if (output.size() < (32u << 20))
						output.append(buffer, read);
			});

			HANDLE game = game_pid != 0 ? OpenProcess(SYNCHRONIZE, FALSE, game_pid) : nullptr;
			HANDLE waits[2] = { process.hProcess, game };
			const DWORD result = WaitForMultipleObjects(game != nullptr ? 2 : 1, waits, FALSE, timeout_ms);
			bool ok = true;
			if (result == WAIT_OBJECT_0 + 1)
			{
				error = Msg("The game was closed before the answer came", "Le jeu a été fermé avant la réponse");
				ok = false;
			}
			else if (result == WAIT_TIMEOUT)
			{
				error = Msg("The AI client did not answer in time", "Le client IA n'a pas répondu à temps");
				ok = false;
			}
			// Whatever it started is not needed any more either; this also ends the pipes
			TerminateJobObject(job, 1);
			WaitForSingleObject(process.hProcess, 5000);
			GetExitCodeProcess(process.hProcess, &exit_code);
			writer.join();
			reader.join();
			CloseHandle(stdout_read);
			CloseHandle(process.hProcess);
			CloseHandle(job);
			if (game != nullptr)
				CloseHandle(game);
			return ok;
		}
	}

	void SetFrenchMessages(bool french)
	{
		g_french = french;
	}

	const char *Msg(const char *english, const char *french)
	{
		return g_french ? french : english;
	}

	const char *ClientName(ClientKind kind)
	{
		switch (kind)
		{
		case ClientKind::ClaudeCode: return "Claude Code";
		case ClientKind::Codex: return "Codex";
		default: return "OpenCode";
		}
	}

	std::vector<std::wstring> FindClient(ClientKind kind, const std::wstring &override_path)
	{
		if (!override_path.empty())
			return IsFile(override_path) && fs::path(override_path).extension() == L".exe" ? std::vector<std::wstring> { override_path } : std::vector<std::wstring> {};

		const std::wstring name = kind == ClientKind::ClaudeCode ? L"claude" : kind == ClientKind::Codex ? L"codex" : L"opencode";
		const fs::path home = Env(L"USERPROFILE");
		const fs::path local = Env(L"LOCALAPPDATA");
		const fs::path roaming = Env(L"APPDATA");

		std::vector<fs::path> candidates;
		if (const fs::path on_path = FindOnPath((name + L".exe").c_str()); !on_path.empty())
			candidates.push_back(on_path);
		if (kind == ClientKind::Codex)
		{
			// The Codex desktop installer keeps versioned copies; the most recent one wins
			std::vector<fs::path> versions;
			std::error_code ec;
			for (const auto &entry : fs::directory_iterator(local / L"OpenAI" / L"Codex" / L"bin", ec))
				if (IsFile(entry.path() / L"codex.exe"))
					versions.push_back(entry.path() / L"codex.exe");
			std::sort(versions.begin(), versions.end(), [](const fs::path &a, const fs::path &b) {
				std::error_code e;
				return fs::last_write_time(a, e) > fs::last_write_time(b, e);
			});
			candidates.insert(candidates.end(), versions.begin(), versions.end());
		}
		candidates.push_back(home / L".local" / L"bin" / (name + L".exe"));
		candidates.push_back(home / (L"." + name) / L"bin" / (name + L".exe"));
		candidates.push_back(local / L"Programs" / name / (name + L".exe"));
		candidates.push_back(roaming / L"npm" / (name + L".exe"));
		// Recent npm packages ship a native executable inside the package, behind their .cmd wrapper
		if (kind == ClientKind::ClaudeCode)
			candidates.push_back(roaming / L"npm" / L"node_modules" / L"@anthropic-ai" / L"claude-code" / L"bin" / L"claude.exe");
		else if (kind == ClientKind::OpenCode)
			candidates.push_back(roaming / L"npm" / L"node_modules" / L"opencode-ai" / L"bin" / L"opencode.exe");
		for (const fs::path &path : candidates)
			if (IsFile(path))
				return { path.wstring() };

		// npm installs a .cmd wrapper; its script is run through node directly, never through a shell
		fs::path node = FindOnPath(L"node.exe");
		if (node.empty() && IsFile(fs::path(Env(L"ProgramFiles")) / L"nodejs" / L"node.exe"))
			node = fs::path(Env(L"ProgramFiles")) / L"nodejs" / L"node.exe";
		const fs::path modules = roaming / L"npm" / L"node_modules";
		const fs::path script = kind == ClientKind::ClaudeCode ? modules / L"@anthropic-ai" / L"claude-code" / L"cli.js"
			: kind == ClientKind::OpenCode ? modules / L"opencode-ai" / L"bin" / L"opencode"
			: modules / L"@openai" / L"codex" / L"bin" / L"codex.js";
		if (!node.empty() && IsFile(script))
			return { node.wstring(), script.wstring() };
		return {};
	}

	bool AskClient(ClientKind kind, const std::vector<std::wstring> &command, const AskRequest &request,
		std::string &answer, std::string &error)
	{
		std::vector<std::wstring> args = command;
		std::string input;
		const fs::path answer_file = fs::path(request.work_folder) / L"codex-answer.txt";

		switch (kind)
		{
		case ClientKind::ClaudeCode:
		{
			// Print mode, no tools, no MCP server, no user settings, no saved session: it only reads
			// (stream-json in requires stream-json out, and that requires --verbose: same as CyAICodex's connectors)
			args.insert(args.end(), { L"-p", L"--output-format", L"stream-json", L"--input-format", L"stream-json", L"--verbose",
				L"--tools", L"", L"--strict-mcp-config", L"--mcp-config", L"{\"mcpServers\":{}}",
				L"--setting-sources", L"", L"--no-session-persistence" });
			if (!request.model.empty())
				args.insert(args.end(), { L"--model", Widen(request.model) });
			// The pictures travel in the message itself, as the API's image blocks
			std::string content = "[{\"type\":\"text\",\"text\":";
			json::AppendString(content, request.prompt);
			content += "}";
			for (const std::wstring &image : request.images)
			{
				content += ",{\"type\":\"image\",\"source\":{\"type\":\"base64\",\"media_type\":\"image/png\",\"data\":\"";
				content += Base64(ReadBytes(image));
				content += "\"}}";
			}
			content += "]";
			input = "{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":" + content + "}}\n";
			break;
		}
		case ClientKind::Codex:
		{
			args.insert(args.end(), { L"exec", L"--ignore-user-config", L"--skip-git-repo-check", L"--ephemeral",
				L"--sandbox", L"read-only", L"--color", L"never", L"-C", request.work_folder, L"-o", answer_file.wstring() });
			if (!request.model.empty())
				args.insert(args.end(), { L"--model", Widen(request.model) });
			for (const std::wstring &image : request.images)
				args.insert(args.end(), { L"--image", image });
			args.push_back(L"-");   // the prompt comes on stdin
			input = request.prompt;
			std::error_code ec;
			fs::remove(answer_file, ec);
			break;
		}
		case ClientKind::OpenCode:
			args.insert(args.end(), { L"run", L"--pure", L"--format", L"json" });
			if (!request.model.empty())
				args.insert(args.end(), { L"--model", Widen(request.model) });
			for (const std::wstring &image : request.images)
				args.insert(args.end(), { L"--file", image });
			// Every permission denied (no tool can run) and no sharing, for this run only
			SetEnvironmentVariableW(L"OPENCODE_CONFIG_CONTENT", L"{\"permission\":{\"*\":\"deny\"},\"share\":\"disabled\"}");
			SetEnvironmentVariableW(L"OPENCODE_AUTO_SHARE", L"false");
			input = request.prompt;
			break;
		}

		std::string output;
		DWORD exit_code = 0;
		if (!RunProcess(args, input, request.work_folder, request.game_pid, request.timeout_ms, output, exit_code, error))
			return false;
		if (exit_code != 0)
		{
			error = std::string(ClientName(kind)) + Msg(" failed (code ", " a échoué (code ") + std::to_string(exit_code) +
				Msg("). Check that it is signed in, and its version.", "). Vérifiez qu'il est connecté, et sa version.");
			return false;
		}

		switch (kind)
		{
		case ClientKind::ClaudeCode:
		{
			// One JSON event per line; only a final "result" event that says success counts as an answer
			size_t start = 0;
			bool answered = false;
			while (start < output.size())
			{
				size_t end = output.find('\n', start);
				if (end == std::string::npos)
					end = output.size();
				json::Value event;
				if (json::Parse(output.substr(start, end - start), event) && event.String("type") == "result")
				{
					if (event.String("subtype") != "success" || event.Bool("is_error") || event.String("result").empty())
						break;
					answer = event.String("result");
					answered = true;
				}
				start = end + 1;
			}
			if (!answered)
			{
				error = Msg("Claude Code could not answer. Sign in with \"claude\" in a terminal, and check your quota.",
					"Claude Code n'a pas pu répondre. Connectez-vous avec \"claude\" dans un terminal, et vérifiez votre quota.");
				return false;
			}
			return true;
		}
		case ClientKind::Codex:
		{
			std::ifstream file(answer_file, std::ios::binary);
			answer.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			if (answer.empty())
			{
				error = Msg("Codex did not answer. Check its sign-in, the model and the account's limits.",
					"Codex n'a pas répondu. Vérifiez sa connexion, le modèle et les limites du compte.");
				return false;
			}
			return true;
		}
		default:
		{
			// One JSON event per line; the answer is the text parts put back together
			size_t start = 0;
			while (start < output.size())
			{
				size_t end = output.find('\n', start);
				if (end == std::string::npos)
					end = output.size();
				json::Value event;
				if (json::Parse(output.substr(start, end - start), event))
				{
					if (event.String("type") == "error")
					{
						error = Msg("OpenCode refused the request. Check its account and the chosen model.",
							"OpenCode a refusé la demande. Vérifiez son compte et le modèle choisi.");
						return false;
					}
					if (event.String("type") == "text")
						if (const json::Value *part = event.Get("part"))
							answer += part->String("text");
				}
				start = end + 1;
			}
			if (answer.empty())
			{
				error = Msg("OpenCode did not answer.", "OpenCode n'a pas répondu.");
				return false;
			}
			return true;
		}
		}
	}
}
