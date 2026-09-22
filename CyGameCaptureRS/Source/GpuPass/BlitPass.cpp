// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "BlitPass.hpp"

namespace cygc
{
	namespace
	{
		constexpr const char *kBlitShader = R"HLSL(
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

Texture2D    SourceTexture : register(t0);
SamplerState LinearSampler : register(s0);

cbuffer Constants : register(b0)
{
	float4 SourceRect;   // xy = scale, zw = offset
};

float4 PSBlit(VSOut input) : SV_Target
{
	return SourceTexture.SampleLevel(LinearSampler, input.uv * SourceRect.xy + SourceRect.zw, 0);
}
)HLSL";
	}

	SourceRect MakeSourceRect(const ActiveRect &rect, uint32_t texture_width, uint32_t texture_height)
	{
		SourceRect result;
		if (!rect.IsValid() || texture_width == 0 || texture_height == 0)
			return result;

		result.scale_x = static_cast<float>(rect.width) / static_cast<float>(texture_width);
		result.scale_y = static_cast<float>(rect.height) / static_cast<float>(texture_height);
		result.offset_x = static_cast<float>(rect.x) / static_cast<float>(texture_width);
		result.offset_y = static_cast<float>(rect.y) / static_cast<float>(texture_height);
		return result;
	}

	bool CreateBlitPass(reshade::api::device *device, reshade::api::format render_target_format,
		FullscreenPass &out_pass, std::string &error)
	{
		return out_pass.Create(device, kBlitShader, "PSBlit", 1,
			sizeof(BlitConstants) / sizeof(uint32_t), render_target_format, error);
	}
}
