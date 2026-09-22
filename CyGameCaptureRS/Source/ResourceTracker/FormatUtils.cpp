// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "FormatUtils.hpp"

#include <cstdio>

using namespace reshade::api;

namespace cygc
{
	const char *FormatName(format f)
	{
		switch (f)
		{
		case format::unknown: return "UNKNOWN";
		case format::r32g32b32a32_typeless: return "R32G32B32A32_TYPELESS";
		case format::r32g32b32a32_float: return "R32G32B32A32_FLOAT";
		case format::r32g32b32a32_uint: return "R32G32B32A32_UINT";
		case format::r32g32b32a32_sint: return "R32G32B32A32_SINT";
		case format::r32g32b32_typeless: return "R32G32B32_TYPELESS";
		case format::r32g32b32_float: return "R32G32B32_FLOAT";
		case format::r32g32b32_uint: return "R32G32B32_UINT";
		case format::r32g32b32_sint: return "R32G32B32_SINT";
		case format::r16g16b16a16_typeless: return "R16G16B16A16_TYPELESS";
		case format::r16g16b16a16_float: return "R16G16B16A16_FLOAT";
		case format::r16g16b16a16_unorm: return "R16G16B16A16_UNORM";
		case format::r16g16b16a16_uint: return "R16G16B16A16_UINT";
		case format::r16g16b16a16_snorm: return "R16G16B16A16_SNORM";
		case format::r16g16b16a16_sint: return "R16G16B16A16_SINT";
		case format::r32g32_typeless: return "R32G32_TYPELESS";
		case format::r32g32_float: return "R32G32_FLOAT";
		case format::r32g32_uint: return "R32G32_UINT";
		case format::r32g32_sint: return "R32G32_SINT";
		case format::r32_g8_typeless: return "R32G8X24_TYPELESS";
		case format::d32_float_s8_uint: return "D32_FLOAT_S8X24_UINT";
		case format::r32_float_x8_uint: return "R32_FLOAT_X8X24_TYPELESS";
		case format::x32_float_g8_uint: return "X32_TYPELESS_G8X24_UINT";
		case format::r10g10b10a2_typeless: return "R10G10B10A2_TYPELESS";
		case format::r10g10b10a2_unorm: return "R10G10B10A2_UNORM";
		case format::r10g10b10a2_uint: return "R10G10B10A2_UINT";
		case format::r10g10b10a2_xr_bias: return "R10G10B10_XR_BIAS_A2_UNORM";
		case format::r11g11b10_float: return "R11G11B10_FLOAT";
		case format::r8g8b8a8_typeless: return "R8G8B8A8_TYPELESS";
		case format::r8g8b8a8_unorm: return "R8G8B8A8_UNORM";
		case format::r8g8b8a8_unorm_srgb: return "R8G8B8A8_UNORM_SRGB";
		case format::r8g8b8a8_uint: return "R8G8B8A8_UINT";
		case format::r8g8b8a8_snorm: return "R8G8B8A8_SNORM";
		case format::r8g8b8a8_sint: return "R8G8B8A8_SINT";
		case format::r16g16_typeless: return "R16G16_TYPELESS";
		case format::r16g16_float: return "R16G16_FLOAT";
		case format::r16g16_unorm: return "R16G16_UNORM";
		case format::r16g16_uint: return "R16G16_UINT";
		case format::r16g16_snorm: return "R16G16_SNORM";
		case format::r16g16_sint: return "R16G16_SINT";
		case format::r32_typeless: return "R32_TYPELESS";
		case format::d32_float: return "D32_FLOAT";
		case format::r32_float: return "R32_FLOAT";
		case format::r32_uint: return "R32_UINT";
		case format::r32_sint: return "R32_SINT";
		case format::r24_g8_typeless: return "R24G8_TYPELESS";
		case format::d24_unorm_s8_uint: return "D24_UNORM_S8_UINT";
		case format::r24_unorm_x8_uint: return "R24_UNORM_X8_TYPELESS";
		case format::x24_unorm_g8_uint: return "X24_TYPELESS_G8_UINT";
		case format::r8g8_typeless: return "R8G8_TYPELESS";
		case format::r8g8_unorm: return "R8G8_UNORM";
		case format::r8g8_uint: return "R8G8_UINT";
		case format::r8g8_snorm: return "R8G8_SNORM";
		case format::r8g8_sint: return "R8G8_SINT";
		case format::r16_typeless: return "R16_TYPELESS";
		case format::r16_float: return "R16_FLOAT";
		case format::d16_unorm: return "D16_UNORM";
		case format::r16_unorm: return "R16_UNORM";
		case format::r16_uint: return "R16_UINT";
		case format::r16_snorm: return "R16_SNORM";
		case format::r16_sint: return "R16_SINT";
		case format::r8_typeless: return "R8_TYPELESS";
		case format::r8_unorm: return "R8_UNORM";
		case format::r8_uint: return "R8_UINT";
		case format::r8_snorm: return "R8_SNORM";
		case format::r8_sint: return "R8_SINT";
		case format::a8_unorm: return "A8_UNORM";
		case format::r1_unorm: return "R1_UNORM";
		case format::r9g9b9e5: return "R9G9B9E5_SHAREDEXP";
		case format::r8g8_b8g8_unorm: return "R8G8_B8G8_UNORM";
		case format::g8r8_g8b8_unorm: return "G8R8_G8B8_UNORM";
		case format::bc1_typeless: return "BC1_TYPELESS";
		case format::bc1_unorm: return "BC1_UNORM";
		case format::bc1_unorm_srgb: return "BC1_UNORM_SRGB";
		case format::bc2_typeless: return "BC2_TYPELESS";
		case format::bc2_unorm: return "BC2_UNORM";
		case format::bc2_unorm_srgb: return "BC2_UNORM_SRGB";
		case format::bc3_typeless: return "BC3_TYPELESS";
		case format::bc3_unorm: return "BC3_UNORM";
		case format::bc3_unorm_srgb: return "BC3_UNORM_SRGB";
		case format::bc4_typeless: return "BC4_TYPELESS";
		case format::bc4_unorm: return "BC4_UNORM";
		case format::bc4_snorm: return "BC4_SNORM";
		case format::bc5_typeless: return "BC5_TYPELESS";
		case format::bc5_unorm: return "BC5_UNORM";
		case format::bc5_snorm: return "BC5_SNORM";
		case format::b5g6r5_unorm: return "B5G6R5_UNORM";
		case format::b5g5r5a1_unorm: return "B5G5R5A1_UNORM";
		case format::b8g8r8a8_unorm: return "B8G8R8A8_UNORM";
		case format::b8g8r8x8_unorm: return "B8G8R8X8_UNORM";
		case format::b8g8r8a8_typeless: return "B8G8R8A8_TYPELESS";
		case format::b8g8r8a8_unorm_srgb: return "B8G8R8A8_UNORM_SRGB";
		case format::b8g8r8x8_typeless: return "B8G8R8X8_TYPELESS";
		case format::b8g8r8x8_unorm_srgb: return "B8G8R8X8_UNORM_SRGB";
		case format::bc6h_typeless: return "BC6H_TYPELESS";
		case format::bc6h_ufloat: return "BC6H_UF16";
		case format::bc6h_sfloat: return "BC6H_SF16";
		case format::bc7_typeless: return "BC7_TYPELESS";
		case format::bc7_unorm: return "BC7_UNORM";
		case format::bc7_unorm_srgb: return "BC7_UNORM_SRGB";
		case format::b4g4r4a4_unorm: return "B4G4R4A4_UNORM";
		case format::a4b4g4r4_unorm: return "A4B4G4R4_UNORM";
		default:
			break;
		}
		// Rare formats: keep a few rotating buffers so several unknown formats can be displayed in one frame
		static thread_local char buffers[4][24];
		static thread_local unsigned int next = 0;
		char *buffer = buffers[next++ % 4];
		snprintf(buffer, sizeof(buffers[0]), "FORMAT_%u", static_cast<uint32_t>(f));
		return buffer;
	}

	const char *FormatShortName(format f)
	{
		switch (format_to_typeless(f))
		{
		case format::r8g8b8a8_typeless: return IsSrgbFormat(f) ? "RGBA8s" : "RGBA8";
		case format::b8g8r8a8_typeless: return IsSrgbFormat(f) ? "BGRA8s" : "BGRA8";
		case format::b8g8r8x8_typeless: return IsSrgbFormat(f) ? "BGRX8s" : "BGRX8";
		case format::r16g16b16a16_typeless: return f == format::r16g16b16a16_float ? "RGBA16F" : "RGBA16";
		case format::r32g32b32a32_typeless: return "RGBA32F";
		case format::r10g10b10a2_typeless: return "RGB10A2";
		case format::r11g11b10_float: return "R11G11B10F";
		case format::r16g16_typeless: return f == format::r16g16_float ? "RG16F" : "RG16";
		case format::r32g32_typeless: return "RG32F";
		case format::r8g8_typeless: return "RG8";
		case format::r16_typeless: return (f == format::d16_unorm) ? "D16" : (f == format::r16_float ? "R16F" : "R16");
		case format::r32_typeless: return (f == format::d32_float) ? "D32F" : (f == format::r32_float ? "R32F" : "R32");
		case format::r8_typeless: return "R8";
		case format::r24_g8_typeless: return "D24S8";
		case format::r32_g8_typeless: return "D32FS8";
		case format::r9g9b9e5: return "RGB9E5";
		case format::bc1_typeless: return "BC1";
		case format::bc2_typeless: return "BC2";
		case format::bc3_typeless: return "BC3";
		case format::bc4_typeless: return "BC4";
		case format::bc5_typeless: return "BC5";
		case format::bc6h_typeless: return "BC6H";
		case format::bc7_typeless: return "BC7";
		default:
			return FormatName(f);
		}
	}

	bool IsDepthStencilFormat(format f)
	{
		switch (f)
		{
		case format::d16_unorm:
		case format::d24_unorm_s8_uint:
		case format::d32_float:
		case format::d32_float_s8_uint:
		case format::r24_g8_typeless:
		case format::r24_unorm_x8_uint:
		case format::x24_unorm_g8_uint:
		case format::r32_g8_typeless:
		case format::r32_float_x8_uint:
		case format::x32_float_g8_uint:
			return true;
		default:
			return false;
		}
	}

	bool IsSrgbFormat(format f)
	{
		switch (f)
		{
		case format::r8g8b8a8_unorm_srgb:
		case format::b8g8r8a8_unorm_srgb:
		case format::b8g8r8x8_unorm_srgb:
		case format::bc1_unorm_srgb:
		case format::bc2_unorm_srgb:
		case format::bc3_unorm_srgb:
		case format::bc7_unorm_srgb:
			return true;
		default:
			return false;
		}
	}

	bool IsFloatColorFormat(format f)
	{
		switch (f)
		{
		case format::r16g16b16a16_float:
		case format::r16g16b16a16_typeless:
		case format::r32g32b32a32_float:
		case format::r32g32b32a32_typeless:
		case format::r11g11b10_float:
		case format::r9g9b9e5:
		case format::r16g16_float:
		case format::r32g32_float:
		case format::r16_float:
		case format::r32_float:
			return true;
		default:
			return false;
		}
	}

	bool IsBlockCompressedFormat(format f)
	{
		return format_is_block_compressed(f);
	}

	format ToOutputFormat(format f)
	{
		return format_to_default_typed(f, 0);
	}

	bool IsSpoutCompatibleFormat(format f)
	{
		// DXGI formats the Spout receivers (including the OBS Spout2 plugin) map directly to a texture format.
		switch (ToOutputFormat(f))
		{
		case format::r8g8b8a8_unorm:
		case format::b8g8r8a8_unorm:
		case format::r10g10b10a2_unorm:
		case format::r16g16b16a16_float:
		case format::r16g16b16a16_unorm:
		case format::r32g32b32a32_float:
			return true;
		default:
			return false;
		}
	}

	format ToPublishableFormat(format value)
	{
		const format typed = ToOutputFormat(value);
		if (IsSpoutCompatibleFormat(typed))
			return typed;

		switch (typed)
		{
		// Floating point or more than eight bits per channel: keep the range and the precision
		case format::r11g11b10_float:
		case format::r9g9b9e5:
		case format::r16g16_float:
		case format::r32g32_float:
		case format::r16_float:
		case format::r32_float:
		case format::r16g16_unorm:
		case format::r16_unorm:
			return format::r16g16b16a16_float;
		default:
			// Everything else is eight bits or less per channel, or something exotic enough that the
			// safest landing place is the format every receiver understands.
			return format::r8g8b8a8_unorm;
		}
	}

	bool AreCopyCompatible(format a, format b)
	{
		return format_to_typeless(a) == format_to_typeless(b);
	}
}
