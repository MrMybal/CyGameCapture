// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — a conversation with an AI about the game's buffers, inside the ReShade overlay.
//
// The question it exists for is "where is the game's picture without its interface?". The button asks it
// in one click (an automatic first message carrying the whole method); the discussion that follows lets the
// user say what is still wrong ("the subtitles are still there", "and the interface alone?") and get a new
// proposal. Every proposal is only applied when the user clicks.
//
// Each message goes to an AI command line client the user already has (Claude Code, Codex or OpenCode),
// through CyGameCaptureAI.exe, a separate process next to the add-on:
//   * the game side (this class) gathers what the AI needs about the current frame: its steady buffers,
//     the order they were written in, and, when asked, pictures of them rendered on the GPU into one shared
//     texture (the back buffer after each of its writes, the full-size buffers at the end of the frame);
//   * the helper reads the pictures back, runs the client with the conversation so far, and returns the
//     reply and the proposal it may contain.
// The game never reads a frame back to the CPU, never waits on the AI, and never holds a key. The clients
// keep no session either: the conversation is sent again with each message, which works the same way with
// all three of them.
//
// D3D11 only for now, like the moments of the frame it relies on.
#pragma once

#include "Capture/FrameSnapshots.hpp"
#include "GpuPass/BlitPass.hpp"
#include "GpuPass/PassInput.hpp"
#include "ResourceTracker/ResourceTracker.hpp"

#include <reshade.hpp>

#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace cygc
{
	/** The add-on's own module, to find CyGameCaptureAI.exe next to it. Set by AddonInit. */
	void SetAddonModule(HMODULE addon_module);

	struct AiProposal
	{
		uint32_t buffer = 0;          // resource id
		uint32_t moment = 0;          // 0 = end of the frame, K = right after its K-th write
		uint32_t back_buffer = 0;     // the finished image, for the HUD stream
		bool hud_stream = false;      // the interface alone can be had against the end of the frame
	};

	struct AiMessage
	{
		bool from_user = false;
		std::string text;             // UTF-8
		std::string client;           // who answered (assistant messages)
		bool pictures = false;        // this message came with pictures of the frame
		bool has_proposal = false;
		AiProposal proposal;
		bool failed = false;          // an error, shown in the conversation rather than as an answer
	};

	class AiAssistant
	{
	public:
		enum class State { Idle, Collecting, Running };

		AiAssistant(reshade::api::device *device, ResourceTracker &tracker, FrameSnapshots &snapshots);
		~AiAssistant();

		AiAssistant(const AiAssistant &) = delete;
		AiAssistant &operator=(const AiAssistant &) = delete;

		bool IsSupported(std::string *why) const;

		/** The button: the automatic first question, with pictures of the current frame. */
		void FindSettingWithoutInterface();
		/** A message typed by the user; `attach_pictures` adds pictures of the frame as it is now. */
		void Send(const std::string &text, bool attach_pictures);
		/** Forgets the conversation (the files of the last exchange stay in the folder). */
		void NewConversation();

		/** Present time, after the captures: renders the pictures when asked, then starts the helper. */
		void OnPresent(reshade::api::command_queue *queue, reshade::api::resource current_back_buffer);
		/** From the overlay while something runs: reads the helper's progress and reply. Cheap. */
		void Poll();

		State GetState() const { return state_; }
		bool IsBusy() const { return state_ != State::Idle; }
		const std::string &Progress() const { return progress_; }
		const std::vector<AiMessage> &Conversation() const { return conversation_; }
		/** Bumped with every new message, so the chat view knows when to scroll down. */
		uint32_t Revision() const { return revision_; }
		/** The most recent proposal of the conversation, or nullptr. */
		const AiMessage *LatestProposal() const;

	private:
		struct Tile
		{
			uint32_t id = 0;
			uint32_t moment = 0;       // 0 = end of the frame
			uint64_t ticket = 0;       // snapshot, for a moment inside the frame
			int index = -1;            // position in the atlas, -1 when it could not be pictured
			std::string label;
		};

		void Start(const std::string &text, bool attach_pictures, bool find_mode);
		bool Plan(reshade::api::resource current_back_buffer);
		bool Render(reshade::api::command_queue *queue);
		bool WriteRequest(std::string *error);
		bool LaunchHelper(std::string *error);
		void ReleaseTickets();
		void ReleaseGpu();
		void Fail(const std::string &message);
		void AddMessage(AiMessage message);

		reshade::api::device *const device_;
		ResourceTracker &tracker_;
		FrameSnapshots &snapshots_;

		State state_ = State::Idle;
		bool requested_ = false;
		bool find_mode_ = false;           // the automatic first question rather than a typed message
		bool attach_pictures_ = false;
		int frames_to_wait_ = 0;
		bool rendered_ = false;
		std::string progress_;
		std::vector<AiMessage> conversation_;
		uint32_t revision_ = 0;

		std::vector<Tile> tiles_;
		uint32_t back_buffer_id_ = 0;
		uint32_t back_buffer_writes_ = 0;
		uint32_t tile_width_ = 640;
		uint32_t tile_height_ = 360;
		uint32_t columns_ = 4;

		// GPU side: one tile render target reused for every tile, and the shared atlas they are copied to
		FullscreenPass blit_;
		reshade::api::resource tile_ = {};
		reshade::api::resource_view tile_target_ = {};
		reshade::api::resource atlas_ = {};
		void *atlas_handle_ = nullptr;
		uint32_t atlas_width_ = 0;
		uint32_t atlas_height_ = 0;
		PassInput input_;

		// The helper's side. The run id is unique across launches of the game (its process id is part of
		// it): the files of an earlier exchange left in the folder must never be taken for this one's.
		uint64_t run_id_ = 0;
		uint32_t run_count_ = 0;
		std::wstring folder_;
		uint64_t launched_at_ = 0;
		uint64_t last_poll_ = 0;
		bool helper_has_images_ = false;
	};
}
