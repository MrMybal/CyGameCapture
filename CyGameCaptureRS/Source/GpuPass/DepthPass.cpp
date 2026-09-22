// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "DepthPass.hpp"

namespace cygc
{
	namespace
	{
		/**
		 * The linearisation itself is the one ReShade uses in ReShade.fxh, kept identical on purpose so
		 * that settings worked out for a game with a ReShade depth shader carry over unchanged:
		 *
		 *   reversed     : depth = 1 - depth
		 *   logarithmic  : depth = (exp(depth * log(far + 1)) - 1) / far
		 *   linearise    : depth /= far - depth * (far - 1)
		 *
		 * The near plane is taken as 1.0, again like ReShade: the ratio is what matters and exposing a
		 * second unknowable number would only make the result harder to dial in.
		 */
		constexpr const char *kDepthShader = R"HLSL(
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

Texture2D    DepthTexture : register(t0);
SamplerState DepthSampler : register(s0);

cbuffer Constants : register(b0)
{
	float FarPlane;
	uint  Flags;
	float2 Padding;
	float4 SourceRect;   // xy = scale, zw = offset: the part of the buffer the frame was rendered into
};

float4 PSDepthLinear(VSOut input) : SV_Target
{
	float2 uv = input.uv;
	if (Flags & 4u)
		uv.y = 1.0 - uv.y;
	uv = uv * SourceRect.xy + SourceRect.zw;

	float depth = DepthTexture.SampleLevel(DepthSampler, uv, 0).x;

	if (Flags & 1u)
		depth = 1.0 - depth;

	if (Flags & 2u)
		depth = (exp(depth * log(FarPlane + 1.0)) - 1.0) / FarPlane;

	depth /= FarPlane - depth * (FarPlane - 1.0);
	depth = saturate(depth);

	// Same value in R, G and B: 16 real bits in each channel, and it still reads as a grey-scale image
	// in the inspector preview and in OBS.
	return float4(depth, depth, depth, 1.0);
}
)HLSL";
	}

	bool CreateDepthPass(reshade::api::device *device, FullscreenPass &out_pass, std::string &error)
	{
		return out_pass.Create(device, kDepthShader, "PSDepthLinear", 1,
			sizeof(DepthConstants) / sizeof(uint32_t), kDepthStreamFormat, error);
	}

	DepthConstants MakeDepthConstants(const DepthSettings &settings)
	{
		DepthConstants constants = {};
		constants.far_plane = settings.far_plane > 1.0f ? settings.far_plane : 1.0f;
		constants.flags = (settings.reversed ? 1u : 0u) | (settings.logarithmic ? 2u : 0u) | (settings.upside_down ? 4u : 0u);
		return constants;
	}
}
