// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "BlitPass.hpp"

namespace cygc
{
	namespace
	{
		constexpr const char *kOpaqueAlphaShader = R"HLSL(
float4 PSOpaqueAlpha() : SV_Target { return float4(0, 0, 0, 1); }
)HLSL";
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

	bool WriteOpaqueAlpha(reshade::api::device *device, reshade::api::command_list *cmd,
		reshade::api::resource_view target, const reshade::api::resource_desc &desc,
		FullscreenPass &pass, std::string &error)
	{
		if (pass.IsValid() && pass.RenderTargetFormat() != desc.texture.format)
			pass.Destroy();
		if (!pass.IsValid() && !pass.Create(device, kOpaqueAlphaShader, "PSOpaqueAlpha", 0, 0,
			desc.texture.format, error, 0x8))
			return false;
		pass.Draw(cmd, target, nullptr, 0, nullptr, 0, desc.texture.width, desc.texture.height);
		return true;
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
