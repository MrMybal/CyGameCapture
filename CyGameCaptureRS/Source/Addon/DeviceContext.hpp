// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — per-device state (attached to reshade::api::device as private data).
// Owns the resource tracker, the preview and the capture streams for that device and drives
// the per-frame work from the 'present' and 'reshade_begin_effects' events.
#pragma once

#include "Backends/Backend.hpp"
#include "BufferInspector/InspectorModel.hpp"
#include "Capture/CaptureManager.hpp"
#include "Capture/FrameSnapshots.hpp"
#include "Assistant/AiAssistant.hpp"
#include "Preview/PreviewManager.hpp"
#include "Preview/ThumbnailGrid.hpp"
#include "UI/Logo.hpp"
#include "ResourceTracker/ResourceTracker.hpp"

#include <reshade.hpp>
#include <memory>
#include <string>
#include <vector>

namespace cygc
{
	// What the player sees on screen
	enum class ViewMode
	{
		Game,             // untouched game output (HUD included)
		SelectedBuffer,   // the selected buffer is copied over the back buffer before ReShade effects: play without HUD
	};

	struct __declspec(uuid("7f2a9e1c-3d5b-4c6e-9a8f-1b2c3d4e5f60")) DeviceContext
	{
		/**
		 * No artificial cap on simultaneous streams: the real ceiling is Spout's machine-wide sender
		 * table (64 by default, see SpoutSender::CanRegisterNewSender) and the GPU cost of one copy per
		 * stream per frame. This value only guards against a runaway loop creating streams forever.
		 */
		static constexpr size_t kSanityStreamLimit = 256;

		static DeviceContext *From(reshade::api::device *device);
		static DeviceContext *Create(reshade::api::device *device);
		static void Destroy(reshade::api::device *device);

		reshade::api::device *const gpu_device;
		const BackendSupport backend;
		std::unique_ptr<ResourceTracker> tracker;
		std::unique_ptr<PreviewManager> preview;
		/** Copies of buffers taken at a chosen moment of the frame; shared by the preview, the view mode and the captures. */
		std::unique_ptr<FrameSnapshots> snapshots;
		/** The AI assistant's side in the game: its conversation, and the pictures it sends. */
		std::unique_ptr<AiAssistant> ai;
		/** The CyGameCapture logo for the overlay; the texture is only made the first time it is drawn. */
		std::unique_ptr<LogoTexture> logo;
		/** Contact sheet of the listed buffers; only refreshed while the overlay shows it. */
		std::unique_ptr<ThumbnailGrid> thumbnails;
		size_t thumbnail_page = 0;       // first entry of the page the grid is drawing
		bool thumbnails_wanted = false;  // set by the overlay for the next present
		uint32_t thumbnails_idle_ = 0;   // presents in a row without the sheet on screen
		// Capture streams: slot 0 is the primary stream ("CyGameCaptureRS::Game"), additional slots
		// get "::2", "::3", ... Every active slot costs one GPU copy per frame (two on D3D12).
		std::vector<std::unique_ptr<CaptureManager>> captures;
		InspectorModel inspector;

		// Submit the copies right away (ID3D11DeviceContext::Flush) before the Spout access mutexes are
		// released, like Spout's own sender does. Off by default: Present follows immediately anyway and the
		// flush costs ~200 us of CPU per frame (measured), while it does not add any GPU side ordering guarantee.
		bool flush_after_copy = false;
		double last_flush_us = 0.0;

		/** Queue of the last present; the D3D12 bridge is built on it. */
		reshade::api::command_queue *present_queue = nullptr;

		/** Incremented once per presented frame and published by every stream, for recorder alignment. */
		uint64_t present_frame_index = 0;

		ViewMode view_mode = ViewMode::Game;
		uint32_t view_toggle_key = 0;       // virtual key code toggling the view mode in game (0 = disabled)
		std::string view_message;           // why the selected buffer cannot be shown in game (empty when fine)

		/**
		 * The selected buffer at the inspector's moment of the frame, for the preview and the view mode.
		 * Kept while either of them uses it, released otherwise so no copy runs for nothing.
		 */
		bool SelectionAtMoment(const TrackedResource &selected, TrackedResource &out);
		void ReleaseSelectionSnapshot();
		uint64_t selection_snapshot_ = 0;
		reshade::api::resource selection_snapshot_source_ = {};
		uint32_t selection_snapshot_moment_ = 0;
		std::string selection_snapshot_error;   // shown under the moment slider (empty when fine)

		// Effect runtimes living on this device (one per swap chain); used for hotkey polling
		std::vector<reshade::api::effect_runtime *> runtimes;

		// Per frame work (runs in the 'present' event, i.e. after the game finished its frame and before
		// ReShade renders effects and the overlay): merge immediate command list state, finalize the tracker
		// frame, capture copies, view mode copy, preview copy
		void OnPresent(reshade::api::command_queue *queue, reshade::api::swapchain *swapchain);

	private:
		// View mode: copy the selected buffer over the back buffer so the player sees it without HUD
		void ApplyViewMode(reshade::api::command_queue *queue, reshade::api::swapchain *swapchain);
		void PollHotkeys();

	public:

		CaptureManager &PrimaryCapture() { return *captures.front(); }
		CaptureManager *CaptureOf(uint32_t resource_id);        // active slot capturing this resource, or nullptr
		CaptureManager *CaptureAt(uint32_t resource_id, uint32_t moment);   // same, at that moment of the frame only
		// `transform` selects what happens between the resource and the shared texture: a plain copy, or
		// a shader pass such as the depth linearisation (see Capture/CaptureManager.hpp).
		// `secondary_id` is only meaningful for StreamTransform::HudDifference, where it is the buffer
		// holding the scene before the interface was composited on `resource_id`.
		// `moment` / `secondary_moment`: when in the frame each buffer is read, 0 = end of frame (see FrameSnapshots).
		bool StartCapture(uint32_t resource_id, std::string *error = nullptr, StreamTransform transform = StreamTransform::Copy, uint32_t secondary_id = 0,
			uint32_t moment = 0, uint32_t secondary_moment = 0);            // primary slot (replaces its source)
		bool StartCaptureInNewSlot(uint32_t resource_id, std::string *error = nullptr, StreamTransform transform = StreamTransform::Copy, uint32_t secondary_id = 0,
			uint32_t moment = 0, uint32_t secondary_moment = 0);            // additional stream
		void StopCapture(size_t slot);                                                    // slot index (0 = primary)
		void StopAllCaptures();

	private:
		explicit DeviceContext(reshade::api::device *device);
		~DeviceContext();
		friend struct DeviceContextAccess;

		// [CYGAMECAPTURE] AutoCapture=<width>x<height>:<format>[:depth][;...]
		// Bootstrap of the future profile system: once the frame is stable, automatically select the
		// first non-back-buffer resource written this frame that matches each entry (first entry =
		// primary stream, following entries = extra streams). Used by the automated tests.
		struct AutoCaptureEntry
		{
			// "#42": pick that exact resource instead of the first one matching a size and a format.
			// Resource ids are stable within a run, so a buffer dump can be acted upon directly.
			unsigned int id = 0;
			unsigned int width = 0;
			unsigned int height = 0;
			reshade::api::format format = reshade::api::format::unknown;
			StreamTransform transform = StreamTransform::Copy;   // ":depth" suffix in the entry
			unsigned int moment = 0;                             // "#5@2": right after the 2nd write of the frame
		};
		void TryAutoCapture();

		/**
		* [CYGAMECAPTURE] DumpBuffers=<frame>
		*
		* Writes the whole tracked resource list to the ReShade log once, at that frame. The Buffer
		* Inspector shows the same thing, but reading it means driving an overlay inside a running
		* game; this makes what a game exposes reportable from a log file alone.
		*/
		void TryDumpBuffers();
		bool dump_buffers_done_ = false;

		/**
		* [CYGAMECAPTURE] OpenOverlayOnStart=1
		*
		* Opens the ReShade overlay once the game has settled, through effect_runtime::open_overlay.
		* For automated checks of the interface: the alternative, sending the overlay key, lands in
		* whatever window has the focus, which is not necessarily the game.
		*/
		void TryOpenOverlay();
		bool open_overlay_done_ = false;
		void TryAiSearchOnStart();
		bool ai_search_on_start_done_ = false;
		bool auto_capture_done_ = false;
		bool auto_capture_parsed_ = false;
		std::vector<AutoCaptureEntry> auto_capture_entries_;
	};

	// Base sender name for this process, e.g. "CyGameCaptureRS::Beyond" (stream suffix added per slot)
	const std::string &SenderBaseName();
	void SetSenderBaseName(std::string name);

	// Device / swap chain / present / begin-effects events
	void RegisterDeviceEvents();
	void UnregisterDeviceEvents();
}
