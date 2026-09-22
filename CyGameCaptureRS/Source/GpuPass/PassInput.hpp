// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — making a game buffer readable by a shader pass.
//
// A pass samples its source through a shader resource view, and two things get in the way:
//
//   * a lot of interesting buffers are not created with `shader_resource` usage at all (a back buffer
//     almost never is), so no view can be made of them. The answer is a copy the add-on owns, created
//     once and reused, which *is* samplable;
//   * a depth buffer cannot be read through its own format: D32_FLOAT has to be seen as R32_FLOAT and
//     D24_UNORM_S8_UINT as R24_UNORM_X8_UINT.
//
// Shared by the capture (which publishes the result) and by the buffer inspector's thumbnails (which
// only look at it), so both behave identically in front of the same awkward buffer.
#pragma once

#include "ResourceTracker/ResourceTracker.hpp"

#include <functional>
#include <reshade.hpp>

namespace cygc
{
	/** Best guess of the state a game resource is in when it is read at present time. */
	reshade::api::resource_usage GuessSourceState(const TrackedResource &res);

	struct PassInput
	{
		reshade::api::resource_view view = {};
		reshade::api::resource viewed = {};        // resource `view` belongs to (the source, or `copy`)
		reshade::api::resource copy = {};          // our readable copy, 0 when the source can be sampled
		reshade::api::resource_desc copy_desc = {};
	};

	/**
	 * Called with every resource this helper creates and destroys.
	 *
	 * The tracker has to be told to ignore them: they are not part of the game's frame, and ReShade
	 * reports a destruction synchronously, so a tracked one would call back into the middle of whatever
	 * is releasing it. May be empty.
	 */
	using ResourceNotice = std::function<void(reshade::api::resource)>;

	/**
	 * Points `input` at something the pass can sample, copying the source first if it has to.
	 *
	 * Returns false when neither is possible, which is never fatal: the caller falls back or skips.
	 */
	bool PrepareInput(reshade::api::device *device, PassInput &input, const TrackedResource &source,
		reshade::api::command_list *cmd_list, const ResourceNotice &on_created = {});

	void DestroyInput(reshade::api::device *device, PassInput &input, const ResourceNotice &on_destroyed = {});
}
