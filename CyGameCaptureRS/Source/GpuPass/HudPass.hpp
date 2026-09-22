// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — isolating the HUD by comparing two buffers of the same frame.
//
// In most games the HUD is not a buffer you can select: it is drawn straight onto the image, so what
// exists is "the scene before the interface" and "the finished image". The interface on its own is the
// difference between the two.
//
// What this can and cannot do
// ---------------------------
// Where the game composited the interface with alpha blending, the finished image is
// `final = hud * a + clean * (1 - a)`. That is one equation with two unknowns per pixel, so `hud` and
// `a` cannot both be recovered exactly. What this pass produces instead is what an editor actually
// wants: the finished image as the colour, and a mask in the alpha channel saying where the two buffers
// disagree. Laid over the clean scene with that alpha it reproduces the finished image, and on its own
// it shows the interface cut out on transparency.
//
// It is exact wherever the interface is opaque (there `final` *is* the interface) and an approximation
// in the soft edges and translucent panels, which is why the threshold and the softness are exposed
// rather than hard-coded.
//
// When a game *does* render its interface into a separate render target with an alpha channel, select
// that buffer directly instead: a plain copy is exact and costs one copy rather than a pass.
#pragma once

#include "GpuPass/FullscreenPass.hpp"

#include <cstdint>

namespace cygc
{
	/** How the difference between the two buffers becomes a mask. */
	struct HudSettings
	{
		/// Below this much difference (0..1, per channel) the pixel is considered pure scene.
		float threshold = 0.02f;

		/// Difference at which the mask reaches fully opaque. Must stay above `threshold`.
		float softness = 0.08f;

		/// Keep the finished image as the colour (what an editor wants) instead of the raw difference.
		bool keep_final_colour = true;

		bool operator==(const HudSettings &other) const
		{
			return threshold == other.threshold && softness == other.softness &&
				keep_final_colour == other.keep_final_colour;
		}
		bool operator!=(const HudSettings &other) const { return !(*this == other); }
	};

	/** The HUD stream carries an alpha channel, so it is published as 8-bit RGBA. */
	constexpr reshade::api::format kHudStreamFormat = reshade::api::format::r8g8b8a8_unorm;

	bool CreateHudPass(reshade::api::device *device, FullscreenPass &out_pass, std::string &error);

	struct HudConstants
	{
		float threshold;
		float softness;
		uint32_t keep_final_colour;
		float padding;
		float final_rect[4];    // see GpuPass/BlitPass.hpp: dynamic resolution
		float clean_rect[4];
	};

	HudConstants MakeHudConstants(const HudSettings &settings);
}
