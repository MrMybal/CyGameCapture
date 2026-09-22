// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "ThumbnailGrid.hpp"
#include "ResourceTracker/FormatUtils.hpp"
#include "Addon/Log.hpp"

using namespace reshade::api;

namespace cygc
{
	namespace
	{
		// Every thumbnail lands in the same plain format, so one pipeline serves the whole sheet
		// whatever the buffers are made of.
		constexpr format kThumbnailFormat = format::r8g8b8a8_unorm;
	}

	ThumbnailGrid::ThumbnailGrid(device *device) :
		device_(device)
	{
	}

	ThumbnailGrid::~ThumbnailGrid()
	{
		Destroy();
	}

	void ThumbnailGrid::SetResourceNotice(ResourceNotice on_created, ResourceNotice on_destroyed)
	{
		on_created_ = std::move(on_created);
		on_destroyed_ = std::move(on_destroyed);
	}

	bool ThumbnailGrid::EnsureSlot(Slot &slot)
	{
		if (slot.texture != 0)
			return true;

		const resource_desc desc(kThumbnailWidth, kThumbnailHeight, 1, 1, kThumbnailFormat, 1,
			memory_heap::default_, resource_usage::render_target | resource_usage::shader_resource);

		if (!device_->create_resource(desc, nullptr, resource_usage::shader_resource, &slot.texture))
		{
			slot.texture = {};
			return false;
		}
		if (on_created_)
			on_created_(slot.texture);

		if (!device_->create_resource_view(slot.texture, resource_usage::render_target, resource_view_desc(kThumbnailFormat), &slot.target) ||
			!device_->create_resource_view(slot.texture, resource_usage::shader_resource, resource_view_desc(kThumbnailFormat), &slot.view))
		{
			DestroySlot(slot);
			return false;
		}
		return true;
	}

	void ThumbnailGrid::DestroySlot(Slot &slot)
	{
		DestroyInput(device_, slot.input, on_destroyed_);
		if (slot.target != 0)
			device_->destroy_resource_view(slot.target);
		if (slot.view != 0)
			device_->destroy_resource_view(slot.view);
		if (slot.texture != 0)
		{
			if (on_destroyed_)
				on_destroyed_(slot.texture);
			device_->destroy_resource(slot.texture);
		}
		slot = Slot {};
	}

	void ThumbnailGrid::Update(command_queue *queue, const std::vector<TrackedResource> &entries, size_t first)
	{
		if (queue == nullptr)
			return;

		if (!blit_.IsValid() && !CreateBlitPass(device_, kThumbnailFormat, blit_, pass_error_))
			return;   // reported once through ReasonFor; the single preview still works

		if (slots_.size() < kThumbnailBudget)
			slots_.resize(kThumbnailBudget);

		command_list *const cmd_list = queue->get_immediate_command_list();

		for (size_t i = 0; i < slots_.size(); ++i)
		{
			Slot &slot = slots_[i];
			const size_t index = first + i;
			if (index >= entries.size())
			{
				// Out of the page: release the readable copy, which is full resolution and the
				// expensive part, but keep the small texture for the next page.
				DestroyInput(device_, slot.input, on_destroyed_);
				slot.resource_id = 0;
				slot.reason = nullptr;
				continue;
			}

			const TrackedResource &res = entries[index];
			if (slot.resource_id != res.id)
				DestroyInput(device_, slot.input, on_destroyed_);
			slot.resource_id = res.id;
			slot.reason = nullptr;

			if (res.IsMultisampled())
			{
				slot.reason = "multisampled";
				continue;
			}
			if (IsBlockCompressedFormat(res.Format()))
			{
				slot.reason = "compressed";
				continue;
			}
			if (!EnsureSlot(slot))
			{
				slot.reason = "no texture";
				continue;
			}
			if (!PrepareInput(device_, slot.input, res, cmd_list, on_created_))
			{
				slot.reason = "not readable";
				continue;
			}

			// Only the part of the buffer the game actually rendered into, so a target left at its
			// maximum size by dynamic resolution shows its picture rather than its black margin.
			const SourceRect rect = MakeSourceRect(res.ActiveArea(), res.Width(), res.Height());
			const BlitConstants constants { { rect.scale_x, rect.scale_y, rect.offset_x, rect.offset_y } };

			const bool direct = slot.input.copy == 0;
			const resource_usage source_state = GuessSourceState(res);
			if (direct)
				cmd_list->barrier(res.handle, source_state, resource_usage::shader_resource);
			cmd_list->barrier(slot.texture, resource_usage::shader_resource, resource_usage::render_target);

			blit_.Draw(cmd_list, slot.target, &slot.input.view, 1, &constants,
				sizeof(constants) / sizeof(uint32_t), kThumbnailWidth, kThumbnailHeight);

			cmd_list->barrier(slot.texture, resource_usage::render_target, resource_usage::shader_resource);
			if (direct)
				cmd_list->barrier(res.handle, resource_usage::shader_resource, source_state);
		}
	}

	resource_view ThumbnailGrid::ViewFor(uint32_t resource_id) const
	{
		for (const Slot &slot : slots_)
			if (slot.resource_id == resource_id && slot.reason == nullptr)
				return slot.view;
		return resource_view {};
	}

	const char *ThumbnailGrid::ReasonFor(uint32_t resource_id) const
	{
		for (const Slot &slot : slots_)
			if (slot.resource_id == resource_id)
				return slot.reason;
		return !pass_error_.empty() ? "pass unavailable" : nullptr;
	}

	void ThumbnailGrid::ReleaseInputs()
	{
		for (Slot &slot : slots_)
		{
			DestroyInput(device_, slot.input, on_destroyed_);
			slot.resource_id = 0;
		}
	}

	void ThumbnailGrid::Destroy()
	{
		for (Slot &slot : slots_)
			DestroySlot(slot);
		slots_.clear();
		blit_.Destroy();
	}
}
