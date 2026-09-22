// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

#include "Spout/CySpoutBackend.h"
#include "CyGameCaptureSettings.h"
#include "CyGameCaptureLog.h"

#if CYGC_WITH_SPOUT
TUniquePtr<ICySenderBackend> CyCreateD3D11SenderBackend(FString& OutError);
TUniquePtr<ICyReceiverBackend> CyCreateD3D11ReceiverBackend(FString& OutError);
TUniquePtr<ICySenderBackend> CyCreateD3D12SenderBackend(FString& OutError);
TUniquePtr<ICyReceiverBackend> CyCreateD3D12ReceiverBackend(FString& OutError);
void CyShutdownD3D11On12Bridge();
#endif

namespace CySpoutBackend
{
	bool IsSupported(FString& OutReason, FString& OutRHIName)
	{
		const ECyRHIType Type = CyGetRHIType();
		OutRHIName = CyRHITypeName(Type);
#if !CYGC_WITH_SPOUT
		OutReason = TEXT("Spout2 is only available on Windows (Win64) builds");
		return false;
#else
		const UCyGameCaptureSettings* Settings = UCyGameCaptureSettings::Get();
		if (Settings != nullptr && !Settings->bEnableSpout)
		{
			OutReason = TEXT("Spout disabled in Project Settings > Plugins > CyGameCaptureUE");
			return false;
		}
		switch (Type)
		{
		case ECyRHIType::D3D11:
			if (Settings != nullptr && !Settings->bEnableD3D11)
			{
				OutReason = TEXT("D3D11 backend disabled in project settings");
				return false;
			}
			return true;
		case ECyRHIType::D3D12:
			if (Settings != nullptr && !Settings->bEnableD3D12)
			{
				OutReason = TEXT("D3D12 backend disabled in project settings");
				return false;
			}
			return true;
		case ECyRHIType::Unknown:
			OutReason = TEXT("RHI not initialized yet");
			return false;
		default:
			OutReason = FString::Printf(TEXT("Spout texture sharing needs D3D11 or D3D12 (current RHI: %s). Start the game with -dx11 or -dx12."), *OutRHIName);
			return false;
		}
#endif
	}

	TUniquePtr<ICySenderBackend> CreateSender(FString& OutError)
	{
#if CYGC_WITH_SPOUT
		FString RHIName;
		if (!IsSupported(OutError, RHIName))
		{
			return nullptr;
		}
		switch (CyGetRHIType())
		{
		case ECyRHIType::D3D11: return CyCreateD3D11SenderBackend(OutError);
		case ECyRHIType::D3D12: return CyCreateD3D12SenderBackend(OutError);
		default: break;
		}
#endif
		OutError = TEXT("No Spout backend for this RHI");
		return nullptr;
	}

	TUniquePtr<ICyReceiverBackend> CreateReceiver(FString& OutError)
	{
#if CYGC_WITH_SPOUT
		FString RHIName;
		if (!IsSupported(OutError, RHIName))
		{
			return nullptr;
		}
		switch (CyGetRHIType())
		{
		case ECyRHIType::D3D11: return CyCreateD3D11ReceiverBackend(OutError);
		case ECyRHIType::D3D12: return CyCreateD3D12ReceiverBackend(OutError);
		default: break;
		}
#endif
		OutError = TEXT("No Spout backend for this RHI");
		return nullptr;
	}

	void ShutdownShared()
	{
#if CYGC_WITH_SPOUT
		CyShutdownD3D11On12Bridge();
#endif
	}
}
