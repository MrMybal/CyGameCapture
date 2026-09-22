// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "AiAssistant.hpp"
#include "Addon/Config.hpp"
#include "Addon/Log.hpp"
#include "ResourceTracker/FormatUtils.hpp"
#include "UI/Localization.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace reshade::api;

namespace cygc
{
	namespace
	{
		HMODULE g_addon_module = nullptr;

		constexpr format kTileFormat = format::r8g8b8a8_unorm;
		constexpr size_t kMaxMoments = 8;         // pictures of the back buffer inside the frame, end included
		constexpr size_t kMaxCandidates = 8;      // other full-size buffers at the end of the frame
		constexpr size_t kMaxWriteOrder = 300;    // entries of the frame's write order sent along
		constexpr size_t kMaxBuffers = 40;
		constexpr size_t kMaxHistory = 12;        // previous messages sent with a new one
		constexpr size_t kMaxMessageBytes = 4000; // of each of them
		constexpr uint64_t kHelperTimeoutMs = 10 * 60 * 1000;
		constexpr uint64_t kAtlasKeepMs = 60 * 1000;

		void AppendJsonString(std::string &out, const std::string &text)
		{
			out += '"';
			for (const char c : text)
			{
				switch (c)
				{
				case '"': out += "\\\""; break;
				case '\\': out += "\\\\"; break;
				case '\n': out += "\\n"; break;
				case '\r': out += "\\r"; break;
				case '\t': out += "\\t"; break;
				default:
					if (static_cast<unsigned char>(c) < 0x20)
					{
						char escaped[8];
						snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned int>(c));
						out += escaped;
					}
					else
						out += c;
				}
			}
			out += '"';
		}

		std::string GameName()
		{
			wchar_t path[MAX_PATH] = {};
			GetModuleFileNameW(nullptr, path, MAX_PATH);
			return std::filesystem::path(path).stem().u8string();
		}

		std::string ReadFile(const std::wstring &path)
		{
			std::ifstream file(std::filesystem::path(path), std::ios::binary);
			return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		}

		/** "key=value" lines, as written by the helper: nothing to parse beyond that in the game. */
		std::vector<std::pair<std::string, std::string>> ReadKeyValues(const std::wstring &path)
		{
			std::vector<std::pair<std::string, std::string>> values;
			std::istringstream lines(ReadFile(path));
			std::string line;
			while (std::getline(lines, line))
			{
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				const size_t equal = line.find('=');
				if (equal != std::string::npos)
					values.emplace_back(line.substr(0, equal), line.substr(equal + 1));
			}
			return values;
		}

		std::string Find(const std::vector<std::pair<std::string, std::string>> &values, const char *key)
		{
			for (const auto &[k, v] : values)
				if (k == key)
					return v;
			return {};
		}

		/** The replies follow the interface's language, unless AILanguage says otherwise. */
		std::string ReplyLanguage()
		{
			return config::GetString("AILanguage", i18n::Code());
		}

		bool French()
		{
			return ReplyLanguage() != "en";
		}
	}

	void SetAddonModule(HMODULE addon_module)
	{
		g_addon_module = addon_module;
	}

	AiAssistant::AiAssistant(device *device, ResourceTracker &tracker, FrameSnapshots &snapshots) :
		device_(device),
		tracker_(tracker),
		snapshots_(snapshots)
	{
	}

	AiAssistant::~AiAssistant()
	{
		ReleaseTickets();
		ReleaseGpu();
		blit_.Destroy();
	}

	bool AiAssistant::IsSupported(std::string *why) const
	{
		if (device_->get_api() == device_api::d3d11)
			return true;
		if (why != nullptr)
			*why = "D3D11 games only for now: the assistant looks at the back buffer at every moment of the frame, "
				"which is not available on D3D12 yet.";
		return false;
	}

	const AiMessage *AiAssistant::LatestProposal() const
	{
		for (auto it = conversation_.rbegin(); it != conversation_.rend(); ++it)
			if (!it->from_user && it->has_proposal)
				return &*it;
		return nullptr;
	}

	void AiAssistant::AddMessage(AiMessage message)
	{
		conversation_.push_back(std::move(message));
		++revision_;
	}

	void AiAssistant::FindSettingWithoutInterface()
	{
		Start(French() ? "Trouve le réglage qui enlève l'interface du jeu." : "Find the setting that removes the game's interface.", true, true);
	}

	void AiAssistant::Send(const std::string &text, bool attach_pictures)
	{
		if (text.find_first_not_of(" \t\r\n") == std::string::npos)
			return;
		Start(text, attach_pictures, false);
	}

	void AiAssistant::NewConversation()
	{
		if (IsBusy())
			return;
		conversation_.clear();
		++revision_;
	}

	void AiAssistant::Start(const std::string &text, bool attach_pictures, bool find_mode)
	{
		if (IsBusy())
			return;
		AiMessage message;
		message.from_user = true;
		message.text = text;
		message.pictures = attach_pictures;
		AddMessage(std::move(message));

		requested_ = true;
		find_mode_ = find_mode;
		attach_pictures_ = attach_pictures;
		state_ = State::Collecting;
		progress_ = attach_pictures ? "Collecting pictures of the frame..." : "Starting the AI client...";
	}

	void AiAssistant::Fail(const std::string &text)
	{
		AiMessage message;
		message.text = text;
		message.failed = true;
		AddMessage(std::move(message));
		state_ = State::Idle;
		progress_.clear();
		ReleaseTickets();
		log::Warning("AI assistant: %s", text.c_str());
	}

	void AiAssistant::ReleaseTickets()
	{
		for (Tile &tile : tiles_)
		{
			snapshots_.Release(tile.ticket);
			tile.ticket = 0;
		}
	}

	void AiAssistant::ReleaseGpu()
	{
		DestroyInput(device_, input_, [this](resource res) { tracker_.Forget(res); });
		if (tile_target_ != 0)
			device_->destroy_resource_view(tile_target_);
		for (resource *const res : { &tile_, &atlas_ })
		{
			if (*res == 0)
				continue;
			tracker_.Forget(*res);
			device_->destroy_resource(*res);
			*res = {};
		}
		tile_target_ = {};
		atlas_handle_ = nullptr;
	}

	bool AiAssistant::Plan(resource current_back_buffer)
	{
		ReleaseTickets();
		tiles_.clear();

		TrackedResource back_buffer;
		if (current_back_buffer == 0 || !tracker_.GetByHandle(current_back_buffer, back_buffer))
		{
			Fail("The back buffer is not known yet; try again once the game shows a frame.");
			return false;
		}
		back_buffer_id_ = back_buffer.id;
		// The present counts as a write of a back buffer; the moments are the game's own writes
		back_buffer_writes_ = back_buffer.last.writes > 0 ? back_buffer.last.writes - 1 : 0;
		if (!attach_pictures_)
			return true;   // the buffer list and the write order are enough for this message

		// Tiles keep the picture's proportions, so the interface is where it is on screen
		tile_width_ = 640;
		tile_height_ = back_buffer.Width() > 0 ? std::clamp((640u * back_buffer.Height() / back_buffer.Width()) & ~1u, 200u, 480u) : 360u;

		// The back buffer after each of its writes (spread out when there are many), then at the end
		std::vector<uint32_t> moments;
		const uint32_t inside = back_buffer_writes_ > 1 ? back_buffer_writes_ - 1 : 0;
		if (inside + 1 <= kMaxMoments)
		{
			for (uint32_t k = 1; k <= inside; ++k)
				moments.push_back(k);
		}
		else
		{
			// The first writes matter most (clear, tonemapper, first interface draw); the rest is sampled
			for (uint32_t k = 1; k <= 3; ++k)
				moments.push_back(k);
			const uint32_t spread = static_cast<uint32_t>(kMaxMoments) - 4;
			for (uint32_t i = 1; i <= spread; ++i)
			{
				const uint32_t k = 3 + (inside - 3) * i / spread;
				if (k > moments.back())
					moments.push_back(k);
			}
		}
		for (const uint32_t k : moments)
		{
			std::string error;
			Tile tile;
			tile.id = back_buffer.id;
			tile.moment = k;
			tile.ticket = snapshots_.Acquire(back_buffer, k, &error);
			if (tile.ticket == 0)
			{
				log::Warning("AI assistant: no snapshot of #%03u after write %u: %s", back_buffer.id, k, error.c_str());
				continue;
			}
			char label[160];
			snprintf(label, sizeof(label), "#%03u back buffer, right after its write %u of %u", back_buffer.id, k, back_buffer_writes_);
			tile.label = label;
			tiles_.push_back(tile);
		}
		{
			Tile tile;
			tile.id = back_buffer.id;
			char label[160];
			snprintf(label, sizeof(label), "#%03u back buffer at the end of the frame (what the player sees)", back_buffer.id);
			tile.label = label;
			tiles_.push_back(tile);
		}

		// Other buffers the size of the screen, at the end of the frame: a game may keep a clean one
		std::vector<TrackedResource> candidates;
		const uint64_t frame = tracker_.FrameIndex();
		for (const TrackedResource &res : tracker_.Snapshot())
		{
			if (res.is_backbuffer || !res.IsTexture2D() || res.last.writes == 0 || !res.IsPersistent(frame))
				continue;
			if (res.IsDepthStencil() || IsDepthStencilFormat(res.Format()) || res.IsMultisampled() || IsBlockCompressedFormat(res.Format()))
				continue;
			if (res.Width() * 2 < back_buffer.Width() || res.Height() * 2 < back_buffer.Height())
				continue;
			candidates.push_back(res);
		}
		std::sort(candidates.begin(), candidates.end(), [](const TrackedResource &a, const TrackedResource &b) {
			const uint64_t area_a = static_cast<uint64_t>(a.Width()) * a.Height(), area_b = static_cast<uint64_t>(b.Width()) * b.Height();
			return area_a != area_b ? area_a > area_b : a.id < b.id;
		});
		if (candidates.size() > kMaxCandidates)
			candidates.resize(kMaxCandidates);
		for (const TrackedResource &res : candidates)
		{
			Tile tile;
			tile.id = res.id;
			char label[160];
			snprintf(label, sizeof(label), "#%03u %ux%u %s at the end of the frame", res.id, res.Width(), res.Height(), FormatName(res.Format()));
			tile.label = label;
			tiles_.push_back(tile);
		}
		return true;
	}

	bool AiAssistant::Render(command_queue *queue)
	{
		command_list *const cmd_list = queue->get_immediate_command_list();
		std::string error;
		if (!blit_.IsValid() && !CreateBlitPass(device_, kTileFormat, blit_, error))
		{
			Fail("Picture pass unavailable: " + error);
			return false;
		}

		const resource_desc tile_desc(tile_width_, tile_height_, 1, 1, kTileFormat, 1, memory_heap::default_,
			resource_usage::render_target | resource_usage::shader_resource | resource_usage::copy_source);
		if (!device_->create_resource(tile_desc, nullptr, resource_usage::render_target, &tile_) ||
			!device_->create_resource_view(tile_, resource_usage::render_target, resource_view_desc(kTileFormat), &tile_target_))
		{
			Fail("Could not create the picture texture");
			return false;
		}
		tracker_.Forget(tile_);

		const uint32_t rows = static_cast<uint32_t>((tiles_.size() + columns_ - 1) / columns_);
		atlas_width_ = tile_width_ * columns_;
		atlas_height_ = tile_height_ * std::max(rows, 1u);
		const resource_desc atlas_desc(atlas_width_, atlas_height_, 1, 1, kTileFormat, 1, memory_heap::default_,
			resource_usage::copy_dest | resource_usage::shader_resource, resource_flags::shared);
		atlas_handle_ = nullptr;
		if (!device_->create_resource(atlas_desc, nullptr, resource_usage::copy_dest, &atlas_, &atlas_handle_) || atlas_handle_ == nullptr)
		{
			Fail("Could not create the shared picture atlas");
			return false;
		}
		tracker_.Forget(atlas_);

		const auto forget = [this](resource res) { tracker_.Forget(res); };
		uint32_t index = 0;
		for (Tile &tile : tiles_)
		{
			TrackedResource source;
			if (!tracker_.Get(tile.id, source))
				continue;   // gone since the plan
			if (tile.moment != 0)
			{
				TrackedResource at_moment;
				if (!snapshots_.Proxy(tile.ticket, source, at_moment))
					continue;
				source = at_moment;
			}
			if (!PrepareInput(device_, input_, source, cmd_list, forget))
				continue;

			// Only the part the game rendered into (dynamic resolution), like the capture
			const SourceRect rect = MakeSourceRect(source.ActiveArea(), source.Width(), source.Height());
			const BlitConstants constants { { rect.scale_x, rect.scale_y, rect.offset_x, rect.offset_y } };
			const bool direct = input_.copy == 0;
			const resource_usage source_state = GuessSourceState(source);
			if (direct)
				cmd_list->barrier(source.handle, source_state, resource_usage::shader_resource);
			blit_.Draw(cmd_list, tile_target_, &input_.view, 1, &constants, sizeof(constants) / sizeof(uint32_t), tile_width_, tile_height_);
			if (direct)
				cmd_list->barrier(source.handle, resource_usage::shader_resource, source_state);

			const uint32_t x = (index % columns_) * tile_width_;
			const uint32_t y = (index / columns_) * tile_height_;
			const subresource_box box { x, y, 0, x + tile_width_, y + tile_height_, 1 };
			cmd_list->barrier(tile_, resource_usage::render_target, resource_usage::copy_source);
			cmd_list->copy_texture_region(tile_, 0, nullptr, atlas_, 0, &box);
			cmd_list->barrier(tile_, resource_usage::copy_source, resource_usage::render_target);
			tile.index = static_cast<int>(index++);
		}
		DestroyInput(device_, input_, forget);
		if (index == 0)
		{
			Fail("None of the buffers could be pictured");
			return false;
		}

		// Submitted now: the helper opens the atlas from another process a couple of frames later
		queue->flush_immediate_command_list();
		return true;
	}

	bool AiAssistant::WriteRequest(std::string *error)
	{
		std::string json = "{\n\"version\": 2,\n\"run_id\": " + std::to_string(run_id_) + ",\n\"game\": ";
		AppendJsonString(json, GameName());
		json += ",\n\"pid\": " + std::to_string(GetCurrentProcessId());
		json += ",\n\"mode\": ";
		AppendJsonString(json, find_mode_ ? "find" : "chat");
		json += ",\n\"client\": ";
		AppendJsonString(json, config::GetString("AIClient", "auto"));
		json += ",\n\"model\": ";
		AppendJsonString(json, config::GetString("AIModel", ""));
		json += ",\n\"client_path\": ";
		AppendJsonString(json, config::GetString("AIClientPath", ""));
		json += ",\n\"language\": ";
		AppendJsonString(json, ReplyLanguage());
		json += ",\n\"back_buffer\": " + std::to_string(back_buffer_id_) + ",\n\"back_buffer_writes\": " + std::to_string(back_buffer_writes_);

		// The conversation: the new message, and the ones before it (the clients keep no session)
		std::vector<const AiMessage *> history;
		for (const AiMessage &message : conversation_)
			if (!message.failed)
				history.push_back(&message);
		const AiMessage *current = history.empty() ? nullptr : history.back();
		if (!history.empty())
			history.pop_back();
		if (history.size() > kMaxHistory)
			history.erase(history.begin(), history.end() - kMaxHistory);
		json += ",\n\"message\": ";
		AppendJsonString(json, current != nullptr ? current->text : std::string());
		json += ",\n\"conversation\": [";
		bool first = true;
		for (const AiMessage *message : history)
		{
			json += first ? "\n" : ",\n";
			first = false;
			json += "  {\"role\": ";
			AppendJsonString(json, message->from_user ? "user" : "assistant");
			json += ", \"pictures\": ";
			json += message->pictures ? "true" : "false";
			json += ", \"text\": ";
			AppendJsonString(json, message->text.substr(0, kMaxMessageBytes));
			json += "}";
		}
		json += "\n]";

		if (atlas_handle_ != nullptr)
		{
			json += ",\n\"atlas\": {\"handle\": " + std::to_string(reinterpret_cast<uintptr_t>(atlas_handle_)) +
				", \"width\": " + std::to_string(atlas_width_) + ", \"height\": " + std::to_string(atlas_height_) +
				", \"tile_width\": " + std::to_string(tile_width_) + ", \"tile_height\": " + std::to_string(tile_height_) +
				", \"columns\": " + std::to_string(columns_) + "}";
		}
		json += ",\n\"tiles\": [";
		first = true;
		for (const Tile &tile : tiles_)
		{
			if (tile.index < 0 || atlas_handle_ == nullptr)
				continue;
			json += first ? "\n" : ",\n";
			first = false;
			json += "  {\"index\": " + std::to_string(tile.index) + ", \"buffer\": " + std::to_string(tile.id) +
				", \"moment\": " + std::to_string(tile.moment) + ", \"label\": ";
			AppendJsonString(json, tile.label);
			json += "}";
		}
		json += "\n]";

		// The buffers worth considering, and the order they were written in during the last frame
		const std::vector<TrackedResource> all = tracker_.Snapshot();
		uint32_t screen_w = 0, screen_h = 0;
		for (const TrackedResource &res : all)
			if (res.id == back_buffer_id_)
			{
				screen_w = res.Width();
				screen_h = res.Height();
			}
		const uint64_t frame = tracker_.FrameIndex();
		auto large = [&](const TrackedResource &res) { return res.Width() * 4 >= screen_w && res.Height() * 4 >= screen_h; };

		json += ",\n\"buffers\": [";
		first = true;
		size_t count = 0;
		for (const TrackedResource &res : all)
		{
			if (!res.IsTexture2D() || res.last.writes == 0 || !large(res) || (!res.is_backbuffer && !res.IsPersistent(frame)))
				continue;
			if (++count > kMaxBuffers)
				break;
			json += first ? "\n" : ",\n";
			first = false;
			char line[320];
			snprintf(line, sizeof(line), "  {\"id\": %u, \"width\": %u, \"height\": %u, \"format\": \"%s\", \"writes\": %u, \"draws\": %u, "
				"\"steady_percent\": %.0f, \"back_buffer\": %s, \"depth\": %s}",
				res.id, res.Width(), res.Height(), FormatName(res.Format()), res.last.writes, res.last.draw_calls,
				res.WriteRatio(frame) * 100.0f, res.is_backbuffer ? "true" : "false",
				(res.IsDepthStencil() || IsDepthStencilFormat(res.Format())) ? "true" : "false");
			json += line;
		}
		json += "\n]";

		json += ",\n\"write_order\": [";
		first = true;
		count = 0;
		for (const TimelineEntry &entry : tracker_.TimelineSnapshot())
		{
			if (!entry.is_write)
				continue;
			const auto it = std::find_if(all.begin(), all.end(), [&](const TrackedResource &res) { return res.id == entry.resource_id; });
			if (it == all.end() || !it->IsTexture2D() || !large(*it))
				continue;
			if (++count > kMaxWriteOrder)
				break;
			json += first ? "\n" : ",\n";
			first = false;
			char line[160];
			snprintf(line, sizeof(line), "  {\"buffer\": %u, \"kind\": \"%s\", \"count\": %u}", entry.resource_id, WriteKindName(entry.kind), entry.count);
			json += line;
		}
		json += "\n]\n}\n";

		std::ofstream file(std::filesystem::path(folder_) / L"request.json", std::ios::binary | std::ios::trunc);
		file << json;
		if (!file)
		{
			if (error != nullptr)
				*error = "Could not write the request in " + std::filesystem::path(folder_).u8string();
			return false;
		}
		return true;
	}

	bool AiAssistant::LaunchHelper(std::string *error)
	{
		wchar_t module_path[MAX_PATH] = {};
		GetModuleFileNameW(g_addon_module, module_path, MAX_PATH);
		const std::filesystem::path helper = std::filesystem::path(module_path).parent_path() / L"CyGameCaptureAI.exe";
		if (!std::filesystem::exists(helper))
		{
			if (error != nullptr)
				*error = "CyGameCaptureAI.exe is missing: copy it next to CyGameCaptureRS.addon64.";
			return false;
		}

		const std::wstring request = (std::filesystem::path(folder_) / L"request.json").wstring();
		std::wstring command_line = L"\"" + helper.wstring() + L"\" \"" + request + L"\"";
		STARTUPINFOW startup = { sizeof(startup) };
		PROCESS_INFORMATION process = {};
		// No window over the game, and nothing to wait for here: progress comes back through files
		if (!CreateProcessW(helper.c_str(), command_line.data(), nullptr, nullptr, FALSE,
			CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, nullptr, folder_.c_str(), &startup, &process))
		{
			if (error != nullptr)
				*error = "Could not start CyGameCaptureAI.exe (error " + std::to_string(GetLastError()) + ")";
			return false;
		}
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		launched_at_ = GetTickCount64();
		helper_has_images_ = false;
		return true;
	}

	void AiAssistant::OnPresent(command_queue *queue, resource current_back_buffer)
	{
		if (requested_)
		{
			requested_ = false;
			std::string why;
			if (!IsSupported(&why))
				return Fail(why);
			run_id_ = static_cast<uint64_t>(GetCurrentProcessId()) * 1000 + (++run_count_ % 1000);
			wchar_t local[MAX_PATH] = {};
			if (GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH) == 0)
				return Fail("LOCALAPPDATA is not set");
			std::error_code ec;
			const std::filesystem::path folder = std::filesystem::path(local) / L"CyGameCapture" / L"AI" / std::filesystem::u8path(GameName());
			std::filesystem::create_directories(folder, ec);
			folder_ = folder.wstring();
			ReleaseGpu();
			if (!Plan(current_back_buffer))
				return;
			log::Info("AI assistant %llu: %s, %zu pictures (back buffer #%03u, %u writes per frame)", static_cast<unsigned long long>(run_id_),
				find_mode_ ? "search" : "message", tiles_.size(), back_buffer_id_, back_buffer_writes_);
			frames_to_wait_ = attach_pictures_ ? 2 : 0;   // one full frame for the snapshots inside it
			rendered_ = !attach_pictures_;
			return;
		}

		if (state_ == State::Collecting)
		{
			if (frames_to_wait_ > 0 && --frames_to_wait_ > 0)
				return;
			if (!rendered_)
			{
				const bool ok = Render(queue);
				ReleaseTickets();   // the pictures are in the atlas now, or never will be
				if (!ok)
					return ReleaseGpu();
				rendered_ = true;
				frames_to_wait_ = 2;   // let the GPU finish before another process reads the atlas
				return;
			}
			std::string error;
			if (!WriteRequest(&error) || !LaunchHelper(&error))
			{
				Fail(error);
				return ReleaseGpu();
			}
			state_ = State::Running;
			progress_ = "Starting the AI client...";
			return;
		}

		// The answer is picked up even while the overlay is closed
		Poll();

		// The atlas only has to live until the helper has read it
		if (atlas_ != 0 && (helper_has_images_ || state_ == State::Idle || GetTickCount64() - launched_at_ > kAtlasKeepMs))
			ReleaseGpu();
	}

	void AiAssistant::Poll()
	{
		if (state_ != State::Running)
			return;
		const uint64_t now = GetTickCount64();
		if (now - last_poll_ < 400)
			return;
		last_poll_ = now;

		const auto status = ReadKeyValues(folder_ + L"\\status.txt");
		if (Find(status, "run_id") != std::to_string(run_id_))
		{
			if (now - launched_at_ > kHelperTimeoutMs)
				Fail("CyGameCaptureAI.exe did not answer.");
			return;
		}
		const std::string stage = Find(status, "stage");
		if (stage != "images")
			helper_has_images_ = true;
		if (!Find(status, "message").empty())
			progress_ = Find(status, "message");

		if (stage != "done" && stage != "error")
		{
			if (now - launched_at_ > kHelperTimeoutMs)
				Fail("The AI client did not answer in time.");
			return;
		}

		const auto result = ReadKeyValues(folder_ + L"\\result.txt");
		if (Find(result, "run_id") != std::to_string(run_id_) || Find(result, "ok") != "1")
		{
			const std::string reason = Find(result, "message");
			return Fail(reason.empty() ? progress_ : reason);
		}

		AiMessage reply;
		reply.client = Find(result, "client");
		reply.text = ReadFile(folder_ + L"\\reply.txt");
		reply.has_proposal = Find(result, "proposal") == "1";
		if (reply.has_proposal)
		{
			reply.proposal.buffer = static_cast<uint32_t>(strtoul(Find(result, "buffer").c_str(), nullptr, 10));
			reply.proposal.moment = static_cast<uint32_t>(strtoul(Find(result, "moment").c_str(), nullptr, 10));
			reply.proposal.hud_stream = Find(result, "hud_stream") == "1";
			reply.proposal.back_buffer = back_buffer_id_;
			log::Info("AI assistant %llu: %s proposes #%03u at moment %u", static_cast<unsigned long long>(run_id_),
				reply.client.c_str(), reply.proposal.buffer, reply.proposal.moment);
		}
		AddMessage(std::move(reply));
		state_ = State::Idle;
		progress_.clear();
	}
}
