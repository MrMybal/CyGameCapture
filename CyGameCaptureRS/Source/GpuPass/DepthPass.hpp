// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — turning a game depth buffer into something that can be shared and recorded.
//
// A depth buffer cannot be sent as-is: its format is not one a Spout receiver can open, and its values
// are not linear — they sit wherever the game's projection matrix put them. Worse, the convention
// varies: many modern engines use reversed-Z (1 at the near plane), some render upside down, a few use
// a logarithmic distribution, and the near/far planes are simply not knowable from outside the game.
//
// So this pass linearises the depth with the parameters the user gives it and writes the result to
// R16G16B16A16_UNORM: 16 real bits of precision, a format every Spout receiver understands, and it
// still looks like a plain grey-scale image in a preview or in OBS because R, G and B all carry the
// same value.
//
// The settings mirror ReShade's own depth handling (RESHADE_DEPTH_*), so anything already known about a
// game from a ReShade depth-based shader applies here unchanged.
#pragma once

#include "GpuPass/FullscreenPass.hpp"

#include <cstdint>

namespace cygc
{
	/** What the depth buffer of this particular game looks like. Nothing here can be detected reliably. */
	struct DepthSettings
	{
		/// Distance the linearisation maps to 1.0. ReShade's own default, and a sane starting point.
		float far_plane = 1000.0f;

		/// 1.0 at the near plane instead of 0.0. Very common in modern engines.
		bool reversed = false;

		/// Depth distributed logarithmically (a few engines, mostly for very large worlds).
		bool logarithmic = false;

		/// The depth buffer is stored flipped compared to the colour buffer (some D3D9-era and OpenGL games).
		bool upside_down = false;

		bool operator==(const DepthSettings &other) const
		{
			return far_plane == other.far_plane && reversed == other.reversed &&
				logarithmic == other.logarithmic && upside_down == other.upside_down;
		}
		bool operator!=(const DepthSettings &other) const { return !(*this == other); }
	};

	/** The format the depth stream is published in. Spout compatible and 16 bits per channel. */
	constexpr reshade::api::format kDepthStreamFormat = reshade::api::format::r16g16b16a16_unorm;

	/** Builds the linearisation pass for `kDepthStreamFormat`. */
	bool CreateDepthPass(reshade::api::device *device, FullscreenPass &out_pass, std::string &error);

	/** The constant block the pass expects, in the order the shader declares it. */
	struct DepthConstants
	{
		float far_plane;
		uint32_t flags;      // bit 0 reversed, bit 1 logarithmic, bit 2 upside down
		float padding[2];
		float source_rect[4];   // see GpuPass/BlitPass.hpp: dynamic resolution
	};

	DepthConstants MakeDepthConstants(const DepthSettings &settings);
}
