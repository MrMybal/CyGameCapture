// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "ResourceTracker.hpp"
#include "FormatUtils.hpp"
#include "Addon/DeviceContext.hpp"
#include "Addon/Log.hpp"

#include <algorithm>
#include <mutex>

using namespace reshade::api;

namespace cygc
{
	namespace
	{
		constexpr size_t kMaxTimelineEntries = 4096;
		constexpr size_t kMaxPendingEventsPerCommandList = 65536;

		bool IsTrackableTexture(const resource_desc &desc)
		{
			if (desc.type != resource_type::texture_2d && desc.type != resource_type::surface)
				return false;
			// Only textures that can be rendered into. Plain shader resources (assets) are ignored
			// to keep the list small; anything that later becomes a copy/resolve destination is picked up lazily.
			return (desc.usage & (resource_usage::render_target | resource_usage::depth_stencil | resource_usage::unordered_access)) != 0;
		}
	}

	const char *WriteKindName(WriteKind kind)
	{
		switch (kind)
		{
		case WriteKind::None: return "None";
		case WriteKind::Draw: return "Draw";
		case WriteKind::Clear: return "Clear";
		case WriteKind::Copy: return "Copy";
		case WriteKind::Resolve: return "Resolve";
		case WriteKind::Compute: return "Compute";
		case WriteKind::Present: return "Present";
		}
		return "?";
	}

	// ---------------------------------------------------------------------------------------
	// CommandListState
	// ---------------------------------------------------------------------------------------
	void CommandListState::ResetBindings()
	{
		render_target_count = 0;
		depth_stencil = {};
		in_render_pass = false;
	}
	void CommandListState::Reset()
	{
		ResetBindings();
		events.clear();
	}
	ActiveRect ClampViewport(const viewport &viewport, const resource_desc &desc)
	{
		if (viewport.width <= 0.0f || viewport.height <= 0.0f)
			return ActiveRect {};

		ActiveRect rect;
		rect.x = static_cast<uint32_t>(viewport.x < 0.0f ? 0.0f : viewport.x);
		rect.y = static_cast<uint32_t>(viewport.y < 0.0f ? 0.0f : viewport.y);
		// Round to the nearest texel: a scaled viewport is rarely a whole number
		rect.width = static_cast<uint32_t>(viewport.width + 0.5f);
		rect.height = static_cast<uint32_t>(viewport.height + 0.5f);

		// A viewport may legitimately be larger than its target; what interests us is the covered part
		if (rect.x >= desc.texture.width || rect.y >= desc.texture.height)
			return ActiveRect {};
		rect.width = rect.width > desc.texture.width - rect.x ? desc.texture.width - rect.x : rect.width;
		rect.height = rect.height > desc.texture.height - rect.y ? desc.texture.height - rect.y : rect.height;
		return rect;
	}

	void CommandListState::Add(resource resource, WriteKind kind, bool is_write, uint32_t count, bool record_viewport)
	{
		if (resource == 0)
			return;
		// Before anything else, and before the coalescing below: the observer counts every single write
		if (is_write && tracker != nullptr)
			tracker->NotifyWrite(owner, resource, count);
		// Coalesce consecutive identical accesses (hundreds of draws into the same target become one entry)
		if (!events.empty())
		{
			AccessEvent &back = events.back();
			if (back.resource == resource && back.kind == kind && back.is_write == is_write)
			{
				back.count += count;
				// The viewport of the most recent draw wins: it is the one that produced the picture
				if (record_viewport && viewport_set)
				{
					back.viewport = viewport;
					back.has_viewport = true;
				}
				return;
			}
		}
		if (events.size() >= kMaxPendingEventsPerCommandList)
			return; // safety valve for command lists that are recorded but never executed
		AccessEvent event { resource, kind, count, is_write };
		if (record_viewport && viewport_set)
		{
			event.viewport = viewport;
			event.has_viewport = true;
		}
		events.push_back(event);
	}

	// ---------------------------------------------------------------------------------------
	// ResourceTracker
	// ---------------------------------------------------------------------------------------
	ResourceTracker::ResourceTracker(device *device) :
		device_(device)
	{
		resources_.reserve(1024);
		view_to_resource_.reserve(4096);
		timeline_current_.reserve(kMaxTimelineEntries);
	}

	void ResourceTracker::OnInitResource(const resource_desc &desc, resource resource)
	{
		if (!IsTrackableTexture(desc))
			return;

		std::unique_lock<std::shared_mutex> lock(mutex_);

		TrackedResource &res = resources_[resource.handle];
		res.id = next_id_++;
		res.handle = resource;
		res.desc = desc;
		res.created_frame = frame_index_;
		res.is_backbuffer = backbuffers_.count(resource.handle) != 0;
		counters_.created++;
		counters_.alive = resources_.size();

		log::Verbose("Resource created #%03u %ux%u %s usage=0x%X samples=%u mips=%u layers=%u",
			res.id, desc.texture.width, desc.texture.height, FormatName(desc.texture.format),
			static_cast<uint32_t>(desc.usage), desc.texture.samples, desc.texture.levels, desc.texture.depth_or_layers);
	}

	void ResourceTracker::OnDestroyResource(resource resource)
	{
		uint32_t id = 0;
		{
			std::unique_lock<std::shared_mutex> lock(mutex_);
			if (ignored_.erase(resource.handle) != 0)
				return;   // one of ours: nobody is waiting to hear about it
			const auto it = resources_.find(resource.handle);
			if (it == resources_.end())
				return;
			id = it->second.id;
			log::Verbose("Resource destroyed #%03u", id);
			resources_.erase(it);
			counters_.destroyed++;
			counters_.alive = resources_.size();
		}
		// Notify without holding the lock: listeners may call back into queries
		for (const DestroyListener &listener : destroy_listeners_)
			listener(resource, id);
	}

	void ResourceTracker::OnInitResourceView(resource resource, resource_view view)
	{
		if (view == 0)
			return;
		std::unique_lock<std::shared_mutex> lock(mutex_);
		view_to_resource_[view.handle] = resource.handle;
		counters_.views = view_to_resource_.size();
	}

	void ResourceTracker::OnDestroyResourceView(resource_view view)
	{
		std::unique_lock<std::shared_mutex> lock(mutex_);
		view_to_resource_.erase(view.handle);
		counters_.views = view_to_resource_.size();
	}

	void ResourceTracker::OnSetResourceName(resource resource, const char *name)
	{
		if (name == nullptr)
			return;
		std::unique_lock<std::shared_mutex> lock(mutex_);
		if (const auto it = resources_.find(resource.handle); it != resources_.end())
			it->second.debug_name = name;
	}

	void ResourceTracker::OnInitSwapchain(swapchain *swapchain)
	{
		std::unique_lock<std::shared_mutex> lock(mutex_);
		const uint32_t count = swapchain->get_back_buffer_count();
		for (uint32_t i = 0; i < count; ++i)
		{
			const resource bb = swapchain->get_back_buffer(i);
			if (bb == 0)
				continue;
			backbuffers_.insert(bb.handle);
			backbuffer_desc_ = device_->get_resource_desc(bb);

			TrackedResource &res = resources_[bb.handle];
			if (res.id == 0)
			{
				res.id = next_id_++;
				res.handle = bb;
				res.desc = backbuffer_desc_;
				res.created_frame = frame_index_;
				counters_.created++;
			}
			res.is_backbuffer = true;
		}
		counters_.alive = resources_.size();
		log::Info("Swap chain initialized: %u back buffer(s) %ux%u %s", count,
			backbuffer_desc_.texture.width, backbuffer_desc_.texture.height, FormatName(backbuffer_desc_.texture.format));
	}

	void ResourceTracker::OnDestroySwapchain(swapchain *swapchain)
	{
		std::vector<std::pair<resource, uint32_t>> removed;
		{
			std::unique_lock<std::shared_mutex> lock(mutex_);
			const uint32_t count = swapchain->get_back_buffer_count();
			for (uint32_t i = 0; i < count; ++i)
			{
				const resource bb = swapchain->get_back_buffer(i);
				backbuffers_.erase(bb.handle);
				if (const auto it = resources_.find(bb.handle); it != resources_.end())
				{
					removed.emplace_back(bb, it->second.id);
					resources_.erase(it);
					counters_.destroyed++;
				}
			}
			counters_.alive = resources_.size();
		}
		for (const auto &[res, id] : removed)
			for (const DestroyListener &listener : destroy_listeners_)
				listener(res, id);
	}

	void ResourceTracker::OnInitCommandQueue(command_queue *queue)
	{
		std::unique_lock<std::shared_mutex> lock(mutex_);
		if (std::find(queues_.begin(), queues_.end(), queue) == queues_.end())
			queues_.push_back(queue);
	}
	void ResourceTracker::OnDestroyCommandQueue(command_queue *queue)
	{
		std::unique_lock<std::shared_mutex> lock(mutex_);
		queues_.erase(std::remove(queues_.begin(), queues_.end(), queue), queues_.end());
	}

	resource ResourceTracker::ResolveView(resource_view view) const
	{
		if (view == 0)
			return {};
		{
			std::shared_lock<std::shared_mutex> lock(mutex_);
			if (const auto it = view_to_resource_.find(view.handle); it != view_to_resource_.end())
				return resource { it->second };
		}
		// Not seen through init_resource_view (view created before the add-on was loaded): ask the runtime
		return device_->get_resource_from_view(view);
	}

	TrackedResource *ResourceTracker::FindOrRegisterLocked(resource resource)
	{
		if (const auto it = resources_.find(resource.handle); it != resources_.end())
			return &it->second;
		if (ignored_.count(resource.handle) != 0)
			return nullptr;   // one of ours; it is not part of the game's frame

		// Lazily register resources that show up through usage only (e.g. copy destinations,
		// or resources created before the add-on was loaded)
		const resource_desc desc = device_->get_resource_desc(resource);
		if (desc.type != resource_type::texture_2d && desc.type != resource_type::surface)
			return nullptr;

		TrackedResource &res = resources_[resource.handle];
		res.id = next_id_++;
		res.handle = resource;
		res.desc = desc;
		res.created_frame = frame_index_;
		res.late_registered = true;
		res.is_backbuffer = backbuffers_.count(resource.handle) != 0;
		counters_.created++;
		counters_.alive = resources_.size();
		log::Verbose("Resource discovered #%03u %ux%u %s (late registration)", res.id, desc.texture.width, desc.texture.height, FormatName(desc.texture.format));
		return &res;
	}

	void ResourceTracker::ApplyEventLocked(TrackedResource &res, const AccessEvent &event, uint32_t event_index)
	{
		FrameStats &stats = res.current;
		if (event.is_write)
		{
			if (stats.writes == 0)
				stats.first_write_event = event_index;
			stats.writes += event.count;
			stats.last_write_event = event_index;
			stats.last_write_kind = event.kind;
			if (event.kind == WriteKind::Draw)
				stats.draw_calls += event.count;
			else if (event.kind == WriteKind::Clear)
				stats.cleared = true;

			// Which part of the target the draw covered; see ActiveRect for why this matters
			if (event.has_viewport)
			{
				const ActiveRect rect = ClampViewport(event.viewport, res.desc);
				if (rect.IsValid())
					res.current_active_rect = rect;
			}
		}
		else
		{
			stats.reads += event.count;
			stats.last_read_event = event_index;
		}

		if (timeline_current_.size() < kMaxTimelineEntries)
			timeline_current_.push_back(TimelineEntry { event_index, res.id, event.kind, event.count, event.is_write });
	}

	void ResourceTracker::Merge(CommandListState &state)
	{
		if (state.events.empty())
			return;

		std::unique_lock<std::shared_mutex> lock(mutex_);
		counters_.merges++;
		for (const AccessEvent &event : state.events)
		{
			const uint32_t index = ++event_counter_;
			if (TrackedResource *const res = FindOrRegisterLocked(event.resource))
				ApplyEventLocked(*res, event, index);
		}
		state.events.clear();
	}

	void ResourceTracker::EndFrame()
	{
		std::unique_lock<std::shared_mutex> lock(mutex_);

		for (auto &[handle, res] : resources_)
		{
			if (res.is_backbuffer && res.current.writes > 0)
			{
				const uint32_t index = ++event_counter_;
				const AccessEvent present { res.handle, WriteKind::Present, 1, true };
				ApplyEventLocked(res, present, index);
			}

			res.last = res.current;
			res.current = FrameStats {};
			if (res.current_active_rect.IsValid())
			{
				res.active_rect = res.current_active_rect;
				res.current_active_rect = ActiveRect {};
			}
			if (res.last.writes > 0)
			{
				res.last_written_frame = frame_index_;
				res.total_writes += res.last.writes;
				res.frames_written++;
			}
			if (res.last.reads > 0)
			{
				res.last_read_frame = frame_index_;
				res.total_reads += res.last.reads;
			}
		}

		counters_.events_last_frame = event_counter_;
		timeline_last_.swap(timeline_current_);
		timeline_current_.clear();
		event_counter_ = 0;
		frame_index_++;
	}

	uint64_t ResourceTracker::FrameIndex() const
	{
		std::shared_lock<std::shared_mutex> lock(mutex_);
		return frame_index_;
	}

	bool ResourceTracker::Get(uint32_t id, TrackedResource &out) const
	{
		std::shared_lock<std::shared_mutex> lock(mutex_);
		for (const auto &[handle, res] : resources_)
		{
			if (res.id == id)
			{
				out = res;
				return true;
			}
		}
		return false;
	}

	bool ResourceTracker::GetByHandle(resource resource, TrackedResource &out) const
	{
		std::shared_lock<std::shared_mutex> lock(mutex_);
		if (const auto it = resources_.find(resource.handle); it != resources_.end())
		{
			out = it->second;
			return true;
		}
		return false;
	}

	uint32_t ResourceTracker::IdOf(resource resource) const
	{
		std::shared_lock<std::shared_mutex> lock(mutex_);
		if (const auto it = resources_.find(resource.handle); it != resources_.end())
			return it->second.id;
		return 0;
	}

	bool ResourceTracker::IsBackbuffer(resource resource) const
	{
		std::shared_lock<std::shared_mutex> lock(mutex_);
		return backbuffers_.count(resource.handle) != 0;
	}

	resource_desc ResourceTracker::BackbufferDesc() const
	{
		std::shared_lock<std::shared_mutex> lock(mutex_);
		return backbuffer_desc_;
	}

	std::vector<TrackedResource> ResourceTracker::Snapshot() const
	{
		std::shared_lock<std::shared_mutex> lock(mutex_);
		std::vector<TrackedResource> result;
		result.reserve(resources_.size());
		for (const auto &[handle, res] : resources_)
			result.push_back(res);
		std::sort(result.begin(), result.end(), [](const TrackedResource &a, const TrackedResource &b) { return a.id < b.id; });
		return result;
	}

	std::vector<TimelineEntry> ResourceTracker::TimelineSnapshot() const
	{
		std::shared_lock<std::shared_mutex> lock(mutex_);
		return timeline_last_;
	}

	std::vector<command_queue *> ResourceTracker::Queues() const
	{
		std::shared_lock<std::shared_mutex> lock(mutex_);
		return queues_;
	}

	ResourceTracker::Counters ResourceTracker::GetCounters() const
	{
		std::shared_lock<std::shared_mutex> lock(mutex_);
		return counters_;
	}

	void ResourceTracker::Forget(resource resource)
	{
		if (resource == 0)
			return;
		std::unique_lock<std::shared_mutex> lock(mutex_);
		if (const auto it = resources_.find(resource.handle); it != resources_.end())
		{
			resources_.erase(it);
			counters_.alive = resources_.size();
		}
		ignored_.insert(resource.handle);
	}

	void ResourceTracker::SetWriteObserver(WriteObserver observer, const std::atomic<bool> *enabled)
	{
		write_observer_ = std::move(observer);
		write_enabled_ = enabled;
	}

	void ResourceTracker::AddDestroyListener(DestroyListener listener)
	{
		std::unique_lock<std::shared_mutex> lock(mutex_);
		destroy_listeners_.push_back(std::move(listener));
	}

	// ---------------------------------------------------------------------------------------
	// ReShade event callbacks (command list side)
	// ---------------------------------------------------------------------------------------
	namespace
	{
		inline ResourceTracker *TrackerOf(device *device)
		{
			DeviceContext *const ctx = DeviceContext::From(device);
			return ctx != nullptr ? ctx->tracker.get() : nullptr;
		}
		inline ResourceTracker *TrackerOf(command_list *cmd_list)
		{
			return TrackerOf(cmd_list->get_device());
		}

		void AttachOwner(CommandListState &state, command_list *cmd_list)
		{
			if (state.owner == nullptr)
				state.owner = cmd_list;
			if (state.tracker == nullptr && cmd_list != nullptr)
				state.tracker = TrackerOf(cmd_list);
		}

		void on_init_command_list(command_list *cmd_list)
		{
			// In D3D11 the immediate context is both queue and command list and may receive both init events
			if (cmd_list->get_private_data<CommandListState>() == nullptr)
				cmd_list->create_private_data<CommandListState>();
			if (CommandListState *const state = cmd_list->get_private_data<CommandListState>())
				AttachOwner(*state, cmd_list);
		}
		void on_destroy_command_list(command_list *cmd_list)
		{
			if (cmd_list->get_private_data<CommandListState>() != nullptr)
				cmd_list->destroy_private_data<CommandListState>();
		}
		void on_init_command_queue(command_queue *queue)
		{
			if (queue->get_private_data<CommandListState>() == nullptr)
				queue->create_private_data<CommandListState>();
			if (CommandListState *const state = queue->get_private_data<CommandListState>())
				AttachOwner(*state, queue->get_immediate_command_list());
			if (ResourceTracker *const tracker = TrackerOf(queue->get_device()))
				tracker->OnInitCommandQueue(queue);
		}
		void on_destroy_command_queue(command_queue *queue)
		{
			if (ResourceTracker *const tracker = TrackerOf(queue->get_device()))
				tracker->OnDestroyCommandQueue(queue);
			if (queue->get_private_data<CommandListState>() != nullptr)
				queue->destroy_private_data<CommandListState>();
		}

		void on_init_resource(device *device, const resource_desc &desc, const subresource_data *, resource_usage, resource resource)
		{
			if (ResourceTracker *const tracker = TrackerOf(device))
				tracker->OnInitResource(desc, resource);
		}
		void on_destroy_resource(device *device, resource resource)
		{
			if (ResourceTracker *const tracker = TrackerOf(device))
				tracker->OnDestroyResource(resource);
		}
		void on_init_resource_view(device *device, resource resource, resource_usage, const resource_view_desc &, resource_view view)
		{
			if (ResourceTracker *const tracker = TrackerOf(device))
				tracker->OnInitResourceView(resource, view);
		}
		void on_destroy_resource_view(device *device, resource_view view)
		{
			if (ResourceTracker *const tracker = TrackerOf(device))
				tracker->OnDestroyResourceView(view);
		}

		// ---- render target bindings -----------------------------------------------------------
		void on_bind_render_targets_and_depth_stencil(command_list *cmd_list, uint32_t count, const resource_view *rtvs, resource_view dsv)
		{
			CommandListState *const state = cmd_list->get_private_data<CommandListState>();
			ResourceTracker *const tracker = TrackerOf(cmd_list);
			if (state == nullptr || tracker == nullptr)
				return;
			AttachOwner(*state, cmd_list);   // in case the list was created before the device context

			state->render_target_count = 0;
			for (uint32_t i = 0; i < count && state->render_target_count < CommandListState::kMaxRenderTargets; ++i)
			{
				const resource res = tracker->ResolveView(rtvs[i]);
				if (res != 0)
					state->render_targets[state->render_target_count++] = res;
			}
			state->depth_stencil = tracker->ResolveView(dsv);
		}
		bool on_begin_render_pass(command_list *cmd_list, uint32_t count, const render_pass_render_target_desc *rts, const render_pass_depth_stencil_desc *ds, render_pass_flags)
		{
			CommandListState *const state = cmd_list->get_private_data<CommandListState>();
			ResourceTracker *const tracker = TrackerOf(cmd_list);
			if (state == nullptr || tracker == nullptr)
				return false;

			state->render_target_count = 0;
			for (uint32_t i = 0; i < count && state->render_target_count < CommandListState::kMaxRenderTargets; ++i)
			{
				const resource res = tracker->ResolveView(rts[i].view);
				if (res == 0)
					continue;
				state->render_targets[state->render_target_count++] = res;
				if (rts[i].load_op == render_pass_load_op::clear)
					state->Add(res, WriteKind::Clear, true);
			}
			state->depth_stencil = ds != nullptr ? tracker->ResolveView(ds->view) : resource {};
			if (ds != nullptr && state->depth_stencil != 0 && (ds->depth_load_op == render_pass_load_op::clear || ds->stencil_load_op == render_pass_load_op::clear))
				state->Add(state->depth_stencil, WriteKind::Clear, true);
			state->in_render_pass = true;
			return false;
		}
		bool on_end_render_pass(command_list *cmd_list)
		{
			if (CommandListState *const state = cmd_list->get_private_data<CommandListState>())
				state->in_render_pass = false;
			return false;
		}

		// ---- draws -----------------------------------------------------------------------
		inline void RecordDraw(command_list *cmd_list, uint32_t draw_count)
		{
			CommandListState *const state = cmd_list->get_private_data<CommandListState>();
			if (state == nullptr)
				return;
			// Only a draw says anything about the covered area, so only a draw carries the viewport
			for (uint32_t i = 0; i < state->render_target_count; ++i)
				state->Add(state->render_targets[i], WriteKind::Draw, true, draw_count, true);
			if (state->depth_stencil != 0)
				state->Add(state->depth_stencil, WriteKind::Draw, true, draw_count, true);
		}
		void on_bind_viewports(command_list *cmd_list, uint32_t first, uint32_t count, const viewport *viewports)
		{
			// Only viewport 0 is of interest: it is the one a fullscreen or scene pass renders through.
			if (first != 0 || count == 0 || viewports == nullptr)
				return;
			if (CommandListState *const state = cmd_list->get_private_data<CommandListState>())
			{
				state->viewport = viewports[0];
				state->viewport_set = true;
			}
		}

		bool on_draw(command_list *cmd_list, uint32_t, uint32_t, uint32_t, uint32_t)
		{
			RecordDraw(cmd_list, 1);
			return false;
		}
		bool on_draw_indexed(command_list *cmd_list, uint32_t, uint32_t, uint32_t, int32_t, uint32_t)
		{
			RecordDraw(cmd_list, 1);
			return false;
		}
		bool on_dispatch_mesh(command_list *cmd_list, uint32_t, uint32_t, uint32_t)
		{
			RecordDraw(cmd_list, 1);
			return false;
		}
		bool on_draw_or_dispatch_indirect(command_list *cmd_list, indirect_command type, resource, uint64_t, uint32_t draw_count, uint32_t)
		{
			if (type == indirect_command::dispatch || type == indirect_command::dispatch_rays)
				return false;
			RecordDraw(cmd_list, std::max(draw_count, 1u));
			return false;
		}

		// ---- clears ----------------------------------------------------------------------
		bool on_clear_render_target_view(command_list *cmd_list, resource_view rtv, const float[4], uint32_t, const rect *)
		{
			CommandListState *const state = cmd_list->get_private_data<CommandListState>();
			ResourceTracker *const tracker = TrackerOf(cmd_list);
			if (state != nullptr && tracker != nullptr)
				state->Add(tracker->ResolveView(rtv), WriteKind::Clear, true);
			return false;
		}
		bool on_clear_depth_stencil_view(command_list *cmd_list, resource_view dsv, const float *, const uint8_t *, uint32_t, const rect *)
		{
			CommandListState *const state = cmd_list->get_private_data<CommandListState>();
			ResourceTracker *const tracker = TrackerOf(cmd_list);
			if (state != nullptr && tracker != nullptr)
				state->Add(tracker->ResolveView(dsv), WriteKind::Clear, true);
			return false;
		}
		bool on_clear_unordered_access_view_uint(command_list *cmd_list, resource_view uav, const uint32_t[4], uint32_t, const rect *)
		{
			CommandListState *const state = cmd_list->get_private_data<CommandListState>();
			ResourceTracker *const tracker = TrackerOf(cmd_list);
			if (state != nullptr && tracker != nullptr)
				state->Add(tracker->ResolveView(uav), WriteKind::Clear, true);
			return false;
		}
		bool on_clear_unordered_access_view_float(command_list *cmd_list, resource_view uav, const float[4], uint32_t, const rect *)
		{
			CommandListState *const state = cmd_list->get_private_data<CommandListState>();
			ResourceTracker *const tracker = TrackerOf(cmd_list);
			if (state != nullptr && tracker != nullptr)
				state->Add(tracker->ResolveView(uav), WriteKind::Clear, true);
			return false;
		}

		// ---- copies / resolves -----------------------------------------------------------
		bool on_copy_resource(command_list *cmd_list, resource source, resource dest)
		{
			if (CommandListState *const state = cmd_list->get_private_data<CommandListState>())
			{
				state->Add(source, WriteKind::Copy, false);
				state->Add(dest, WriteKind::Copy, true);
			}
			return false;
		}
		bool on_copy_texture_region(command_list *cmd_list, resource source, uint32_t, const subresource_box *, resource dest, uint32_t, const subresource_box *, filter_mode)
		{
			if (CommandListState *const state = cmd_list->get_private_data<CommandListState>())
			{
				state->Add(source, WriteKind::Copy, false);
				state->Add(dest, WriteKind::Copy, true);
			}
			return false;
		}
		bool on_copy_buffer_to_texture(command_list *cmd_list, resource, uint64_t, uint32_t, uint32_t, resource dest, uint32_t, const subresource_box *)
		{
			if (CommandListState *const state = cmd_list->get_private_data<CommandListState>())
				state->Add(dest, WriteKind::Copy, true);
			return false;
		}
		bool on_copy_texture_to_buffer(command_list *cmd_list, resource source, uint32_t, const subresource_box *, resource, uint64_t, uint32_t, uint32_t)
		{
			if (CommandListState *const state = cmd_list->get_private_data<CommandListState>())
				state->Add(source, WriteKind::Copy, false);
			return false;
		}
		bool on_resolve_texture_region(command_list *cmd_list, resource source, uint32_t, const subresource_box *, resource dest, uint32_t, uint32_t, uint32_t, uint32_t, format)
		{
			if (CommandListState *const state = cmd_list->get_private_data<CommandListState>())
			{
				state->Add(source, WriteKind::Resolve, false);
				state->Add(dest, WriteKind::Resolve, true);
			}
			return false;
		}
		bool on_generate_mipmaps(command_list *cmd_list, resource_view srv)
		{
			CommandListState *const state = cmd_list->get_private_data<CommandListState>();
			ResourceTracker *const tracker = TrackerOf(cmd_list);
			if (state != nullptr && tracker != nullptr)
				state->Add(tracker->ResolveView(srv), WriteKind::Copy, true);
			return false;
		}

		// ---- barriers: compute write hints (D3D12 / Vulkan) --------------------------------
		void on_barrier(command_list *cmd_list, uint32_t count, const resource *resources, const resource_usage *old_states, const resource_usage *new_states)
		{
			CommandListState *const state = cmd_list->get_private_data<CommandListState>();
			if (state == nullptr)
				return;
			for (uint32_t i = 0; i < count; ++i)
			{
				if ((old_states[i] & resource_usage::unordered_access) != 0 && (new_states[i] & resource_usage::unordered_access) == 0)
					state->Add(resources[i], WriteKind::Compute, true);
			}
		}

		// ---- command list lifetime -------------------------------------------------------
		void on_reset_command_list(command_list *cmd_list)
		{
			if (CommandListState *const state = cmd_list->get_private_data<CommandListState>())
				state->Reset();
		}
		void on_execute_command_list(command_queue *queue, command_list *cmd_list)
		{
			CommandListState *const state = cmd_list->get_private_data<CommandListState>();
			ResourceTracker *const tracker = TrackerOf(queue->get_device());
			if (state != nullptr && tracker != nullptr)
				tracker->Merge(*state);
		}
		void on_execute_secondary_command_list(command_list *cmd_list, command_list *secondary_cmd_list)
		{
			// D3D11: called after FinishCommandList (cmd_list = new command list object, secondary = deferred context)
			//        and before ExecuteCommandList (cmd_list = immediate context, secondary = command list object).
			// In both cases the events flow from 'secondary' into 'cmd_list'.
			CommandListState *const target = cmd_list->get_private_data<CommandListState>();
			CommandListState *const source = secondary_cmd_list->get_private_data<CommandListState>();
			if (target == nullptr || source == nullptr || target == source)
				return;
			for (const AccessEvent &event : source->events)
				target->Add(event.resource, event.kind, event.is_write, event.count);
			// Bundles / command lists are re-usable in D3D12, but a D3D11 command list is executed once;
			// events are kept so that repeated D3D12 bundle execution is counted every time.
			if (cmd_list->get_device()->get_api() != device_api::d3d12)
				source->events.clear();
		}
	}

	void RegisterTrackerEvents()
	{
		reshade::register_event<reshade::addon_event::init_command_list>(on_init_command_list);
		reshade::register_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
		reshade::register_event<reshade::addon_event::init_command_queue>(on_init_command_queue);
		reshade::register_event<reshade::addon_event::destroy_command_queue>(on_destroy_command_queue);

		reshade::register_event<reshade::addon_event::init_resource>(on_init_resource);
		reshade::register_event<reshade::addon_event::destroy_resource>(on_destroy_resource);
		reshade::register_event<reshade::addon_event::init_resource_view>(on_init_resource_view);
		reshade::register_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);

		reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_render_targets_and_depth_stencil);
		reshade::register_event<reshade::addon_event::begin_render_pass>(on_begin_render_pass);
		reshade::register_event<reshade::addon_event::end_render_pass>(on_end_render_pass);

		reshade::register_event<reshade::addon_event::bind_viewports>(on_bind_viewports);
		reshade::register_event<reshade::addon_event::draw>(on_draw);
		reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
		reshade::register_event<reshade::addon_event::dispatch_mesh>(on_dispatch_mesh);
		reshade::register_event<reshade::addon_event::draw_or_dispatch_indirect>(on_draw_or_dispatch_indirect);

		reshade::register_event<reshade::addon_event::clear_render_target_view>(on_clear_render_target_view);
		reshade::register_event<reshade::addon_event::clear_depth_stencil_view>(on_clear_depth_stencil_view);
		reshade::register_event<reshade::addon_event::clear_unordered_access_view_uint>(on_clear_unordered_access_view_uint);
		reshade::register_event<reshade::addon_event::clear_unordered_access_view_float>(on_clear_unordered_access_view_float);

		reshade::register_event<reshade::addon_event::copy_resource>(on_copy_resource);
		reshade::register_event<reshade::addon_event::copy_texture_region>(on_copy_texture_region);
		reshade::register_event<reshade::addon_event::copy_buffer_to_texture>(on_copy_buffer_to_texture);
		reshade::register_event<reshade::addon_event::copy_texture_to_buffer>(on_copy_texture_to_buffer);
		reshade::register_event<reshade::addon_event::resolve_texture_region>(on_resolve_texture_region);
		reshade::register_event<reshade::addon_event::generate_mipmaps>(on_generate_mipmaps);

		reshade::register_event<reshade::addon_event::barrier>(on_barrier);

		reshade::register_event<reshade::addon_event::reset_command_list>(on_reset_command_list);
		reshade::register_event<reshade::addon_event::execute_command_list>(on_execute_command_list);
		reshade::register_event<reshade::addon_event::execute_secondary_command_list>(on_execute_secondary_command_list);
	}

	void UnregisterTrackerEvents()
	{
		reshade::unregister_event<reshade::addon_event::init_command_list>(on_init_command_list);
		reshade::unregister_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
		reshade::unregister_event<reshade::addon_event::init_command_queue>(on_init_command_queue);
		reshade::unregister_event<reshade::addon_event::destroy_command_queue>(on_destroy_command_queue);

		reshade::unregister_event<reshade::addon_event::init_resource>(on_init_resource);
		reshade::unregister_event<reshade::addon_event::destroy_resource>(on_destroy_resource);
		reshade::unregister_event<reshade::addon_event::init_resource_view>(on_init_resource_view);
		reshade::unregister_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);

		reshade::unregister_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_render_targets_and_depth_stencil);
		reshade::unregister_event<reshade::addon_event::begin_render_pass>(on_begin_render_pass);
		reshade::unregister_event<reshade::addon_event::end_render_pass>(on_end_render_pass);

		reshade::unregister_event<reshade::addon_event::bind_viewports>(on_bind_viewports);
		reshade::unregister_event<reshade::addon_event::draw>(on_draw);
		reshade::unregister_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
		reshade::unregister_event<reshade::addon_event::dispatch_mesh>(on_dispatch_mesh);
		reshade::unregister_event<reshade::addon_event::draw_or_dispatch_indirect>(on_draw_or_dispatch_indirect);

		reshade::unregister_event<reshade::addon_event::clear_render_target_view>(on_clear_render_target_view);
		reshade::unregister_event<reshade::addon_event::clear_depth_stencil_view>(on_clear_depth_stencil_view);
		reshade::unregister_event<reshade::addon_event::clear_unordered_access_view_uint>(on_clear_unordered_access_view_uint);
		reshade::unregister_event<reshade::addon_event::clear_unordered_access_view_float>(on_clear_unordered_access_view_float);

		reshade::unregister_event<reshade::addon_event::copy_resource>(on_copy_resource);
		reshade::unregister_event<reshade::addon_event::copy_texture_region>(on_copy_texture_region);
		reshade::unregister_event<reshade::addon_event::copy_buffer_to_texture>(on_copy_buffer_to_texture);
		reshade::unregister_event<reshade::addon_event::copy_texture_to_buffer>(on_copy_texture_to_buffer);
		reshade::unregister_event<reshade::addon_event::resolve_texture_region>(on_resolve_texture_region);
		reshade::unregister_event<reshade::addon_event::generate_mipmaps>(on_generate_mipmaps);

		reshade::unregister_event<reshade::addon_event::barrier>(on_barrier);

		reshade::unregister_event<reshade::addon_event::reset_command_list>(on_reset_command_list);
		reshade::unregister_event<reshade::addon_event::execute_command_list>(on_execute_command_list);
		reshade::unregister_event<reshade::addon_event::execute_secondary_command_list>(on_execute_secondary_command_list);
	}
}
