// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "InspectorModel.hpp"
#include "ResourceTracker/FormatUtils.hpp"

#include <algorithm>

using namespace reshade::api;

namespace cygc
{
	bool InspectorModel::PassesFilter(const TrackedResource &res, const InspectorFilter &filter,
		const resource_desc &backbuffer, uint64_t frame_index)
	{
		if (!res.IsTexture2D())
			return false;
		if (res.is_backbuffer && !filter.backbuffers)
			return false;

		const bool is_depth = res.IsDepthStencil() || IsDepthStencilFormat(res.Format());
		if (is_depth ? !filter.depth : !filter.color_targets)
			return false;

		if (filter.written_recently && res.last.writes == 0 &&
			res.last_written_frame + filter.recent_frames < frame_index)
			return false;
		if (filter.persistent_only && !res.is_backbuffer && !res.IsPersistent(frame_index))
			return false;
		if (!filter.multisampled && res.IsMultisampled())
			return false;
		if (!filter.mips_or_arrays && res.HasMipsOrLayers())
			return false;

		if (filter.format != format::unknown && format_to_typeless(filter.format) != format_to_typeless(res.Format()))
			return false;

		bool resolution_ok = true;
		const uint32_t w = res.Width(), h = res.Height();
		const uint32_t bw = backbuffer.texture.width, bh = backbuffer.texture.height;
		switch (filter.resolution)
		{
		case ResolutionFilter::Any:
			break;
		case ResolutionFilter::Backbuffer:
			resolution_ok = (bw == 0) || (w == bw && h == bh);
			break;
		case ResolutionFilter::AtLeast75Percent:
			resolution_ok = (bw == 0) || (w * 4 >= bw * 3 && h * 4 >= bh * 3);
			break;
		case ResolutionFilter::AtLeast50Percent:
			resolution_ok = (bw == 0) || (w * 2 >= bw && h * 2 >= bh);
			break;
		case ResolutionFilter::Custom:
			resolution_ok = w >= filter.custom_width && h >= filter.custom_height;
			break;
		}
		if (!resolution_ok && !filter.small_resources)
			return false;

		return true;
	}

	void InspectorModel::Refresh(const ResourceTracker &tracker)
	{
		const std::vector<TrackedResource> all = tracker.Snapshot();
		backbuffer_desc_ = tracker.BackbufferDesc();
		frame_index_ = tracker.FrameIndex();
		total_count_ = all.size();

		entries_.clear();
		formats_.clear();
		for (const TrackedResource &res : all)
		{
			if (!res.IsTexture2D())
				continue;
			const format typeless = format_to_typeless(res.Format());
			if (std::find(formats_.begin(), formats_.end(), typeless) == formats_.end())
				formats_.push_back(typeless);
			if (PassesFilter(res, filter, backbuffer_desc_, frame_index_))
				entries_.push_back(res);
		}
		std::sort(formats_.begin(), formats_.end(), [](format a, format b) { return static_cast<uint32_t>(a) < static_cast<uint32_t>(b); });

		// Snapshot() already comes ordered by id; anything else is a ranking, best first, with the id
		// as the tie breaker so that equal entries never swap places from one frame to the next.
		const uint64_t frame = frame_index_;
		switch (filter.sort)
		{
		case SortKey::Id:
			break;
		case SortKey::Stability:
			std::stable_sort(entries_.begin(), entries_.end(), [frame](const TrackedResource &a, const TrackedResource &b) {
				return a.WriteRatio(frame) > b.WriteRatio(frame);
			});
			break;
		case SortKey::Size:
			std::stable_sort(entries_.begin(), entries_.end(), [](const TrackedResource &a, const TrackedResource &b) {
				return static_cast<uint64_t>(a.Width()) * a.Height() > static_cast<uint64_t>(b.Width()) * b.Height();
			});
			break;
		case SortKey::Writes:
			std::stable_sort(entries_.begin(), entries_.end(), [](const TrackedResource &a, const TrackedResource &b) {
				return a.last.writes > b.last.writes;
			});
			break;
		case SortKey::Format:
			std::stable_sort(entries_.begin(), entries_.end(), [](const TrackedResource &a, const TrackedResource &b) {
				return static_cast<uint32_t>(a.Format()) < static_cast<uint32_t>(b.Format());
			});
			break;
		}
	}

	int InspectorModel::SelectedIndex() const
	{
		if (selected_id == 0)
			return -1;
		for (size_t i = 0; i < entries_.size(); ++i)
			if (entries_[i].id == selected_id)
				return static_cast<int>(i);
		return -1;
	}

	bool InspectorModel::Select(uint32_t id)
	{
		if (id != selected_id)
			moment = 0;
		selected_id = id;
		return SelectedIndex() >= 0;
	}

	void InspectorModel::SelectNext()
	{
		if (entries_.empty())
			return;
		const int index = SelectedIndex();
		Select(entries_[(index < 0) ? 0 : (static_cast<size_t>(index) + 1) % entries_.size()].id);
	}

	void InspectorModel::SelectPrevious()
	{
		if (entries_.empty())
			return;
		const int index = SelectedIndex();
		Select(entries_[(index <= 0) ? entries_.size() - 1 : static_cast<size_t>(index) - 1].id);
	}

	const TrackedResource *InspectorModel::Selected() const
	{
		const int index = SelectedIndex();
		return index >= 0 ? &entries_[static_cast<size_t>(index)] : nullptr;
	}
}
