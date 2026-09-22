// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — GPU resource tracker.
//
// Follows every 2D texture a game creates that can be rendered into (render target,
// depth-stencil or unordered access) plus anything that becomes the destination of a
// copy / resolve, and records per frame:
//   * how many times it was written (draws, clears, copies, resolves, compute hints)
//   * the event index of its first / last write (position inside the frame)
//   * whether it is a swap chain back buffer
//
// Design:
//   * Per command list state (CommandListState, stored as ReShade private data): bound render
//     targets + a compact list of access events recorded while the game records commands.
//     No locking here, a command list is only ever recorded from one thread at a time.
//   * Device level tracker (ResourceTracker): the map of tracked resources. Command list
//     events are merged into it when the command list is executed (D3D12/Vulkan/deferred
//     contexts) or at present time for the immediate context (D3D11). Protected by a mutex.
//   * Nothing here assumes a GPU handle stays valid: resources are removed on destruction and
//     listeners (capture / preview) are notified synchronously so they can drop references.
#pragma once

#include <reshade.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cygc
{
	enum class WriteKind : uint8_t
	{
		None = 0,
		Draw,      // draw / draw_indexed / indirect / mesh dispatch with the resource bound as render target or depth-stencil
		Clear,     // clear_render_target_view / clear_depth_stencil_view / clear UAV
		Copy,      // destination of copy_resource / copy_texture_region / copy_buffer_to_texture
		Resolve,   // destination of resolve_texture_region
		Compute,   // barrier away from unordered_access state: the resource was probably written by a compute shader
		Present,   // swap chain presentation (back buffers only)
	};
	const char *WriteKindName(WriteKind kind);

	/**
	* The area of a render target a draw actually covered.
	*
	* It matters because of dynamic resolution. Most engines that scale their resolution at run time
	* do *not* reallocate their render targets: they keep them at the maximum size and render into a
	* sub-rectangle, then upscale. The texture description therefore never changes and says nothing
	* about how much of it holds the current frame; the viewport does.
	*
	* Stored in texels, already clamped to the resource. Width 0 means no viewport was ever seen.
	*/
	struct ActiveRect
	{
		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t width = 0;
		uint32_t height = 0;

		bool IsValid() const { return width != 0 && height != 0; }
		bool Covers(uint32_t full_width, uint32_t full_height) const
		{
			return !IsValid() || (x == 0 && y == 0 && width == full_width && height == full_height);
		}
		bool operator==(const ActiveRect &other) const
		{
			return x == other.x && y == other.y && width == other.width && height == other.height;
		}
		bool operator!=(const ActiveRect &other) const { return !(*this == other); }
	};

	struct AccessEvent
	{
		reshade::api::resource resource = {};
		WriteKind kind = WriteKind::None;
		uint32_t count = 1;
		bool is_write = true;
		// Viewport of the draw, for writes only. Kept raw: clamping it needs the resource description,
		// which only the tracker has.
		reshade::api::viewport viewport = {};
		bool has_viewport = false;
	};

	/** The viewport clamped to a resource, in texels. Width 0 when there is nothing usable. */
	ActiveRect ClampViewport(const reshade::api::viewport &viewport, const reshade::api::resource_desc &desc);

	struct FrameStats
	{
		uint32_t writes = 0;
		uint32_t reads = 0;
		uint32_t draw_calls = 0;
		uint32_t first_write_event = 0;
		uint32_t last_write_event = 0;
		uint32_t last_read_event = 0;
		WriteKind last_write_kind = WriteKind::None;
		bool cleared = false;
	};

	/**
	* Frames a resource has to have existed for before it can be called persistent.
	*
	* Engines allocate transient render targets from a pool: they appear, get written once or twice,
	* and are gone. They are noise in a list meant for picking a buffer to capture, and they are what
	* makes that list flicker. A quarter of a second of history is enough to tell them apart from the
	* targets an engine keeps for the whole run.
	*/
	constexpr uint64_t kPersistenceWindow = 30;

	struct TrackedResource
	{
		uint32_t id = 0;                              // stable, human readable identifier (#001 ...)
		reshade::api::resource handle = {};
		reshade::api::resource_desc desc = {};
		uint64_t created_frame = 0;
		uint64_t last_written_frame = 0;
		uint64_t last_read_frame = 0;
		uint64_t total_writes = 0;
		uint64_t frames_written = 0;                  // number of frames during which it received a write
		uint64_t total_reads = 0;
		FrameStats current;                           // frame in progress
		FrameStats last;                              // last completed frame
		// Area the last draw of the last completed frame covered. Equal to the full texture unless the
		// game is scaling its resolution by rendering into part of a fixed-size target.
		ActiveRect active_rect;
		ActiveRect current_active_rect;               // frame in progress
		bool is_backbuffer = false;
		bool late_registered = false;                 // discovered through usage, not through init_resource
		std::string debug_name;                       // set via set_resource_name (rarely available)

		bool IsTexture2D() const { return desc.type == reshade::api::resource_type::texture_2d || desc.type == reshade::api::resource_type::surface; }
		bool IsColorTarget() const { return (desc.usage & reshade::api::resource_usage::render_target) != 0; }
		bool IsDepthStencil() const { return (desc.usage & reshade::api::resource_usage::depth_stencil) != 0; }
		bool IsUnorderedAccess() const { return (desc.usage & reshade::api::resource_usage::unordered_access) != 0; }
		bool IsShaderResource() const { return (desc.usage & reshade::api::resource_usage::shader_resource) != 0; }
		bool IsMultisampled() const { return desc.texture.samples > 1; }
		bool HasMipsOrLayers() const { return desc.texture.levels > 1 || desc.texture.depth_or_layers > 1; }
		uint32_t Width() const { return desc.texture.width; }
		uint32_t Height() const { return desc.texture.height; }

		/** Area that actually holds the picture, which is the whole texture unless it is scaled. */
		ActiveRect ActiveArea() const
		{
			return active_rect.IsValid() ? active_rect : ActiveRect { 0, 0, Width(), Height() };
		}
		bool IsDownscaled() const { return active_rect.IsValid() && !active_rect.Covers(Width(), Height()); }

		/** Age in frames, at least one so it can be divided by. */
		uint64_t Age(uint64_t current_frame) const
		{
			return current_frame > created_frame ? current_frame - created_frame : 1;
		}
		/** Share of its life the resource was written during, 0..1: 1 means every single frame. */
		float WriteRatio(uint64_t current_frame) const
		{
			const float ratio = static_cast<float>(static_cast<double>(frames_written) / static_cast<double>(Age(current_frame)));
			return ratio > 1.0f ? 1.0f : ratio;
		}
		/**
		* A buffer the engine keeps and fills regularly, as opposed to one taken from a transient pool.
		* Old enough to judge, and written during at least half the frames since it appeared.
		*/
		bool IsPersistent(uint64_t current_frame) const
		{
			return Age(current_frame) >= kPersistenceWindow && frames_written * 2 >= Age(current_frame);
		}
		reshade::api::format Format() const { return desc.texture.format; }
	};

	struct TimelineEntry
	{
		uint32_t event_index = 0;
		uint32_t resource_id = 0;
		WriteKind kind = WriteKind::None;
		uint32_t count = 0;
		bool is_write = true;
	};

	class ResourceTracker;

	// Private data attached to every command list (and to the command queue: in D3D11 the
	// immediate device context is both, ReShade returns the same object for both roles).
	struct __declspec(uuid("3b1f8c2a-6e4d-4a0b-9c7e-5d2f1a0b8e41")) CommandListState
	{
		static constexpr uint32_t kMaxRenderTargets = 8;

		// Who records into this state, so a write can be reported while it is being recorded
		// (see ResourceTracker::SetWriteObserver). Filled when the command list is first seen.
		reshade::api::command_list *owner = nullptr;
		ResourceTracker *tracker = nullptr;

		reshade::api::resource render_targets[kMaxRenderTargets] = {};
		uint32_t render_target_count = 0;
		reshade::api::resource depth_stencil = {};
		bool in_render_pass = false;
		reshade::api::viewport viewport = {};          // last one bound on this command list
		bool viewport_set = false;
		std::vector<AccessEvent> events;

		void ResetBindings();
		void Reset();
		void Add(reshade::api::resource resource, WriteKind kind, bool is_write, uint32_t count = 1,
			bool record_viewport = false);
	};

	class ResourceTracker
	{
	public:
		explicit ResourceTracker(reshade::api::device *device);

		// ---- lifetime events -------------------------------------------------------------
		void OnInitResource(const reshade::api::resource_desc &desc, reshade::api::resource resource);
		void OnDestroyResource(reshade::api::resource resource);
		void OnInitResourceView(reshade::api::resource resource, reshade::api::resource_view view);
		void OnDestroyResourceView(reshade::api::resource_view view);
		void OnSetResourceName(reshade::api::resource resource, const char *name);
		void OnInitSwapchain(reshade::api::swapchain *swapchain);
		void OnDestroySwapchain(reshade::api::swapchain *swapchain);
		void OnInitCommandQueue(reshade::api::command_queue *queue);
		void OnDestroyCommandQueue(reshade::api::command_queue *queue);

		// ---- per frame -------------------------------------------------------------------
		reshade::api::resource ResolveView(reshade::api::resource_view view) const;
		void Merge(CommandListState &state);      // consumes the events of a command list
		void EndFrame();                          // finalizes statistics of the frame that was just presented

		// ---- queries (all thread-safe, return copies) --------------------------------------
		uint64_t FrameIndex() const;
		bool Get(uint32_t id, TrackedResource &out) const;
		bool GetByHandle(reshade::api::resource resource, TrackedResource &out) const;
		uint32_t IdOf(reshade::api::resource resource) const;
		bool IsBackbuffer(reshade::api::resource resource) const;
		reshade::api::resource_desc BackbufferDesc() const;
		std::vector<TrackedResource> Snapshot() const;
		std::vector<TimelineEntry> TimelineSnapshot() const;
		std::vector<reshade::api::command_queue *> Queues() const;

		struct Counters
		{
			uint64_t created = 0;
			uint64_t destroyed = 0;
			uint64_t alive = 0;
			uint64_t merges = 0;
			uint64_t events_last_frame = 0;
			uint64_t views = 0;
		};
		Counters GetCounters() const;

		using DestroyListener = std::function<void(reshade::api::resource, uint32_t)>;
		void AddDestroyListener(DestroyListener listener);

		/**
		 * Told about every write while it is being recorded, *before* it reaches the buffer: ReShade
		 * calls the draw, clear and copy events ahead of forwarding them. This is what lets a copy be
		 * slipped in between two writes of the same frame (see Capture/FrameSnapshots).
		 *
		 * Called on the draw path, so it is gated: nothing happens, beyond one relaxed atomic load, while
		 * `enabled` is false. Both are set once, before any command is recorded.
		 */
		using WriteObserver = std::function<void(reshade::api::command_list *, reshade::api::resource, uint32_t)>;
		void SetWriteObserver(WriteObserver observer, const std::atomic<bool> *enabled);
		void NotifyWrite(reshade::api::command_list *cmd_list, reshade::api::resource resource, uint32_t count) const
		{
			if (write_enabled_ != nullptr && write_enabled_->load(std::memory_order_relaxed) && write_observer_)
				write_observer_(cmd_list, resource, count);
		}

		/**
		* Drops a resource from the tracker without telling anyone.
		*
		* Used for the textures the add-on creates for itself (capture outputs, D3D12 intermediates,
		* readable copies). They have no business in the Buffer Inspector, and more importantly ReShade
		* reports their destruction synchronously: left tracked, destroying one would notify the very
		* CaptureManager that is destroying it, which is already holding its own lock.
		*/
		void Forget(reshade::api::resource resource);

	private:
		TrackedResource *FindOrRegisterLocked(reshade::api::resource resource);
		void ApplyEventLocked(TrackedResource &res, const AccessEvent &event, uint32_t event_index);

		reshade::api::device *const device_;
		mutable std::shared_mutex mutex_;
		std::unordered_map<uint64_t, TrackedResource> resources_;
		std::unordered_map<uint64_t, uint64_t> view_to_resource_;
		std::unordered_set<uint64_t> backbuffers_;
		// Textures the add-on created for itself: never tracked, never reported as destroyed
		std::unordered_set<uint64_t> ignored_;
		reshade::api::resource_desc backbuffer_desc_ = {};
		std::vector<reshade::api::command_queue *> queues_;
		std::vector<TimelineEntry> timeline_current_;
		std::vector<TimelineEntry> timeline_last_;
		std::vector<DestroyListener> destroy_listeners_;
		WriteObserver write_observer_;
		const std::atomic<bool> *write_enabled_ = nullptr;
		uint64_t frame_index_ = 1;
		uint32_t next_id_ = 1;
		uint32_t event_counter_ = 0;
		Counters counters_;
	};

	// Registers / unregisters all ReShade events feeding the tracker (command list side).
	void RegisterTrackerEvents();
	void UnregisterTrackerEvents();
}
