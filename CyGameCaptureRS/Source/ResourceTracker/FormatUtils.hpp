// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — helpers around reshade::api::format.
// ReShade's format enumeration uses the DXGI_FORMAT numbering, which is convenient
// because Spout stores the DXGI format value in its sender information.
#pragma once

#include <reshade_api_format.hpp>
#include <cstdint>

namespace cygc
{
	// Human readable names, e.g. "R8G8B8A8_UNORM"
	const char *FormatName(reshade::api::format format);
	// Short names for compact lists, e.g. "RGBA8", "RGBA16F", "RGB10A2", "D24S8"
	const char *FormatShortName(reshade::api::format format);

	bool IsDepthStencilFormat(reshade::api::format format);
	bool IsSrgbFormat(reshade::api::format format);
	// Floating point colour formats: what a game would typically use for HDR scene colour
	bool IsFloatColorFormat(reshade::api::format format);
	bool IsBlockCompressedFormat(reshade::api::format format);

	// Typed, non-sRGB variant of a (possibly typeless / sRGB) format. Used for the
	// persistent capture / preview textures: copies are bitwise so the data is unchanged.
	reshade::api::format ToOutputFormat(reshade::api::format format);

	// Formats the Spout receiver side (OBS Spout2 plugin) can consume directly without conversion.
	bool IsSpoutCompatibleFormat(reshade::api::format format);

	/**
	* The format a buffer is published in.
	*
	* Spout receivers only open a handful of formats, and plenty of the interesting buffers are not
	* among them: Unreal's scene colour is R11G11B10_FLOAT, ambient occlusion is often R8_UNORM. Rather
	* than refusing them, they are converted by the same shader pass that already handles a partly
	* rendered source, into the nearest compatible format that does not lose range: a 16-bit float
	* target for anything floating point or wider than eight bits, plain RGBA8 otherwise.
	*/
	reshade::api::format ToPublishableFormat(reshade::api::format format);

	// True when two formats belong to the same typeless family (bitwise copies are legal)
	bool AreCopyCompatible(reshade::api::format a, reshade::api::format b);

	inline uint32_t ToDxgiFormat(reshade::api::format format) { return static_cast<uint32_t>(format); }
}
