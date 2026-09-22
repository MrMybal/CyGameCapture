// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

#include "CyGameCaptureSpoutReceiverComponent.h"
#include "CyGameCaptureSubsystem.h"
#include "CyGameCaptureSettings.h"
#include "CyGameCaptureLog.h"
#include "RHI/CyRHICompat.h"

#include "Engine/TextureRenderTarget2D.h"
#include "UObject/UObjectGlobals.h"

UCyGameCaptureSpoutReceiverComponent::UCyGameCaptureSpoutReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;   // received frame available for this frame's rendering
	bAutoActivate = true;
}

bool UCyGameCaptureSpoutReceiverComponent::StartReceiver()
{
	if (Stream.IsValid())
	{
		return true;
	}
	UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get();
	if (Subsystem == nullptr)
	{
		return false;
	}
	const UCyGameCaptureSettings* Settings = UCyGameCaptureSettings::Get();
	if (Settings != nullptr && !Settings->bEnableSpout)
	{
		return false;
	}
#if WITH_EDITOR
	if (Settings != nullptr && !Settings->bAllowEditorCapture && GIsEditor)
	{
		return false;
	}
#endif
	if (SenderName.IsEmpty())
	{
		const FString Error = TEXT("No sender name set");
		CYGC_LOG(Warning, TEXT("Receiver '%s': %s"), *ReceiverName, *Error);
		OnError.Broadcast(Error);
		return false;
	}

	FCyReceiverStreamConfig Config;
	Config.ReceiverName = ReceiverName;
	Config.SenderName = SenderName;
	Config.bAutoReconnect = bAutoReconnect;
	Config.bDebug = bDebug;

	FString Error;
	Stream = Subsystem->CreateReceiver(Config, this, Error);
	if (!Stream.IsValid())
	{
		CYGC_LOG(Warning, TEXT("Receiver '%s' failed to start: %s"), *ReceiverName, *Error);
		OnError.Broadcast(Error);
		return false;
	}
	BindStreamEvents();
	Stream->SetTargetRenderTarget(GetReceiverTexture());
	return true;
}

void UCyGameCaptureSpoutReceiverComponent::StopReceiver()
{
	if (!Stream.IsValid())
	{
		return;
	}
	UnbindStreamEvents();
	if (UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get())
	{
		Subsystem->DestroyReceiver(Stream);
	}
	else
	{
		Stream->Stop();
	}
	Stream.Reset();
}

bool UCyGameCaptureSpoutReceiverComponent::IsReceiverActive() const
{
	return Stream.IsValid() && Stream->IsActive();
}

bool UCyGameCaptureSpoutReceiverComponent::IsConnected() const
{
	return Stream.IsValid() && Stream->IsConnected();
}

void UCyGameCaptureSpoutReceiverComponent::SetSenderName(const FString& NewSenderName)
{
	SenderName = NewSenderName;
	if (Stream.IsValid())
	{
		Stream->SetSenderName(NewSenderName);
	}
}

void UCyGameCaptureSpoutReceiverComponent::SetTargetRenderTarget(UTextureRenderTarget2D* NewTarget)
{
	TargetRenderTarget = NewTarget;
	if (NewTarget != nullptr)
	{
		OutputMode = ECyGameCaptureReceiverOutputMode::UserRenderTarget;
	}
	if (Stream.IsValid())
	{
		Stream->SetTargetRenderTarget(GetReceiverTexture());
	}
}

UTextureRenderTarget2D* UCyGameCaptureSpoutReceiverComponent::GetReceiverTexture() const
{
	return OutputMode == ECyGameCaptureReceiverOutputMode::UserRenderTarget ? TargetRenderTarget : InternalRenderTarget;
}

FIntPoint UCyGameCaptureSpoutReceiverComponent::GetReceiverResolution() const
{
	if (!Stream.IsValid() || !Stream->IsConnected())
	{
		return FIntPoint::ZeroValue;
	}
	const FCySpoutSenderInfo Info = Stream->GetConnectedSenderInfo();
	return FIntPoint(Info.Width, Info.Height);
}

FString UCyGameCaptureSpoutReceiverComponent::GetReceiverFormat() const
{
	return Stream.IsValid() && Stream->IsConnected() ? Stream->GetConnectedSenderInfo().FormatName : FString();
}

float UCyGameCaptureSpoutReceiverComponent::GetReceiverFPS() const
{
	return Stream.IsValid() ? Stream->GetFPS() : 0.0f;
}

FCyGameCaptureStreamStats UCyGameCaptureSpoutReceiverComponent::GetReceiverStats() const
{
	return Stream.IsValid() ? Stream->GetStats() : FCyGameCaptureStreamStats();
}

void UCyGameCaptureSpoutReceiverComponent::EnsureTargetMatches(const FCySpoutSenderInfo& Info)
{
	if (Info.Width <= 0 || Info.Height <= 0)
	{
		return;
	}
	const EPixelFormat WantedFormat = CyDxgiToPixelFormat(static_cast<uint32>(Info.DxgiFormat));
	if (WantedFormat == PF_Unknown)
	{
		return;
	}

	if (OutputMode == ECyGameCaptureReceiverOutputMode::InternalRenderTarget)
	{
		if (InternalRenderTarget == nullptr)
		{
			InternalRenderTarget = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
			InternalRenderTarget->ClearColor = FLinearColor::Black;
			InternalRenderTarget->bAutoGenerateMips = false;
		}
	}

	UTextureRenderTarget2D* Target = GetReceiverTexture();
	if (Target == nullptr)
	{
		return;
	}

	const bool bSizeMismatch = Target->SizeX != Info.Width || Target->SizeY != Info.Height;
	const bool bFormatMismatch = !CyDxgiCopyCompatible(CyPixelFormatToDxgi(Target->GetFormat()), static_cast<uint32>(Info.DxgiFormat));
	if (!bSizeMismatch && !bFormatMismatch)
	{
		return;
	}
	const bool bAllowed = OutputMode == ECyGameCaptureReceiverOutputMode::InternalRenderTarget || bResizeTargetAutomatically;
	if (!bAllowed)
	{
		return;
	}

	// 8 bit formats keep sRGB encoding (bForceLinearGamma=false); float formats are linear
	const bool bFloat = WantedFormat == PF_FloatRGBA || WantedFormat == PF_A32B32G32R32F;
	Target->InitCustomFormat(Info.Width, Info.Height, WantedFormat, bFloat);
	Target->UpdateResourceImmediate(false);
	CYGC_LOG(Log, TEXT("Receiver '%s': target render target set to %dx%d %s"), *ReceiverName, Info.Width, Info.Height, GPixelFormats[WantedFormat].Name);
	if (Stream.IsValid())
	{
		Stream->SetTargetRenderTarget(Target);
	}
}

void UCyGameCaptureSpoutReceiverComponent::BindStreamEvents()
{
	if (!Stream.IsValid())
	{
		return;
	}
	TWeakObjectPtr<UCyGameCaptureSpoutReceiverComponent> WeakThis(this);
	Stream->OnConnected.AddLambda([WeakThis](FCyReceiverStream& S)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->EnsureTargetMatches(S.GetConnectedSenderInfo());
			WeakThis->OnConnected.Broadcast();
		}
	});
	Stream->OnDisconnected.AddLambda([WeakThis](FCyReceiverStream&) { if (WeakThis.IsValid()) { WeakThis->OnDisconnected.Broadcast(); } });
	Stream->OnResolutionChanged.AddLambda([WeakThis](FCyReceiverStream& S)
	{
		if (WeakThis.IsValid())
		{
			const FCySpoutSenderInfo Info = S.GetConnectedSenderInfo();
			WeakThis->EnsureTargetMatches(Info);
			WeakThis->OnResolutionChanged.Broadcast(Info.Width, Info.Height);
		}
	});
	Stream->OnFormatChanged.AddLambda([WeakThis](FCyReceiverStream& S)
	{
		if (WeakThis.IsValid())
		{
			const FCySpoutSenderInfo Info = S.GetConnectedSenderInfo();
			WeakThis->EnsureTargetMatches(Info);
			WeakThis->OnFormatChanged.Broadcast(Info.FormatName);
		}
	});
	Stream->OnFrameReceived.AddLambda([WeakThis](FCyReceiverStream&)
	{
		if (WeakThis.IsValid() && WeakThis->FrameEventRateHz > 0.0f)
		{
			const double Now = FPlatformTime::Seconds();
			if (Now >= WeakThis->NextFrameEventTime)
			{
				WeakThis->NextFrameEventTime = Now + 1.0 / WeakThis->FrameEventRateHz;
				WeakThis->OnFrameReceived.Broadcast();
			}
		}
	});
	Stream->OnError.AddLambda([WeakThis](FCyReceiverStream&, const FString& Error) { if (WeakThis.IsValid()) { WeakThis->OnError.Broadcast(Error); } });
}

void UCyGameCaptureSpoutReceiverComponent::UnbindStreamEvents()
{
	if (Stream.IsValid())
	{
		Stream->OnConnected.Clear();
		Stream->OnDisconnected.Clear();
		Stream->OnResolutionChanged.Clear();
		Stream->OnFormatChanged.Clear();
		Stream->OnFrameReceived.Clear();
		Stream->OnError.Clear();
	}
}

void UCyGameCaptureSpoutReceiverComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoConnect)
	{
		StartReceiver();
	}
}

void UCyGameCaptureSpoutReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopReceiver();
	Super::EndPlay(EndPlayReason);
}

void UCyGameCaptureSpoutReceiverComponent::OnUnregister()
{
	StopReceiver();
	Super::OnUnregister();
}

void UCyGameCaptureSpoutReceiverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Stream.IsValid() && Stream->IsConnected())
	{
		// Target may have been swapped by the user or not sized yet
		EnsureTargetMatches(Stream->GetConnectedSenderInfo());
		Stream->SetTargetRenderTarget(GetReceiverTexture());
	}
}
