// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

#include "CyGameCaptureSpoutSenderComponent.h"
#include "CyGameCaptureSubsystem.h"
#include "CyGameCaptureSettings.h"
#include "CyGameCaptureLog.h"
#include "RHI/CyRHICompat.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"

UCyGameCaptureSpoutSenderComponent::UCyGameCaptureSpoutSenderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;   // after cameras / scene captures updated
	bAutoActivate = true;
}

FString UCyGameCaptureSpoutSenderComponent::ResolveRequestedName() const
{
	if (bUseCustomSenderName && !SenderName.IsEmpty())
	{
		return SenderName;
	}
	if (UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get())
	{
		return Subsystem->MakeDefaultSenderName(StreamName);
	}
	return FString::Printf(TEXT("CyGameCaptureUE::%s"), *StreamName);
}

bool UCyGameCaptureSpoutSenderComponent::PrepareSource(FString& OutError)
{
	ActiveSceneCapture = nullptr;
	switch (SourceMode)
	{
	case ECyGameCaptureSourceMode::RenderTarget:
		if (RenderTarget == nullptr)
		{
			// Allowed: the stream waits until a render target is assigned
			CYGC_DEBUG(TEXT("Sender '%s': no render target yet, waiting"), *StreamName);
		}
		return true;

	case ECyGameCaptureSourceMode::SceneCapture:
	{
		USceneCaptureComponent2D* Capture = SceneCapture;
		if (Capture == nullptr)
		{
			// Managed scene capture, attached to the owner (same transform as the actor)
			AActor* OwnerActor = GetOwner();
			if (OwnerActor == nullptr)
			{
				OutError = TEXT("Scene Capture mode needs an owner actor");
				return false;
			}
			if (ManagedSceneCapture == nullptr)
			{
				ManagedSceneCapture = NewObject<USceneCaptureComponent2D>(OwnerActor, NAME_None, RF_Transient);
				ManagedSceneCapture->bCaptureEveryFrame = true;
				ManagedSceneCapture->bCaptureOnMovement = false;
				ManagedSceneCapture->CaptureSource = SCS_FinalColorLDR;
				ManagedSceneCapture->SetupAttachment(OwnerActor->GetRootComponent());
				ManagedSceneCapture->RegisterComponent();
			}
			Capture = ManagedSceneCapture;
		}
		if (Capture->TextureTarget == nullptr)
		{
			if (ManagedRenderTarget == nullptr)
			{
				ManagedRenderTarget = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
				ManagedRenderTarget->RenderTargetFormat = SceneCaptureFormat;
				ManagedRenderTarget->ClearColor = FLinearColor::Black;
				ManagedRenderTarget->bAutoGenerateMips = false;
				ManagedRenderTarget->InitAutoFormat(FMath::Max(16, SceneCaptureSize.X), FMath::Max(16, SceneCaptureSize.Y));
				ManagedRenderTarget->UpdateResourceImmediate(true);
			}
			Capture->TextureTarget = ManagedRenderTarget;
		}
		ActiveSceneCapture = Capture;
		return true;
	}

	case ECyGameCaptureSourceMode::GameViewport:
	default:
		return true;
	}
}

void UCyGameCaptureSpoutSenderComponent::ReleaseManagedSource()
{
	if (ManagedSceneCapture != nullptr)
	{
		ManagedSceneCapture->DestroyComponent();
		ManagedSceneCapture = nullptr;
	}
	ManagedRenderTarget = nullptr;
	ActiveSceneCapture = nullptr;
}

bool UCyGameCaptureSpoutSenderComponent::StartSender()
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
		CYGC_LOG(Log, TEXT("Sender '%s' not started: editor capture disabled in project settings"), *StreamName);
		return false;
	}
#endif

	FString Error;
	if (!PrepareSource(Error))
	{
		CYGC_LOG(Warning, TEXT("Sender '%s': %s"), *StreamName, *Error);
		OnError.Broadcast(Error);
		return false;
	}

	FString FinalName;
	if (!Subsystem->ResolveSenderName(ResolveRequestedName(), NameCollisionPolicy, FinalName, Error))
	{
		CYGC_LOG(Warning, TEXT("Sender '%s': %s"), *StreamName, *Error);
		OnError.Broadcast(Error);
		return false;
	}

	FCySenderStreamConfig Config;
	Config.SenderName = FinalName;
	Config.SourceMode = SourceMode;
	Config.FrameRateMode = FrameRateMode;
	Config.CustomFrameRate = CustomFrameRate;
	Config.bFlushAfterCopy = bFlushAfterCopy;
	Config.bDebug = bDebug;

	Stream = Subsystem->CreateSender(Config, this, Error);
	if (!Stream.IsValid())
	{
		CYGC_LOG(Warning, TEXT("Sender '%s' failed to start: %s"), *StreamName, *Error);
		OnError.Broadcast(Error);
		return false;
	}
	BindStreamEvents();
	Stream->SetSourceRenderTarget(GetSourceRenderTarget());
	return true;
}

void UCyGameCaptureSpoutSenderComponent::StopSender()
{
	if (!Stream.IsValid())
	{
		return;
	}
	UnbindStreamEvents();
	if (UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get())
	{
		Subsystem->DestroySender(Stream);
	}
	else
	{
		Stream->Stop();
	}
	Stream.Reset();
	OnStopped.Broadcast();
}

bool UCyGameCaptureSpoutSenderComponent::IsSenderActive() const
{
	return Stream.IsValid() && Stream->IsActive();
}

void UCyGameCaptureSpoutSenderComponent::SetSenderName(const FString& NewSenderName)
{
	bUseCustomSenderName = true;
	SenderName = NewSenderName;
	if (Stream.IsValid())
	{
		StopSender();
		StartSender();
	}
}

void UCyGameCaptureSpoutSenderComponent::SetRenderTarget(UTextureRenderTarget2D* NewRenderTarget)
{
	const bool bModeChanged = SourceMode != ECyGameCaptureSourceMode::RenderTarget;
	SourceMode = ECyGameCaptureSourceMode::RenderTarget;
	RenderTarget = NewRenderTarget;
	if (Stream.IsValid())
	{
		if (bModeChanged)
		{
			StopSender();
			StartSender();
		}
		else
		{
			Stream->SetSourceRenderTarget(NewRenderTarget);
		}
	}
}

void UCyGameCaptureSpoutSenderComponent::SetSceneCapture(USceneCaptureComponent2D* NewSceneCapture)
{
	SourceMode = ECyGameCaptureSourceMode::SceneCapture;
	SceneCapture = NewSceneCapture;
	if (Stream.IsValid())
	{
		StopSender();
		StartSender();
	}
}

void UCyGameCaptureSpoutSenderComponent::UseGameViewport()
{
	SourceMode = ECyGameCaptureSourceMode::GameViewport;
	if (Stream.IsValid())
	{
		StopSender();
		StartSender();
	}
}

FString UCyGameCaptureSpoutSenderComponent::GetActiveSenderName() const
{
	return Stream.IsValid() ? Stream->GetSenderName() : FString();
}

FIntPoint UCyGameCaptureSpoutSenderComponent::GetSenderResolution() const
{
	return Stream.IsValid() ? Stream->GetResolution() : FIntPoint::ZeroValue;
}

FString UCyGameCaptureSpoutSenderComponent::GetSenderFormat() const
{
	return Stream.IsValid() ? Stream->GetFormatName() : FString();
}

float UCyGameCaptureSpoutSenderComponent::GetSenderFPS() const
{
	return Stream.IsValid() ? Stream->GetFPS() : 0.0f;
}

FCyGameCaptureStreamStats UCyGameCaptureSpoutSenderComponent::GetSenderStats() const
{
	return Stream.IsValid() ? Stream->GetStats() : FCyGameCaptureStreamStats();
}

UTextureRenderTarget2D* UCyGameCaptureSpoutSenderComponent::GetSourceRenderTarget() const
{
	switch (SourceMode)
	{
	case ECyGameCaptureSourceMode::RenderTarget:
		return RenderTarget;
	case ECyGameCaptureSourceMode::SceneCapture:
		return ActiveSceneCapture != nullptr ? ActiveSceneCapture->TextureTarget : nullptr;
	default:
		return nullptr;
	}
}

void UCyGameCaptureSpoutSenderComponent::BindStreamEvents()
{
	if (!Stream.IsValid())
	{
		return;
	}
	TWeakObjectPtr<UCyGameCaptureSpoutSenderComponent> WeakThis(this);
	Stream->OnStarted.AddLambda([WeakThis](FCySenderStream&) { if (WeakThis.IsValid()) { WeakThis->OnStarted.Broadcast(); } });
	Stream->OnResolutionChanged.AddLambda([WeakThis](FCySenderStream& S) { if (WeakThis.IsValid()) { const FIntPoint R = S.GetResolution(); WeakThis->OnResolutionChanged.Broadcast(R.X, R.Y); } });
	Stream->OnError.AddLambda([WeakThis](FCySenderStream&, const FString& Error) { if (WeakThis.IsValid()) { WeakThis->OnError.Broadcast(Error); } });
}

void UCyGameCaptureSpoutSenderComponent::UnbindStreamEvents()
{
	if (Stream.IsValid())
	{
		Stream->OnStarted.Clear();
		Stream->OnStopped.Clear();
		Stream->OnResolutionChanged.Clear();
		Stream->OnError.Clear();
	}
}

void UCyGameCaptureSpoutSenderComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoStart)
	{
		StartSender();
	}
}

void UCyGameCaptureSpoutSenderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSender();
	ReleaseManagedSource();
	Super::EndPlay(EndPlayReason);
}

void UCyGameCaptureSpoutSenderComponent::OnUnregister()
{
	StopSender();
	Super::OnUnregister();
}

void UCyGameCaptureSpoutSenderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bRestartRequested)
	{
		bRestartRequested = false;
		if (Stream.IsValid())
		{
			StopSender();
			StartSender();
		}
	}
	if (Stream.IsValid())
	{
		// The source can change at any time (Blueprint assigning a new target, scene capture target swapped)
		Stream->SetSourceRenderTarget(GetSourceRenderTarget());
	}
}

#if WITH_EDITOR
void UCyGameCaptureSpoutSenderComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (Stream.IsValid())
	{
		bRestartRequested = true;
	}
}
#endif
