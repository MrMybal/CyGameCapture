// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — a contact sheet of the tracked buffers.
//
// Why
// ---
// A game exposes hundreds of render targets and a few dozen pass the inspector's filters. Clicking
// them one by one to see what each holds is the slowest possible way of finding the scene, the HUD or
// the ambient occlusion pass, and it is what the single preview forced. Drawing them all small, at
// once, turns the search into a glance.
//
// How
// ---
// One small render target per visible entry, filled by the same scaling pass the capture uses, on the
// presenting queue, while the overlay is open. Nothing is ever read back to the CPU and no game owned
// view is handed to ImGui: each thumbnail is a texture this class owns.
#pragma once

#include "GpuPass/BlitPass.hpp"
#include "GpuPass/PassInput.hpp"
#include "ResourceTracker/ResourceTracker.hpp"

#include <cstdint>
#include <reshade.hpp>
#include <string>
#include <vector>

namespace cygc
{
	/**
	 * How many buffers the contact sheet draws per frame.
	 *
	 * This is a GPU budget, not a limit on what can be inspected or captured: the grid pages through
	 * a longer list, and raising it only costs one more small draw per frame while the overlay is open.
	 */
	constexpr size_t kThumbnailBudget = 32;

	constexpr uint32_t kThumbnailWidth = 256;
	constexpr uint32_t kThumbnailHeight = 144;

	class ThumbnailGrid
	{
	public:
		explicit ThumbnailGrid(reshade::api::device *device);
		~ThumbnailGrid();

		ThumbnailGrid(const ThumbnailGrid &) = delete;
		ThumbnailGrid &operator=(const ThumbnailGrid &) = delete;

		/** Resources the add-on creates for itself, so the caller can keep them out of the tracker. */
		void SetResourceNotice(ResourceNotice on_created, ResourceNotice on_destroyed);

		/**
		 * Redraws the thumbnails of `entries[first .. first + kThumbnailBudget)`.
		 *
		 * Entries that cannot be sampled (multisampled, block compressed, or a copy that fails) simply
		 * get no image; ViewFor returns 0 for them and the overlay shows the reason.
		 */
		void Update(reshade::api::command_queue *queue, const std::vector<TrackedResource> &entries, size_t first);

		/** The thumbnail of a resource, or 0 when it has none this frame. */
		reshade::api::resource_view ViewFor(uint32_t resource_id) const;
		const char *ReasonFor(uint32_t resource_id) const;

		/**
		* Frees the readable copies without touching the thumbnails themselves.
		*
		* Those copies are full resolution, one per buffer that cannot be sampled directly, so they are
		* by far the expensive part of the contact sheet and there is no reason to keep them alive while
		* nobody is looking at it. The small textures stay, so reopening the sheet is instant.
		*/
		void ReleaseInputs();

		void Destroy();

	private:
		struct Slot
		{
			reshade::api::resource texture = {};
			reshade::api::resource_view target = {};     // render target view, what the blit writes
			reshade::api::resource_view view = {};       // shader resource view, what ImGui shows
			PassInput input;
			uint32_t resource_id = 0;
			const char *reason = nullptr;                // why there is no image, nullptr when there is one
		};

		bool EnsureSlot(Slot &slot);
		void DestroySlot(Slot &slot);

		reshade::api::device *const device_;
		FullscreenPass blit_;
		std::vector<Slot> slots_;
		ResourceNotice on_created_;
		ResourceNotice on_destroyed_;
		std::string pass_error_;
	};
}
