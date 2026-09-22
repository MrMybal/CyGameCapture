// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "HudPass.hpp"

namespace cygc
{
	namespace
	{
		/**
		 * t0 is the finished image (interface included), t1 the scene before it.
		 *
		 * The mask is built from the largest per-channel difference rather than the luminance difference,
		 * because plenty of interface elements are coloured overlays that barely move the luminance --
		 * a red damage vignette over a bright scene, for instance.
		 */
		constexpr const char *kHudShader = R"HLSL(
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

Texture2D    FinalTexture : register(t0);
Texture2D    CleanTexture : register(t1);
SamplerState LinearSampler : register(s0);

cbuffer Constants : register(b0)
{
	float Threshold;
	float Softness;
	uint  KeepFinalColour;
	float Padding;
	// The two buffers may be rendered at different scales: with dynamic resolution the scene is
	// typically smaller than the finished image. Each one is sampled through its own rectangle, so the
	// comparison happens between matching points whatever their sizes.
	float4 FinalRect;
	float4 CleanRect;
};

float4 PSHudIsolate(VSOut input) : SV_Target
{
	float3 final_colour = FinalTexture.SampleLevel(LinearSampler, input.uv * FinalRect.xy + FinalRect.zw, 0).rgb;
	float3 clean_colour = CleanTexture.SampleLevel(LinearSampler, input.uv * CleanRect.xy + CleanRect.zw, 0).rgb;

	float3 difference = abs(final_colour - clean_colour);
	float amount = max(difference.r, max(difference.g, difference.b));

	// smoothstep rather than a hard cut: the soft edges of an interface stay soft instead of aliasing
	float mask = smoothstep(Threshold, max(Softness, Threshold + 1e-4), amount);

	float3 colour = KeepFinalColour ? final_colour : difference;
	return float4(colour, mask);
}
)HLSL";
	}

	bool CreateHudPass(reshade::api::device *device, FullscreenPass &out_pass, std::string &error)
	{
		return out_pass.Create(device, kHudShader, "PSHudIsolate", 2,
			sizeof(HudConstants) / sizeof(uint32_t), kHudStreamFormat, error);
	}

	HudConstants MakeHudConstants(const HudSettings &settings)
	{
		HudConstants constants = {};
		constants.threshold = settings.threshold;
		constants.softness = settings.softness;
		constants.keep_final_colour = settings.keep_final_colour ? 1u : 0u;
		return constants;
	}
}
