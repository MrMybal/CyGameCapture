// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — GPU preview of a tracked resource inside the ReShade overlay.
//
//   Selected game resource --GPU copy/resolve--> CyGameCapture preview texture --SRV--> ImGui::Image
//
// The preview never hands a game-owned view to ImGui (the game could destroy it while the
// overlay draw list still references it) and never reads pixels back to the CPU.
// "Freeze" simply stops refreshing the add-on owned preview texture.
#pragma once

#include "ResourceTracker/ResourceTracker.hpp"

#include <reshade.hpp>
#include <cstdint>
#include <string>

namespace cygc
{
	class PreviewManager
	{
	public:
		explicit PreviewManager(reshade::api::device *device);
		~PreviewManager();

		PreviewManager(const PreviewManager &) = delete;
		PreviewManager &operator=(const PreviewManager &) = delete;

		// Refresh the preview from 'source' (no-op when frozen). Recorded on the presenting queue.
		void Update(reshade::api::command_queue *queue, const TrackedResource &source);

		// Show an existing add-on owned view instead (e.g. the capture output texture): no extra copy.
		void UseExternalView(reshade::api::resource_view view, const reshade::api::resource_desc &desc, uint32_t source_id);

		void Clear();
		void SetFrozen(bool frozen) { frozen_ = frozen; }
		bool IsFrozen() const { return frozen_; }

		bool HasImage() const { return view_ != 0; }
		reshade::api::resource_view View() const { return view_; }
		uint32_t Width() const { return desc_.texture.width; }
		uint32_t Height() const { return desc_.texture.height; }
		reshade::api::format Format() const { return desc_.texture.format; }
		uint32_t SourceId() const { return source_id_; }
		uint64_t LastUpdateFrame() const { return last_update_frame_; }
		const std::string &LastError() const { return last_error_; }

		// The overlay tells the preview it is visible; updates are skipped when nobody looks at them.
		void MarkWanted(uint64_t frame_index) { wanted_frame_ = frame_index; }
		bool IsWanted(uint64_t frame_index) const { return frame_index <= wanted_frame_ + 2; }

	private:
		bool EnsureTexture(const reshade::api::resource_desc &source_desc);
		void DestroyTexture();

		reshade::api::device *const device_;
		reshade::api::resource texture_ = {};
		reshade::api::resource_view texture_view_ = {};
		reshade::api::resource_desc texture_desc_ = {};

		reshade::api::resource_view view_ = {};     // what ImGui displays (own texture view or external view)
		reshade::api::resource_desc desc_ = {};
		bool external_ = false;
		bool frozen_ = false;
		uint32_t source_id_ = 0;
		uint64_t last_update_frame_ = 0;
		uint64_t wanted_frame_ = 0;
		std::string last_error_;
	};
}
