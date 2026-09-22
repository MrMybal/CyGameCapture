// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — copying a sub-rectangle of a buffer into a full-size output.
//
// Why a shader rather than a copy
// -------------------------------
// Games that scale their resolution at run time almost never reallocate their render targets. They
// keep them at the maximum size and render the frame into a corner of them, then upscale on the way to
// the back buffer. The texture description never changes, so nothing in the resource itself says how
// much of it holds the current frame — only the viewport of the draws does (see ActiveRect).
//
// Copying such a target as a whole would hand a receiver a picture with a stale, usually black, margin
// that grows and shrinks as the game adjusts its resolution. Copying only the active rectangle would
// make the Spout sender change size several times a second, which every receiver would have to chase.
//
// So the active rectangle is *scaled* into an output of stable size, exactly as the game itself does
// for its own back buffer. The output only changes size when the game genuinely changes resolution.
#pragma once

#include "GpuPass/FullscreenPass.hpp"
#include "ResourceTracker/ResourceTracker.hpp"

#include <cstdint>

namespace cygc
{
	/**
	 * Where a pass has to read inside its source, as texture coordinates.
	 *
	 * `uv_used = uv * scale + offset`, so the whole of the output samples exactly the active area.
	 * The identity (scale 1, offset 0) means "read the whole texture" and costs nothing extra.
	 */
	struct SourceRect
	{
		float scale_x = 1.0f;
		float scale_y = 1.0f;
		float offset_x = 0.0f;
		float offset_y = 0.0f;

		bool IsIdentity() const
		{
			return scale_x == 1.0f && scale_y == 1.0f && offset_x == 0.0f && offset_y == 0.0f;
		}
	};

	/** Turns an active rectangle in texels into texture coordinates for a texture of that size. */
	SourceRect MakeSourceRect(const ActiveRect &rect, uint32_t texture_width, uint32_t texture_height);

	/** Builds the scaling blit for one render target format. */
	bool CreateBlitPass(reshade::api::device *device, reshade::api::format render_target_format,
		FullscreenPass &out_pass, std::string &error);

	struct BlitConstants
	{
		float source_rect[4];   // scale_x, scale_y, offset_x, offset_y
	};
}
