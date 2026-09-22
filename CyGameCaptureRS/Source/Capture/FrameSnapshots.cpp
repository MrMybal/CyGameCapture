// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "FrameSnapshots.hpp"
#include "ResourceTracker/FormatUtils.hpp"
#include "Addon/Log.hpp"

#include <algorithm>
#include <Windows.h>

using namespace reshade::api;

namespace cygc
{
	FrameSnapshots::Guard::Guard(FrameSnapshots &owner) :
		owner_(owner),
		lock_(owner.mutex_)
	{
		owner_.lock_owner_.store(GetCurrentThreadId(), std::memory_order_release);
	}

	FrameSnapshots::Guard::~Guard()
	{
		owner_.DrainDestroyedLocked();
		owner_.lock_owner_.store(0, std::memory_order_release);
	}

	void FrameSnapshots::DrainDestroyedLocked()
	{
		// Removing an entry destroys its texture, which is a runtime call too and may report more
		// destructions: keep going until nothing new came in.
		while (!destroyed_while_locked_.empty())
		{
			const resource resource = destroyed_while_locked_.back();
			destroyed_while_locked_.pop_back();
			for (size_t i = entries_.size(); i-- > 0;)
				if (entries_[i].source == resource)
					RemoveLocked(entries_[i]);
		}
	}

	FrameSnapshots::FrameSnapshots(device *device) :
		device_(device)
	{
	}

	FrameSnapshots::~FrameSnapshots()
	{
		const Guard guard(*this);
		active_.store(false);
		for (Entry &entry : entries_)
			DestroyTextureLocked(entry);
		entries_.clear();
	}

	bool FrameSnapshots::IsSupported(std::string *why) const
	{
		if (device_->get_api() == device_api::d3d11)
			return true;
		if (why != nullptr)
			*why = "Capturing at a moment of the frame is only available on D3D11 for now: D3D12 records its "
				"command lists in parallel and submits them later, so the order of the writes is not known "
				"while they are recorded.";
		return false;
	}

	void FrameSnapshots::SetResourceNotice(ResourceNotice on_created, ResourceNotice on_destroyed)
	{
		on_created_ = std::move(on_created);
		on_destroyed_ = std::move(on_destroyed);
	}

	FrameSnapshots::Entry *FrameSnapshots::FindLocked(resource source, uint32_t after_write)
	{
		for (Entry &entry : entries_)
			if (entry.source == source && entry.after_write == after_write)
				return &entry;
		return nullptr;
	}

	FrameSnapshots::Entry *FrameSnapshots::FindLocked(uint64_t ticket)
	{
		for (Entry &entry : entries_)
			if (entry.ticket == ticket)
				return &entry;
		return nullptr;
	}

	const FrameSnapshots::Entry *FrameSnapshots::FindLocked(uint64_t ticket) const
	{
		for (const Entry &entry : entries_)
			if (entry.ticket == ticket)
				return &entry;
		return nullptr;
	}

	void FrameSnapshots::RemoveLocked(Entry &entry)
	{
		DestroyTextureLocked(entry);
		entries_.erase(entries_.begin() + (&entry - entries_.data()));
		active_.store(!entries_.empty(), std::memory_order_release);
	}

	bool FrameSnapshots::EnsureTextureLocked(Entry &entry, const resource_desc &source_desc)
	{
		// Same size, mips, layers and samples as the buffer, in its typeless family: that is what a
		// whole-resource copy requires, and a typeless texture can then be viewed through whichever typed
		// format the reader needs.
		const format typeless = format_to_typeless(source_desc.texture.format);
		if (entry.texture != 0 &&
			entry.texture_desc.texture.width == source_desc.texture.width &&
			entry.texture_desc.texture.height == source_desc.texture.height &&
			entry.texture_desc.texture.depth_or_layers == source_desc.texture.depth_or_layers &&
			entry.texture_desc.texture.levels == source_desc.texture.levels &&
			entry.texture_desc.texture.format == typeless)
			return true;

		DestroyTextureLocked(entry);
		const resource_desc desc(resource_type::texture_2d, source_desc.texture.width, source_desc.texture.height,
			source_desc.texture.depth_or_layers, source_desc.texture.levels, typeless, 1,
			memory_heap::default_, resource_usage::copy_dest | resource_usage::copy_source | resource_usage::shader_resource);
		if (!device_->create_resource(desc, nullptr, resource_usage::shader_resource, &entry.texture))
		{
			entry.texture = {};
			return false;
		}
		if (on_created_)
			on_created_(entry.texture);
		entry.texture_desc = device_->get_resource_desc(entry.texture);
		entry.valid = false;
		return true;
	}

	void FrameSnapshots::DestroyTextureLocked(Entry &entry)
	{
		if (entry.texture == 0)
			return;
		if (on_destroyed_)
			on_destroyed_(entry.texture);
		device_->destroy_resource(entry.texture);
		entry.texture = {};
		entry.texture_desc = {};
		entry.valid = false;
	}

	void FrameSnapshots::TakeLocked(Entry &entry, command_list *cmd_list)
	{
		if (entry.texture == 0 || cmd_list == nullptr)
			return;
		// D3D11 has no barriers to record; this is also why the class is D3D11 only (see the header).
		cmd_list->copy_resource(entry.source, entry.texture);
		entry.taken = true;
		entry.valid = true;
	}

	uint64_t FrameSnapshots::Acquire(const TrackedResource &source, uint32_t after_write, std::string *error)
	{
		auto fail = [&](const std::string &message) -> uint64_t {
			if (error != nullptr)
				*error = message;
			return 0;
		};
		if (!IsSupported(error))
			return 0;
		if (after_write == 0)
			return fail("A snapshot needs at least one write to have happened");
		if (source.IsMultisampled())
			return fail("Multisampled buffers cannot be captured at a moment of the frame yet (the copy would need a resolve)");
		if (!source.IsTexture2D())
			return fail("Only 2D textures can be captured at a moment of the frame");

		const Guard guard(*this);
		if (Entry *const existing = FindLocked(source.handle, after_write))
		{
			existing->users++;
			return existing->ticket;
		}

		Entry entry;
		entry.ticket = next_ticket_++;
		entry.source = source.handle;
		entry.after_write = after_write;
		entry.users = 1;
		if (!EnsureTextureLocked(entry, source.desc))
			return fail(std::string("Could not create the snapshot texture (") + FormatName(source.Format()) + ")");
		entries_.push_back(entry);
		active_.store(true, std::memory_order_release);
		log::Info("Snapshot of #%03u after write %u of each frame", source.id, after_write);
		return entries_.back().ticket;
	}

	void FrameSnapshots::Release(uint64_t ticket)
	{
		if (ticket == 0)
			return;
		const Guard guard(*this);
		Entry *const entry = FindLocked(ticket);
		if (entry == nullptr || --entry->users > 0)
			return;
		RemoveLocked(*entry);
	}

	bool FrameSnapshots::Proxy(uint64_t ticket, const TrackedResource &source, TrackedResource &out) const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		const Entry *const entry = FindLocked(ticket);
		if (entry == nullptr || !entry->valid || entry->source != source.handle)
			return false;
		// The buffer's own identity and active area, the snapshot's handle and description. The usage has
		// no render target / depth / UAV bit, so readers treat it as sitting in shader_resource, which is
		// the state it is created in and stays in between copies.
		out = source;
		out.handle = entry->texture;
		// The buffer's typed format (readers pick their views from it; the texture itself is its typeless
		// family, which accepts them), the snapshot's usage and single sample.
		out.desc.usage = entry->texture_desc.usage;
		out.desc.heap = entry->texture_desc.heap;
		out.desc.texture.samples = 1;
		out.is_backbuffer = false;
		return true;
	}

	void FrameSnapshots::OnWrite(command_list *cmd_list, resource target, uint32_t count)
	{
		// Only the immediate context records in the frame's order; anything else is ignored rather than
		// miscounted. Checked before the lock so that other threads never wait on it.
		if (cmd_list == nullptr || cmd_list != immediate_.load(std::memory_order_acquire))
			return;

		const Guard guard(*this);
		for (Entry &entry : entries_)
		{
			if (entry.source != target)
				continue;
			// The write about to happen is number `writes + 1`: once `after_write` of them are done, this
			// is the moment to copy, before the draw reaches the buffer.
			if (!entry.taken && entry.writes >= entry.after_write)
				TakeLocked(entry, cmd_list);
			entry.writes += count;
		}
	}

	void FrameSnapshots::EndFrame(command_queue *queue)
	{
		if (queue == nullptr)
			return;
		command_list *const immediate = queue->get_immediate_command_list();
		immediate_.store(immediate, std::memory_order_release);

		const Guard guard(*this);
		for (Entry &entry : entries_)
		{
			// Fewer writes than asked for this frame: the moment asked for is the end of the frame
			if (!entry.taken)
			{
				const resource_desc desc = device_->get_resource_desc(entry.source);
				if (EnsureTextureLocked(entry, desc))
					TakeLocked(entry, immediate);
			}
			entry.writes = 0;
			entry.taken = false;
		}
	}

	void FrameSnapshots::OnResourceDestroyed(resource resource)
	{
		// Reported from inside one of our own runtime calls: only note it, see Guard
		if (lock_owner_.load(std::memory_order_acquire) == GetCurrentThreadId())
		{
			destroyed_while_locked_.push_back(resource);
			return;
		}
		// Dropped at once: their users learn about the loss from the tracker like for any other source,
		// and their late Release finds no ticket, which is harmless.
		const Guard guard(*this);
		destroyed_while_locked_.push_back(resource);
	}
}
