// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — capture output.
//
// D3D11 / D3D10 (1 GPU copy):
//   Selected game resource --copy/resolve--> persistent shared texture (reshade create_resource with
//   resource_flags::shared -> legacy DXGI handle) --> Spout sender "CyGameCaptureRS::<Game>"
//
// D3D12 (2 GPU copies, through the D3D11On12 bridge, see Backends/D3D12/D3D11On12Bridge.hpp):
//   Selected game resource --copy/resolve--> intermediate (reshade resource on the game device)
//   --[bridge: Acquire / CopyResource / Release / Flush]--> shared D3D11 texture --> Spout sender
//
// The output texture is re-created automatically when the source width / height / format changes.
// When the source resource disappears the capture goes to "Capture Source Lost" and stops copying;
// the sender stays registered so receivers keep their source until a new buffer is selected.
//
// There is no artificial limit on the number of simultaneous streams: the only ceiling is Spout's
// own sender-name table (64 by default, machine wide, see SpoutSender::CanRegisterNewSender).
#pragma once

#include <CyGameCaptureCore/FrameSync.hpp>

#include "GpuPass/DepthPass.hpp"
#include "GpuPass/HudPass.hpp"
#include "GpuPass/BlitPass.hpp"
#include "GpuPass/PassInput.hpp"
#include "Capture/FrameSnapshots.hpp"

#include "SpoutSender.hpp"
#include "ResourceTracker/ResourceTracker.hpp"
#include "Backends/Backend.hpp"

#include <reshade.hpp>
#include <Windows.h>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <string>

struct ID3D11Texture2D;
struct ID3D11Resource;

namespace cygc
{
	enum class CaptureState
	{
		Idle,          // no source selected
		Active,        // copying every frame
		SourceLost,    // the selected resource was destroyed
		Unsupported,   // backend / format cannot be sent
		Error,
	};
	const char *CaptureStateName(CaptureState state);

	struct CaptureStatus
	{
		CaptureState state = CaptureState::Idle;
		std::string message;                 // human readable detail / last error
		std::string sender_name;
		uint32_t source_id = 0;
		uint32_t width = 0;
		uint32_t height = 0;
		// Area of the source that actually held the last frame. Smaller than the source when the game
		// scales its resolution; the stream itself keeps `width` x `height` whatever happens.
		uint32_t active_width = 0;
		uint32_t active_height = 0;
		reshade::api::format format = reshade::api::format::unknown;
		float fps = 0.0f;
		uint64_t frames_sent = 0;
		uint64_t frames_skipped = 0;         // receiver held the access mutex too long
		double last_copy_cpu_us = 0.0;       // CPU time spent recording the copy (mutex wait + command submission), not GPU time
		double last_mutex_wait_us = 0.0;     // part of it spent waiting for the Spout access mutex
		double last_bridge_us = 0.0;         // D3D12 only: CPU time of the D3D11On12 bridge copy
		bool uses_bridge = false;            // D3D12 path
	};

	/**
	* What happens between the selected resource and the shared texture.
	*
	* `Copy` is the original path and stays the cheapest: one GPU copy, bit for bit.
	* `DepthLinear` runs a shader instead, because a depth buffer is neither shareable nor linear as it
	* stands (see GpuPass/DepthPass.hpp).
	*/
	enum class StreamTransform
	{
		Copy,
		DepthLinear,
		/// Finished image minus the scene before the interface, so the interface comes out on its own
		/// with an alpha channel (see GpuPass/HudPass.hpp).
		HudDifference,
	};

	class CaptureManager
	{
	public:
		// slot_index is 1-based: stream 1 uses the plain sender name, stream N gets the "::N" suffix.
		CaptureManager(reshade::api::device *device, const BackendSupport &backend, std::string sender_base_name, unsigned int slot_index = 1);
		~CaptureManager();
		unsigned int SlotIndex() const { return slot_index_; }

		CaptureManager(const CaptureManager &) = delete;
		CaptureManager &operator=(const CaptureManager &) = delete;

		// "Use As Capture Source"
		bool Start(const TrackedResource &source, std::string *error = nullptr)
		{
			return Start(source, StreamTransform::Copy, error);
		}
		bool Start(const TrackedResource &source, StreamTransform transform, std::string *error)
		{
			return Start(source, transform, 0, error);
		}
		/**
		* `secondary_id` is only used by HudDifference, where `source` is the finished image and the
		* secondary resource is the scene before the interface was drawn on it.
		*
		* `moment` and `secondary_moment` choose when in the frame each buffer is read: 0 is the end of
		* the frame, K > 0 is right after its K-th write (see Capture/FrameSnapshots). With them the HUD
		* stream can compare one buffer with itself: the back buffer at the end of the frame against the
		* same back buffer right after the tonemapper, before the interface was drawn into it.
		*/
		bool Start(const TrackedResource &source, StreamTransform transform, uint32_t secondary_id, std::string *error,
			uint32_t moment = 0, uint32_t secondary_moment = 0);

		/** Resource the HUD stream subtracts, 0 for every other transform. */
		uint32_t SecondarySourceId() const;
		/** Moment of the frame each buffer is read at (0 = end of frame). */
		uint32_t Moment() const;
		uint32_t SecondaryMoment() const;

		HudSettings GetHudSettings() const;
		void SetHudSettings(const HudSettings &settings);

		StreamTransform Transform() const;

		/** Depth linearisation settings; they only matter while the transform is DepthLinear. */
		DepthSettings GetDepthSettings() const;
		void SetDepthSettings(const DepthSettings &settings);
		void Stop();

		// Called synchronously from the tracker when a resource is destroyed
		void OnResourceDestroyed(reshade::api::resource resource, uint32_t id);

		// Per presented frame, on the presenting queue, after the tracker finalized the frame.
		// Three phases so that every stream shares a single queue submission:
		//   BeginCopy : validates the source, (re)creates the output when needed, takes the Spout access
		//               mutex and records the GPU copy. Returns true when EndCopy has to be called.
		//   [caller]  : one queue->flush_immediate_command_list() when any stream needs it
		//   EndCopy   : D3D12 bridge copy (if any), mutex release, statistics.
		bool BeginCopy(reshade::api::command_queue *queue, const ResourceTracker &tracker);
		void EndCopy(uint64_t frame_index);

		/** True when this stream needs the queue submitted between BeginCopy and EndCopy (D3D12 bridge). */
		bool NeedsQueueFlush() const { return uses_bridge_; }

		/**
		 * The presenting queue, needed by the D3D12 bridge (D3D11On12CreateDevice takes the queue the
		 * shared texture will be produced on). Set by DeviceContext on every present and before Start,
		 * so a capture started from the overlay already knows it.
		 */
		void SetPresentQueue(reshade::api::command_queue *queue);
		/** The queue is being destroyed by the game: stop referencing it (no wait_idle on a dead queue). */
		void ForgetQueue(reshade::api::command_queue *queue);

		/**
		* The back buffer the swap chain is about to present.
		*
		* A swap chain owns several back buffers and rotates through them, so the one the user picked in
		* the inspector only holds the finished frame every second or third present. Capturing that
		* specific resource would mix current and stale frames -- visible as ghosting in a HUD stream,
		* where the finished image is compared against the scene of the *current* frame. So a stream
		* whose source is a back buffer follows whichever one is current instead.
		*/
		void SetCurrentBackBuffer(reshade::api::resource back_buffer);

		/** Set once by DeviceContext; see tracker_. */
		void SetTracker(ResourceTracker *tracker) { tracker_ = tracker; }
		/** Set once by DeviceContext; needed to read a buffer at a moment of the frame. */
		void SetSnapshots(FrameSnapshots *snapshots) { snapshots_ = snapshots; }

		CaptureStatus GetStatus() const;
		bool IsActive() const;
		uint32_t SourceId() const;

		// Shader resource view on the output texture (valid while active): lets the preview show the
		// exact image that is being sent without a second GPU copy.
		reshade::api::resource_view OutputView() const;
		reshade::api::resource_desc OutputDesc() const;

	private:
		bool EnsureOutput(const reshade::api::resource_desc &source_desc, std::string *error);
		/** Output format the current transform produces from this source format. */
		reshade::api::format OutputFormatFor(reshade::api::format source_format) const;
		/** Keeps a texture this class created out of the tracker; see tracker_. */
		void ForgetOurResource(reshade::api::resource resource);
		/**
		* The resource a stream should actually read this frame: the selected one, or the current back
		* buffer when the selection is one of the swap chain's.
		*/
		bool ResolveSource(const ResourceTracker &tracker, const TrackedResource &selected, TrackedResource &out) const;
		/** PrepareInput with the tracker kept clear of the readable copy it may create. */
		bool PrepareInput(PassInput &input, const TrackedResource &source, reshade::api::command_list *cmd_list);
		void DestroyPassInputs();
		bool CreateSharedOutputD3D11(uint32_t width, uint32_t height, reshade::api::format out_format, std::string *error);
		bool CreateSharedOutputD3D12(uint32_t width, uint32_t height, reshade::api::format out_format, std::string *error);
		void DestroyOutput();
		void SetState(CaptureState state, std::string message);

		/**
		* The tracker, so the textures this class creates for itself can be kept out of it. They are not
		* part of the game's frame, and ReShade reports a destruction synchronously: a tracked one would
		* call back into this very object while it holds its own lock.
		*/
		ResourceTracker *tracker_ = nullptr;
		FrameSnapshots *snapshots_ = nullptr;
		void ReleaseSnapshots();

		reshade::api::device *const device_;
		const BackendSupport backend_;
		const std::string sender_base_name_;
		const unsigned int slot_index_;
		const bool uses_bridge_;             // D3D12: copies go through the D3D11On12 bridge

		mutable std::mutex mutex_;
		/**
		 * Thread currently inside a critical section that may destroy a resource, 0 when none.
		 *
		 * Belt and braces against re-entering our own lock. Forget() should already keep ReShade from
		 * reporting our textures back to us, but a reentrant lock throws std::system_error and takes the
		 * game down with it, so the case is checked rather than assumed away.
		 */
		std::atomic<unsigned long> lock_owner_ { 0 };
		CaptureState state_ = CaptureState::Idle;
		std::string message_;

		reshade::api::resource source_ = {};
		uint32_t source_id_ = 0;
		reshade::api::resource_desc source_desc_ = {};

		// D3D11: the shared texture itself. D3D12: the intermediate the game queue copies into.
		reshade::api::resource output_ = {};
		reshade::api::resource_view output_srv_ = {};
		reshade::api::resource_view output_rtv_ = {};   // only created for a transform that renders
		reshade::api::resource_desc output_desc_ = {};
		HANDLE share_handle_ = nullptr;

		// D3D12 bridge objects (null on D3D11)
		ID3D11Texture2D *bridge_shared_ = nullptr;
		ID3D11Resource *bridge_wrapped_ = nullptr;
		// Cross-process frame index published next to the Spout sender so a recorder can line several
		// streams of the same frame up exactly (see CyGameCaptureCore/FrameSync.hpp).
		FrameSyncWriter frame_sync_;

		// Derived streams: the shader pass, the view on the source it reads, and its settings.
		StreamTransform transform_ = StreamTransform::Copy;
		FullscreenPass depth_pass_;
		DepthSettings depth_settings_;
		FullscreenPass hud_pass_;
		HudSettings hud_settings_;
		// Used by the plain copy path when the source is only partly rendered into (dynamic resolution)
		FullscreenPass blit_pass_;
		ActiveRect active_rect_;        // area of the source that actually held the last frame
		bool downscaled_ = false;       // so the dynamic-resolution message is logged once, not per frame
		reshade::api::resource current_back_buffer_ = {};   // refreshed every present
		uint32_t secondary_id_ = 0;                    // HudDifference only: the scene before the interface
		uint32_t moment_ = 0;                          // 0 = end of the frame, K = after the K-th write
		uint32_t secondary_moment_ = 0;
		uint64_t snapshot_ticket_ = 0;                 // FrameSnapshots tickets while a moment is used
		uint64_t secondary_snapshot_ticket_ = 0;
		PassInput primary_input_;
		PassInput secondary_input_;

		void *bridge_queue_native_ = nullptr;             // ID3D12CommandQueue* of the presenting queue
		reshade::api::command_queue *present_queue_ = nullptr;   // same queue, ReShade side (wait_idle before releases)

		SpoutSender sender_;

		uint64_t frames_sent_ = 0;
		uint64_t frames_skipped_ = 0;
		double last_copy_cpu_us_ = 0.0;
		double last_mutex_wait_us_ = 0.0;
		double last_bridge_us_ = 0.0;
		bool copy_in_progress_ = false;
		uint64_t copy_start_ticks_ = 0;
		float fps_ = 0.0f;
		uint64_t fps_window_start_ = 0;
		uint32_t fps_window_frames_ = 0;
	};
}
