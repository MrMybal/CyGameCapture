// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — RHI compatibility layer for UE 4.27 .. 5.8.
//
// Only public, stable entry points are used:
//   * GDynamicRHI->RHIGetNativeDevice() / RHIGetNativeGraphicsQueue()   (4.27+)
//   * FRHITexture::GetNativeResource()                                    (4.27+)
//   * RHICmdList.CopyTexture / Transition / EnqueueLambda                 (4.26+)
//   * RHICreateTexture2D (<= 5.0) / RHICreateTexture(FRHITextureCreateDesc) (5.1+)
//   * RHIGetInterfaceType() (5.1+) / GDynamicRHI->GetName() (4.27, 5.0)
// No private engine headers, no engine modification.
#pragma once

#include "CoreMinimal.h"
#include "Misc/EngineVersionComparison.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RHICommandList.h"
#include "DynamicRHI.h"
#include "PixelFormat.h"
#include "Spout/CySpoutIncludes.h"

#define CY_UE_AT_LEAST(Major, Minor) (!UE_VERSION_OLDER_THAN(Major, Minor, 0))

#if CY_UE_AT_LEAST(5, 1)
using FCyTextureRef = FTextureRHIRef;
#else
using FCyTextureRef = FTexture2DRHIRef;
#endif

enum class ECyRHIType : uint8
{
	Unknown,
	D3D11,
	D3D12,
	Vulkan,
	OpenGL,
	Other,
};

inline const TCHAR* CyRHITypeName(ECyRHIType Type)
{
	switch (Type)
	{
	case ECyRHIType::D3D11: return TEXT("D3D11");
	case ECyRHIType::D3D12: return TEXT("D3D12");
	case ECyRHIType::Vulkan: return TEXT("Vulkan");
	case ECyRHIType::OpenGL: return TEXT("OpenGL");
	case ECyRHIType::Other: return TEXT("Other");
	default: return TEXT("Unknown");
	}
}

inline ECyRHIType CyGetRHIType()
{
	if (GDynamicRHI == nullptr)
	{
		return ECyRHIType::Unknown;
	}
#if CY_UE_AT_LEAST(5, 1)
	switch (RHIGetInterfaceType())
	{
	case ERHIInterfaceType::D3D11: return ECyRHIType::D3D11;
	case ERHIInterfaceType::D3D12: return ECyRHIType::D3D12;
	case ERHIInterfaceType::Vulkan: return ECyRHIType::Vulkan;
	case ERHIInterfaceType::OpenGL: return ECyRHIType::OpenGL;
	default: return ECyRHIType::Other;
	}
#else
	const FString Name = GDynamicRHI->GetName();
	if (Name.StartsWith(TEXT("D3D11"))) return ECyRHIType::D3D11;
	if (Name.StartsWith(TEXT("D3D12"))) return ECyRHIType::D3D12;
	if (Name.StartsWith(TEXT("Vulkan"))) return ECyRHIType::Vulkan;
	if (Name.StartsWith(TEXT("OpenGL"))) return ECyRHIType::OpenGL;
	return ECyRHIType::Other;
#endif
}

inline FIntPoint CyGetTextureSize(FRHITexture* Texture)
{
	if (Texture == nullptr)
	{
		return FIntPoint::ZeroValue;
	}
#if CY_UE_AT_LEAST(5, 1)
	return Texture->GetSizeXY();
#else
	if (FRHITexture2D* Texture2D = Texture->GetTexture2D())
	{
		return Texture2D->GetSizeXY();
	}
	return FIntPoint::ZeroValue;
#endif
}

inline EPixelFormat CyGetTextureFormat(FRHITexture* Texture)
{
	return Texture != nullptr ? Texture->GetFormat() : PF_Unknown;
}

/** Creates a single mip, single sample 2D texture (render thread or game thread). */
inline FTextureRHIRef CyCreateTexture2D(const TCHAR* DebugName, uint32 Width, uint32 Height, EPixelFormat Format, ETextureCreateFlags Flags, ERHIAccess InitialState)
{
#if CY_UE_AT_LEAST(5, 1)
	const FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create2D(DebugName, static_cast<int32>(Width), static_cast<int32>(Height), Format)
		.SetFlags(Flags)
		.SetNumMips(1)
		.SetInitialState(InitialState);
	return RHICreateTexture(Desc);
#else
	FRHIResourceCreateInfo CreateInfo(DebugName);
	return RHICreateTexture2D(Width, Height, static_cast<uint8>(Format), 1, 1, Flags, InitialState, CreateInfo);
#endif
}

inline void* CyGetNativeDevice()
{
	return GDynamicRHI != nullptr ? GDynamicRHI->RHIGetNativeDevice() : nullptr;
}

inline void* CyGetNativeGraphicsQueue()
{
	return GDynamicRHI != nullptr ? GDynamicRHI->RHIGetNativeGraphicsQueue() : nullptr;
}

/** ID3D11Texture2D* / ID3D12Resource* of an RHI texture. Render thread. */
inline void* CyGetNativeResource(FRHITexture* Texture)
{
	return Texture != nullptr ? Texture->GetNativeResource() : nullptr;
}

// ------------------------------------------------------------------------------------------------
// Pixel formats. DXGI_FORMAT values are used as the interchange format because Spout publishes them.
// ------------------------------------------------------------------------------------------------
namespace CyDxgi
{
	constexpr uint32 Unknown = 0;
	constexpr uint32 R32G32B32A32_FLOAT = 2;
	constexpr uint32 R16G16B16A16_FLOAT = 10;
	constexpr uint32 R16G16B16A16_UNORM = 11;
	constexpr uint32 R10G10B10A2_UNORM = 24;
	constexpr uint32 R11G11B10_FLOAT = 26;
	constexpr uint32 R8G8B8A8_UNORM = 28;
	constexpr uint32 R8G8B8A8_UNORM_SRGB = 29;
	constexpr uint32 B8G8R8A8_UNORM = 87;
	constexpr uint32 B8G8R8X8_UNORM = 88;
	constexpr uint32 B8G8R8A8_UNORM_SRGB = 91;
}

/** Typed DXGI format a UE pixel format is shared as (0 when not supported by the Spout path). */
inline uint32 CyPixelFormatToDxgi(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_B8G8R8A8: return CyDxgi::B8G8R8A8_UNORM;
	case PF_R8G8B8A8: return CyDxgi::R8G8B8A8_UNORM;
	case PF_A2B10G10R10: return CyDxgi::R10G10B10A2_UNORM;
	case PF_FloatRGBA: return CyDxgi::R16G16B16A16_FLOAT;
	case PF_A16B16G16R16: return CyDxgi::R16G16B16A16_UNORM;
	case PF_A32B32G32R32F: return CyDxgi::R32G32B32A32_FLOAT;
	default: return CyDxgi::Unknown;
	}
}

/** UE pixel format used for a render target receiving the given DXGI format (PF_Unknown when unsupported). */
inline EPixelFormat CyDxgiToPixelFormat(uint32 Dxgi)
{
	switch (Dxgi)
	{
	case CyDxgi::B8G8R8A8_UNORM:
	case CyDxgi::B8G8R8A8_UNORM_SRGB:
	case CyDxgi::B8G8R8X8_UNORM:
	case 0:
	case 21: // legacy D3D9 A8R8G8B8 senders
	case 22:
		return PF_B8G8R8A8;
	case CyDxgi::R8G8B8A8_UNORM:
	case CyDxgi::R8G8B8A8_UNORM_SRGB:
		return PF_R8G8B8A8;
	case CyDxgi::R10G10B10A2_UNORM:
		return PF_A2B10G10R10;
	case CyDxgi::R16G16B16A16_FLOAT:
		return PF_FloatRGBA;
	case CyDxgi::R16G16B16A16_UNORM:
		return PF_A16B16G16R16;
	case CyDxgi::R32G32B32A32_FLOAT:
		return PF_A32B32G32R32F;
	default:
		return PF_Unknown;
	}
}

inline FString CyDxgiFormatName(uint32 Dxgi)
{
	switch (Dxgi)
	{
	case CyDxgi::Unknown: return TEXT("UNKNOWN");
	case CyDxgi::R32G32B32A32_FLOAT: return TEXT("R32G32B32A32_FLOAT");
	case CyDxgi::R16G16B16A16_FLOAT: return TEXT("R16G16B16A16_FLOAT");
	case CyDxgi::R16G16B16A16_UNORM: return TEXT("R16G16B16A16_UNORM");
	case CyDxgi::R10G10B10A2_UNORM: return TEXT("R10G10B10A2_UNORM");
	case CyDxgi::R11G11B10_FLOAT: return TEXT("R11G11B10_FLOAT");
	case CyDxgi::R8G8B8A8_UNORM: return TEXT("R8G8B8A8_UNORM");
	case CyDxgi::R8G8B8A8_UNORM_SRGB: return TEXT("R8G8B8A8_UNORM_SRGB");
	case CyDxgi::B8G8R8A8_UNORM: return TEXT("B8G8R8A8_UNORM");
	case CyDxgi::B8G8R8X8_UNORM: return TEXT("B8G8R8X8_UNORM");
	case CyDxgi::B8G8R8A8_UNORM_SRGB: return TEXT("B8G8R8A8_UNORM_SRGB");
	case 21: return TEXT("D3D9_A8R8G8B8");
	case 22: return TEXT("D3D9_X8R8G8B8");
	default: return FString::Printf(TEXT("DXGI_%u"), Dxgi);
	}
}

/** True when two DXGI formats belong to the same typeless family (bitwise copies are legal). */
inline bool CyDxgiCopyCompatible(uint32 A, uint32 B)
{
	auto Family = [](uint32 F) -> uint32
	{
		switch (F)
		{
		case 27: case 28: case 29: case 30: case 31: case 32: return 27;          // R8G8B8A8
		case 87: case 90: case 91: return 90;                                       // B8G8R8A8
		case 88: case 92: case 93: return 92;                                       // B8G8R8X8
		case 9: case 10: case 11: case 12: case 13: case 14: return 9;              // R16G16B16A16
		case 23: case 24: case 25: case 89: return 23;                              // R10G10B10A2
		case 1: case 2: case 3: case 4: return 1;                                   // R32G32B32A32
		default: return F;
		}
	};
	return Family(A) == Family(B);
}

/** Milliseconds between two FPlatformTime::Cycles64 values. */
inline float CyCyclesToMs(uint64 Start, uint64 End)
{
	return static_cast<float>(FPlatformTime::ToMilliseconds64(End - Start));
}
