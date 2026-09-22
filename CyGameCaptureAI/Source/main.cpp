// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureAI — asks an AI client where a game's picture without its interface is.
//
//   CyGameCaptureAI.exe <folder>\request.json
//
// Started by CyGameCaptureRS for every message of the AI assistant's discussion (the "Find the setting that
// removes the game's interface" button is its automatic first message), with a request describing the
// conversation and the current frame: its buffers, the order they were written in, and, when the message
// has pictures, a shared texture holding them. This process reads those pictures back into PNG files, asks
// the chosen AI command line client (Claude Code, Codex or OpenCode, with the user's own sign-in) with the
// whole conversation (the clients keep no session), and writes the reply back:
//
//   status.txt   run_id / stage (images, asking, done, error) / message       -- progress, for the overlay
//   reply.txt    the reply as the user reads it (UTF-8)
//   result.txt   run_id / ok / client / proposal / buffer / moment / hud_stream
//   prompt.txt, answer.txt, picture_NN.png                                     -- what was asked, for review
//
// Everything stays next to the request, in %LOCALAPPDATA%\CyGameCapture\AI\<game>. Nothing is applied to
// the game from here: the overlay shows the proposal and the user decides.
#include "AiClients.hpp"
#include "MiniJson.hpp"
#include "Pictures.hpp"

#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace cygc;

namespace
{
	fs::path g_folder;
	std::string g_run_id = "0";

	/** key=value files, replaced in one move so the overlay never reads half of one. */
	void WriteKeyValues(const wchar_t *name, const std::vector<std::pair<std::string, std::string>> &values)
	{
		const fs::path path = g_folder / name;
		const fs::path temporary = g_folder / (std::wstring(name) + L".tmp");
		{
			std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
			for (const auto &[key, value] : values)
			{
				std::string single_line = value;
				for (char &c : single_line)
					if (c == '\n' || c == '\r')
						c = ' ';
				file << key << '=' << single_line << '\n';
			}
		}
		MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
	}

	void Status(const char *stage, const std::string &message)
	{
		WriteKeyValues(L"status.txt", { { "run_id", g_run_id }, { "stage", stage }, { "message", message } });
		printf("[%s] %s\n", stage, message.c_str());
	}

	int Failure(const std::string &message)
	{
		WriteKeyValues(L"result.txt", { { "run_id", g_run_id }, { "ok", "0" }, { "message", message } });
		Status("error", message);
		return 1;
	}

	std::string ReadText(const fs::path &path)
	{
		std::ifstream file(path, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	}

	/** Re-serialises a part of the request compactly, for the prompt. */
	void Serialise(const json::Value &value, std::string &out)
	{
		switch (value.type)
		{
		case json::Value::Type::Null: out += "null"; break;
		case json::Value::Type::Bool: out += value.boolean ? "true" : "false"; break;
		case json::Value::Type::Number:
		{
			char number[32];
			snprintf(number, sizeof(number), "%g", value.number);
			out += number;
			break;
		}
		case json::Value::Type::String: json::AppendString(out, value.string); break;
		case json::Value::Type::Array:
			out += '[';
			for (size_t i = 0; i < value.array.size(); ++i)
			{
				if (i > 0) out += ",\n";
				Serialise(value.array[i], out);
			}
			out += ']';
			break;
		case json::Value::Type::Object:
			out += '{';
			for (size_t i = 0; i < value.object.size(); ++i)
			{
				if (i > 0) out += ',';
				json::AppendString(out, value.object[i].first);
				out += ':';
				Serialise(value.object[i].second, out);
			}
			out += '}';
			break;
		}
	}

	/**
	 * The method: what a person does in the inspector to find the picture without the interface, written
	 * for the AI. It is the same reasoning that found Stray's clean picture by hand. It goes with every
	 * message, since the clients keep no session.
	 */
	std::string BuildPrompt(const json::Value &request, const std::vector<TileRequest> &tiles)
	{
		const std::string language = request.String("language", "fr") == "en" ? "English" : "French";
		const bool find_mode = request.String("mode") == "find";
		const unsigned back_buffer = static_cast<unsigned>(request.Number("back_buffer"));
		const unsigned writes = static_cast<unsigned>(request.Number("back_buffer_writes"));

		std::string prompt =
			"You are the assistant of CyGameCapture, a tool that streams a game's internal GPU buffers (render targets) "
			"to OBS, running inside the game through ReShade. You talk with its user. The main question is where the game's "
			"picture WITHOUT its user interface is (no HUD, menus, button prompts, subtitles or cursor), so it can be "
			"streamed clean; the user may also ask for the interface alone, or about the buffers in general.\n\n"
			"How games are built, and what to look for:\n"
			"- Engines render the scene, tonemap it into a buffer, then draw the interface over it. The picture without the "
			"interface is therefore either:\n"
			"  (a) a separate buffer that, at the end of the frame, looks exactly like the final image minus the interface; or\n"
			"  (b) the back buffer itself at an earlier moment of the frame, when the engine tonemaps straight into it and "
			"draws the interface right after (Unreal Engine 4 does this). \"Right after its write K\" means the buffer as it "
			"was just after its K-th write of the frame. Write 1 is often only a clear, so it is black.\n"
			"- The right answer looks like the finished game image: same framing, colours, brightness and post-processing "
			"as the end-of-frame back buffer, only without the interface.\n"
			"- Reject: black or partly drawn pictures; HDR or linear buffers that look much darker, washed out or blown out; "
			"G-buffers (normals, albedo, masks), depth; and pictures blurred or darkened by an open menu (that blur and "
			"darkening belong to the menu, so they are interface).\n"
			"- For (b), choose the earliest moment where the whole picture is present and no interface element is. "
			"Choose (a) only when it really matches the final look.\n"
			"- Some interface elements may be drawn earlier than others (subtitles or markers drawn into the scene). If the "
			"user says an element is still there, look for an earlier moment or another buffer, and say plainly when no "
			"moment removes it without also losing the picture.\n"
			"- If the end-of-frame picture shows no interface at all, the user has to show a scene where it is visible: say so.\n\n"
			"The current frame of the game \"" + request.String("game") + "\":\n"
			"- The back buffer (what the player sees) is #" + std::to_string(back_buffer) + "; it received " +
			std::to_string(writes) + " writes during the frame, not counting present.\n";

		if (!tiles.empty())
		{
			prompt += "- The pictures attached to this message, in this order:\n";
			for (size_t i = 0; i < tiles.size(); ++i)
				prompt += "  " + std::to_string(i + 1) + ". " + tiles[i].label + "\n";
		}
		else
			prompt += "- No picture is attached to this message; rely on what was seen and said before.\n";

		std::string buffers, order;
		if (const json::Value *value = request.Get("buffers"))
			Serialise(*value, buffers);
		if (const json::Value *value = request.Get("write_order"))
			Serialise(*value, order);
		prompt += "- Buffers of the frame (steady ones, a quarter of the screen or more):\n" + buffers + "\n"
			"- Order of the writes during the frame (consecutive draws into the same buffer are merged):\n" + order + "\n\n";

		if (const json::Value *conversation = request.Get("conversation"); conversation != nullptr && !conversation->array.empty())
		{
			prompt += "The conversation so far (pictures of earlier messages are not attached again):\n";
			for (const json::Value &message : conversation->array)
			{
				prompt += message.String("role") == "user" ? "USER" : "ASSISTANT";
				if (message.Bool("pictures"))
					prompt += " (with pictures of the frame at that time)";
				prompt += ":\n" + message.String("text") + "\n\n";
			}
		}
		prompt += "The user's new message:\n" + request.String("message") + "\n\n"
			"How to answer:\n"
			"- Reply in " + language + ", briefly and concretely: what you see, what to use. Plain text only, it is shown "
			"as is in a small in-game window: no Markdown (no asterisks, no headings, no tables); simple \"- \" lists are fine.\n";
		if (find_mode)
			prompt += "- End your reply with exactly one JSON object on its own, in this form:\n";
		else
			prompt += "- When your reply proposes a setting (a new one or a corrected one), end it with exactly one JSON "
				"object on its own, in this form; otherwise add no JSON at all:\n";
		prompt +=
			"{\"found\": true, \"buffer\": 5, \"moment\": 2, \"hud_stream\": true}\n"
			"  buffer: the id of the buffer to capture. moment: 0 for the end of the frame, K for right after write K "
			"(only for the back buffer). hud_stream: true when the proposal and the end-of-frame back buffer have the same "
			"framing, so their difference is the interface alone. found: false when there is nothing to propose.\n"
			"- Under your reply, a proposal becomes buttons: \"Show\" (preview), \"Apply\" (streams the picture) and, "
			"when hud_stream is true, \"Apply, and send the interface alone too\" (a second stream with the interface alone). "
			"Refer to those buttons, never to the JSON or its fields, and do not claim to have changed anything yourself.\n"
			"- Do not run commands or use tools: everything needed is here.\n";
		return prompt;
	}

	/** The last JSON object of the answer that has a "found" field, and where it starts. */
	bool FindProposal(const std::string &answer, json::Value &proposal, size_t &position)
	{
		for (position = answer.rfind('{'); position != std::string::npos; position = position > 0 ? answer.rfind('{', position - 1) : std::string::npos)
		{
			json::Parser parser(answer.data() + position, answer.data() + answer.size());
			json::Value value;
			if (parser.Parse(value) && value.type == json::Value::Type::Object && value.Get("found") != nullptr)
			{
				proposal = std::move(value);
				return true;
			}
			if (position == 0)
				break;
		}
		return false;
	}

	/** Emphasis markers a model may still use; the in-game window shows text as is. */
	std::string WithoutEmphasis(std::string text)
	{
		for (const char *marker : { "**", "__" })
			for (size_t found = text.find(marker); found != std::string::npos; found = text.find(marker, found))
				text.erase(found, 2);
		return text;
	}

	/** The reply as the user reads it: without the JSON block the buttons already stand for. */
	std::string ReplyWithoutProposal(std::string text)
	{
		auto trim = [](std::string &s) {
			while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\r' || s.back() == '\t'))
				s.pop_back();
		};
		trim(text);
		for (const char *fence : { "```json", "```JSON", "```" })
		{
			const size_t length = strlen(fence);
			if (text.size() >= length && text.compare(text.size() - length, length, fence) == 0)
			{
				text.erase(text.size() - length);
				trim(text);
				break;
			}
		}
		return text;
	}
}

int wmain(int argc, wchar_t **argv)
{
	if (argc < 2)
	{
		printf("usage: CyGameCaptureAI.exe <request.json>\n");
		return 2;
	}
	const fs::path request_path = argv[1];
	g_folder = request_path.parent_path();

	json::Value request;
	if (!json::Parse(ReadText(request_path), request) || request.type != json::Value::Type::Object)
	{
		printf("could not read %ls\n", request_path.c_str());
		return 2;
	}
	g_run_id = std::to_string(static_cast<unsigned long long>(request.Number("run_id")));
	SetFrenchMessages(request.String("language", "en") != "en");
	const DWORD game_pid = static_cast<DWORD>(request.Number("pid"));

	// 1. The pictures, out of the game's shared texture, when this message has some
	std::vector<TileRequest> tiles;
	if (const json::Value *list = request.Get("tiles"))
		for (const json::Value &item : list->array)
			tiles.push_back({ static_cast<uint32_t>(item.Number("index")), static_cast<uint32_t>(item.Number("buffer")),
				static_cast<uint32_t>(item.Number("moment")), item.String("label") });

	std::vector<std::wstring> pictures;
	std::string error;
	if (const json::Value *atlas = request.Get("atlas"); atlas != nullptr && !tiles.empty())
	{
		Status("images", Msg("Reading the pictures of the frame...", "Lecture des images de la frame..."));
		AtlasLayout layout;
		layout.width = static_cast<uint32_t>(atlas->Number("width"));
		layout.height = static_cast<uint32_t>(atlas->Number("height"));
		layout.tile_width = static_cast<uint32_t>(atlas->Number("tile_width"));
		layout.tile_height = static_cast<uint32_t>(atlas->Number("tile_height"));
		layout.columns = std::max<uint32_t>(1, static_cast<uint32_t>(atlas->Number("columns")));
		if (!ExportTiles(static_cast<uint64_t>(atlas->Number("handle")), layout, tiles, g_folder.wstring(), pictures, error))
			return Failure(error.empty() ? Msg("No picture to look at", "Aucune image à examiner") : error);
	}
	else
		tiles.clear();

	// 2. The client: the one chosen in the overlay, or the first one installed
	const std::string chosen = request.String("client", "auto");
	const std::wstring override_path = fs::u8path(request.String("client_path")).wstring();
	std::vector<ClientKind> order;
	if (chosen == "claude") order = { ClientKind::ClaudeCode };
	else if (chosen == "codex") order = { ClientKind::Codex };
	else if (chosen == "opencode") order = { ClientKind::OpenCode };
	else order = { ClientKind::ClaudeCode, ClientKind::Codex, ClientKind::OpenCode };

	ClientKind kind = order.front();
	std::vector<std::wstring> command;
	for (const ClientKind candidate : order)
	{
		command = FindClient(candidate, override_path);
		if (!command.empty())
		{
			kind = candidate;
			break;
		}
	}
	if (command.empty())
		return Failure(chosen == "auto"
			? Msg("No AI client found: install Claude Code, Codex or OpenCode and sign in once.",
				"Aucun client IA trouvé : installez Claude Code, Codex ou OpenCode et connectez-vous une fois.")
			: std::string(ClientName(order.front())) + Msg(" was not found. Install it, or set AIClientPath in ReShade.ini.",
				" est introuvable. Installez-le, ou indiquez AIClientPath dans ReShade.ini."));

	// 3. The question
	AskRequest ask;
	ask.prompt = BuildPrompt(request, tiles);
	ask.images = pictures;
	ask.model = request.String("model");
	ask.work_folder = g_folder.wstring();
	ask.game_pid = game_pid;
	std::ofstream(g_folder / L"prompt.txt", std::ios::binary | std::ios::trunc) << ask.prompt;

	Status("asking", std::string(ClientName(kind)) + Msg(" is thinking... (usually under a minute or two)", " réfléchit... (en général une à deux minutes)"));
	std::string answer;
	if (!AskClient(kind, command, ask, answer, error))
		return Failure(error);
	std::ofstream(g_folder / L"answer.txt", std::ios::binary | std::ios::trunc) << answer;

	// 4. The reply, and the proposal it may end with, checked against the frame that was described
	std::string reply = answer;
	json::Value proposal;
	size_t position = 0;
	bool has_proposal = false;
	unsigned buffer = 0, moment = 0;
	if (FindProposal(answer, proposal, position))
	{
		reply = ReplyWithoutProposal(answer.substr(0, position));
		if (proposal.Bool("found"))
		{
			buffer = static_cast<unsigned>(proposal.Number("buffer"));
			moment = static_cast<unsigned>(proposal.Number("moment"));
			bool known = false;
			for (const TileRequest &tile : tiles)
				known |= tile.buffer == buffer;
			if (const json::Value *list = request.Get("buffers"))
				for (const json::Value &item : list->array)
					known |= static_cast<unsigned>(item.Number("id")) == buffer;
			// A moment only exists for the back buffer, and only within the frame
			if (buffer != static_cast<unsigned>(request.Number("back_buffer")) || moment >= static_cast<unsigned>(request.Number("back_buffer_writes")))
				moment = 0;
			has_proposal = known;
			if (!known)
				reply += "\n\n(#" + std::to_string(buffer) + Msg(" is not one of this frame's buffers: no proposal to apply.)",
					" n'est pas un tampon de cette frame : aucune proposition à appliquer.)");
		}
	}
	if (reply.empty())
		reply = answer;
	std::ofstream(g_folder / L"reply.txt", std::ios::binary | std::ios::trunc) << WithoutEmphasis(reply);

	WriteKeyValues(L"result.txt", {
		{ "run_id", g_run_id }, { "ok", "1" }, { "client", ClientName(kind) }, { "proposal", has_proposal ? "1" : "0" },
		{ "buffer", std::to_string(buffer) }, { "moment", std::to_string(moment) },
		{ "hud_stream", has_proposal && proposal.Bool("hud_stream") ? "1" : "0" } });
	Status("done", "");
	return 0;
}
