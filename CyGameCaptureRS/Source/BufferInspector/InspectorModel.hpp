// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — Buffer Inspector model (filters + selection), UI toolkit agnostic.
// The ImGui code in UI/Overlay.cpp only renders what this model exposes.
#pragma once

#include "ResourceTracker/ResourceTracker.hpp"

#include <cstdint>
#include <vector>

namespace cygc
{
	enum class ResolutionFilter : int
	{
		Any = 0,
		Backbuffer,          // exactly the swap chain size
		AtLeast75Percent,
		AtLeast50Percent,
		Custom,
	};

	/** Column the list is ordered by. Anything but Id is a ranking, so it is shown best first. */
	enum class SortKey : int
	{
		Id = 0,
		Stability,       // how regularly the buffer is written: the steady ones first
		Size,
		Writes,
		Format,
	};

	struct InspectorFilter
	{
		/**
		* Hide resources that have not been written recently.
		*
		* "Recently" rather than "during the last frame": plenty of buffers are filled every other
		* frame, or every third one, and a strict test makes them blink in and out of the list.
		*/
		bool written_recently = true;
		uint32_t recent_frames = 8;          // tolerance of the test above, in frames
		/** Hide transient pool textures; see TrackedResource::IsPersistent. */
		bool persistent_only = true;
		bool color_targets = true;           // resources usable as render target or UAV
		bool depth = false;                  // depth-stencil resources
		bool small_resources = false;        // below the resolution filter
		bool multisampled = true;
		bool mips_or_arrays = true;
		bool backbuffers = true;
		ResolutionFilter resolution = ResolutionFilter::AtLeast50Percent;
		uint32_t custom_width = 0;
		uint32_t custom_height = 0;
		reshade::api::format format = reshade::api::format::unknown;   // unknown == any
		SortKey sort = SortKey::Id;
	};

	class InspectorModel
	{
	public:
		InspectorFilter filter;
		uint32_t selected_id = 0;
		/**
		 * Moment of the frame the selected buffer is looked at: 0 is the end of the frame, K > 0 is
		 * "after its K-th write" (see Capture/FrameSnapshots). Belongs to the selection, so choosing
		 * another buffer brings it back to the end of the frame.
		 */
		uint32_t moment = 0;

		// Rebuild the filtered list from the tracker (call once per overlay frame)
		void Refresh(const ResourceTracker &tracker);

		/** Frame index the current list was built at, for the stability figures. */
		uint64_t FrameIndex() const { return frame_index_; }

		const std::vector<TrackedResource> &Entries() const { return entries_; }
		size_t TotalCount() const { return total_count_; }
		const std::vector<reshade::api::format> &AvailableFormats() const { return formats_; }
		reshade::api::resource_desc BackbufferDesc() const { return backbuffer_desc_; }

		int SelectedIndex() const;                     // index into Entries(), -1 if not visible
		bool Select(uint32_t id);
		void SelectNext();
		void SelectPrevious();
		const TrackedResource *Selected() const;       // from the filtered list (nullptr when hidden / none)

		static bool PassesFilter(const TrackedResource &res, const InspectorFilter &filter,
			const reshade::api::resource_desc &backbuffer, uint64_t frame_index);

	private:
		std::vector<TrackedResource> entries_;
		std::vector<reshade::api::format> formats_;
		reshade::api::resource_desc backbuffer_desc_ = {};
		uint64_t frame_index_ = 0;
		size_t total_count_ = 0;
	};
}
