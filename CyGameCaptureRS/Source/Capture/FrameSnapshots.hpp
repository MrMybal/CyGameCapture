// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — copies of a buffer taken at a chosen moment of the frame.
//
// Why
// ---
// Everything else in the add-on reads buffers at the end of the frame, in the `present` event. That is
// enough when a game keeps its clean scene in a buffer of its own, but plenty of games do not: Unreal
// Engine 4 tonemaps straight into the back buffer and then draws its interface into that same buffer
// (measured on Stray: clear, one tonemapper draw, then the interface draws, then present). At the end of
// the frame there is no buffer without the interface left anywhere. There is only a *moment* without it:
// right after the tonemapper, right before the first interface draw.
//
// How
// ---
// A snapshot is asked for as (buffer, K): "this buffer as it is after its K-th write of the frame".
// The tracker already sees every write before it happens, because ReShade calls the draw, clear and
// copy events *before* forwarding them to the driver. When the buffer is about to receive write K+1,
// the snapshot records a GPU copy of it into a texture it owns, on the same command list, so the copy
// runs first. A frame with K writes or fewer is snapshotted at its end instead, which makes the last
// position of the slider mean "end of frame", exactly like before.
//
// The capture, the preview and the view mode then read that texture through Proxy(), an ordinary
// TrackedResource pointing at it: every existing path (format conversion, dynamic resolution, the depth
// and HUD passes) works on it unchanged. That includes a HUD stream made of one buffer at two moments.
//
// Scope
// -----
// D3D11 only, on the immediate context. There, recording order is execution order, so "the K-th write"
// is well defined. D3D12 command lists are recorded in parallel and submitted later: finding the K-th
// write there means counting per command list and ordering them at submission, which is not done yet.
// Multisampled buffers are refused (the copy would need the same sample count and a resolve after).
// Nothing is ever read back to the CPU: a snapshot is one GPU copy per frame into an owned texture.
#pragma once

#include "GpuPass/PassInput.hpp"
#include "ResourceTracker/ResourceTracker.hpp"

#include <reshade.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace cygc
{
	class FrameSnapshots
	{
	public:
		explicit FrameSnapshots(reshade::api::device *device);
		~FrameSnapshots();

		FrameSnapshots(const FrameSnapshots &) = delete;
		FrameSnapshots &operator=(const FrameSnapshots &) = delete;

		/** Whether this device can take mid-frame snapshots at all, and if not, why. */
		bool IsSupported(std::string *why = nullptr) const;

		/** Told about every texture created and destroyed, so they can be kept out of the tracker. */
		void SetResourceNotice(ResourceNotice on_created, ResourceNotice on_destroyed);

		/**
		 * Starts snapshotting `source` after its `after_write`-th write of every frame, and returns a
		 * ticket for it (0 on failure). Several users of the same (buffer, moment) share one snapshot.
		 *
		 * A ticket rather than the (buffer, moment) pair, because a destroyed buffer's handle can be
		 * handed to a new one: a late Release of the old snapshot must not touch the new one.
		 */
		uint64_t Acquire(const TrackedResource &source, uint32_t after_write, std::string *error);
		void Release(uint64_t ticket);

		/**
		 * The snapshot as a resource to read from: same id, size, format and active area as the buffer,
		 * but pointing at the owned copy, in `shader_resource` state. False while none has been taken,
		 * or once the buffer is gone.
		 */
		bool Proxy(uint64_t ticket, const TrackedResource &source, TrackedResource &out) const;

		/** Called by the tracker before `target` receives `count` writes on `cmd_list`. The hot path. */
		void OnWrite(reshade::api::command_list *cmd_list, reshade::api::resource target, uint32_t count);

		/**
		 * End of the frame, in `present`, before anything reads a snapshot: a buffer that received K
		 * writes or fewer is snapshotted now, then the counters start over for the next frame.
		 */
		void EndFrame(reshade::api::command_queue *queue);

		/** The game destroyed a buffer: its snapshots are dropped (their users see the source as lost). */
		void OnResourceDestroyed(reshade::api::resource resource);

		/**
		 * Set while something is being snapshotted. The tracker reads it before every write notification,
		 * so nothing is paid on the draw path while no snapshot is in use.
		 */
		const std::atomic<bool> &ActiveFlag() const { return active_; }

	private:
		struct Entry
		{
			uint64_t ticket = 0;
			reshade::api::resource source = {};
			uint32_t after_write = 0;
			uint32_t users = 0;
			reshade::api::resource texture = {};
			reshade::api::resource_desc texture_desc = {};
			uint32_t writes = 0;        // this frame so far
			bool taken = false;         // this frame
			bool valid = false;         // the texture holds a picture
		};

		Entry *FindLocked(reshade::api::resource source, uint32_t after_write);
		Entry *FindLocked(uint64_t ticket);
		const Entry *FindLocked(uint64_t ticket) const;
		void RemoveLocked(Entry &entry);
		bool EnsureTextureLocked(Entry &entry, const reshade::api::resource_desc &source_desc);
		void TakeLocked(Entry &entry, reshade::api::command_list *cmd_list);
		void DestroyTextureLocked(Entry &entry);

		reshade::api::device *const device_;
		ResourceNotice on_created_;
		ResourceNotice on_destroyed_;

		/**
		 * The lock, with the thread holding it.
		 *
		 * D3D11 destroys a released resource late, during whatever runtime call comes next; ReShade then
		 * reports the destruction right there, on that thread. Any copy made here while holding the lock
		 * can therefore bring the destruction of some unrelated game texture back into
		 * OnResourceDestroyed, on this very thread. Taking the lock again would throw std::system_error
		 * and take the game down (it did, in Stray). So a destruction reported by the owning thread is
		 * only noted, and handled when that thread lets the lock go.
		 */
		class Guard
		{
		public:
			explicit Guard(FrameSnapshots &owner);
			~Guard();
			Guard(const Guard &) = delete;
			Guard &operator=(const Guard &) = delete;
		private:
			FrameSnapshots &owner_;
			std::unique_lock<std::mutex> lock_;
		};
		void DrainDestroyedLocked();

		mutable std::mutex mutex_;
		std::atomic<unsigned long> lock_owner_ { 0 };
		std::vector<reshade::api::resource> destroyed_while_locked_;   // only touched by the lock's owner
		std::vector<Entry> entries_;
		uint64_t next_ticket_ = 1;
		std::atomic<bool> active_ { false };
		// The D3D11 immediate context: the only command list whose recording order is the frame's order
		std::atomic<reshade::api::command_list *> immediate_ { nullptr };
	};
}
