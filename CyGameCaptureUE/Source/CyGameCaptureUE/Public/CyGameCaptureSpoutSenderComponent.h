// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — Spout sender component.
//
//   Render Target  ─┐
//   Scene Capture  ─┼─► GPU copy ─► shared texture ─► Spout sender "<Prefix>::<Project>::<Stream>"
//   Game Viewport  ─┘
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CyGameCaptureTypes.h"
#include "CyGameCaptureStream.h"
#include "CyGameCaptureSpoutSenderComponent.generated.h"

class UTextureRenderTarget2D;
class USceneCaptureComponent2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCyGameCaptureSenderEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCyGameCaptureSenderResolutionEvent, int32, Width, int32, Height);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCyGameCaptureSenderErrorEvent, const FString&, Error);

UCLASS(ClassGroup = (CyGameCapture), meta = (BlueprintSpawnableComponent, DisplayName = "CyGameCapture Spout Sender"))
class CYGAMECAPTUREUE_API UCyGameCaptureSpoutSenderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCyGameCaptureSpoutSenderComponent();

	// ------------------------------------------------------------------ configuration
	/** Short stream name used to build the default sender name: <Prefix>::<ProjectName>::<StreamName>. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sender")
	FString StreamName = TEXT("Stream");

	/** Use SenderName as the complete Spout name instead of the default convention. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sender")
	bool bUseCustomSenderName = false;

	/** Complete Spout sender name (when bUseCustomSenderName). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sender", meta = (EditCondition = "bUseCustomSenderName"))
	FString SenderName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sender")
	ECyGameCaptureNameCollisionPolicy NameCollisionPolicy = ECyGameCaptureNameCollisionPolicy::AutoRename;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source")
	ECyGameCaptureSourceMode SourceMode = ECyGameCaptureSourceMode::RenderTarget;

	/** Source render target (SourceMode = Render Target). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source", meta = (EditCondition = "SourceMode == ECyGameCaptureSourceMode::RenderTarget"))
	UTextureRenderTarget2D* RenderTarget = nullptr;

	/**
	 * Existing scene capture to send (SourceMode = Scene Capture). Its TextureTarget is used; one is created
	 * when missing. Leave empty to let the component create and manage its own USceneCaptureComponent2D.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source", meta = (EditCondition = "SourceMode == ECyGameCaptureSourceMode::SceneCapture"))
	USceneCaptureComponent2D* SceneCapture = nullptr;

	/** Size of the render target created for a managed scene capture / a scene capture without texture target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source", meta = (EditCondition = "SourceMode == ECyGameCaptureSourceMode::SceneCapture", ClampMin = "16"))
	FIntPoint SceneCaptureSize = FIntPoint(1920, 1080);

	/** Pixel format of the render target created for a managed scene capture (RGBA8 = SDR, FloatRGBA = HDR). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source", meta = (EditCondition = "SourceMode == ECyGameCaptureSourceMode::SceneCapture"))
	TEnumAsByte<ETextureRenderTargetFormat> SceneCaptureFormat = RTF_RGBA8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	ECyGameCaptureFrameRateMode FrameRateMode = ECyGameCaptureFrameRateMode::EveryFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (EditCondition = "FrameRateMode == ECyGameCaptureFrameRateMode::CustomFrameRate", ClampMin = "1", ClampMax = "480"))
	float CustomFrameRate = 60.0f;

	/** Start sending on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	bool bAutoStart = true;

	/** Recreate the sender after a device reset / source recreation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	bool bAutoReconnect = true;

	/** Flush the D3D11 context after each copy (see project settings). -1 = use project setting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced")
	bool bFlushAfterCopy = false;

	/** Log per-frame details for this stream. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebug = false;

	// ------------------------------------------------------------------ events
	UPROPERTY(BlueprintAssignable, Category = "CyGameCapture|Events")
	FCyGameCaptureSenderEvent OnStarted;

	UPROPERTY(BlueprintAssignable, Category = "CyGameCapture|Events")
	FCyGameCaptureSenderEvent OnStopped;

	UPROPERTY(BlueprintAssignable, Category = "CyGameCapture|Events")
	FCyGameCaptureSenderResolutionEvent OnResolutionChanged;

	UPROPERTY(BlueprintAssignable, Category = "CyGameCapture|Events")
	FCyGameCaptureSenderErrorEvent OnError;

	// ------------------------------------------------------------------ Blueprint API
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Sender")
	bool StartSender();

	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Sender")
	void StopSender();

	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Sender")
	bool IsSenderActive() const;

	/** Sets a complete custom Spout name (restarts the sender when active). */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Sender")
	void SetSenderName(const FString& NewSenderName);

	/** Switches to Render Target mode with this target. */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Sender")
	void SetRenderTarget(UTextureRenderTarget2D* NewRenderTarget);

	/** Switches to Scene Capture mode with this capture component. */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Sender")
	void SetSceneCapture(USceneCaptureComponent2D* NewSceneCapture);

	/** Switches to Game Viewport mode (final back buffer, what the player sees). */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Sender")
	void UseGameViewport();

	/** Actual Spout sender name (after collision policy / default naming). Empty when not started. */
	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Sender")
	FString GetActiveSenderName() const;

	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Sender")
	FIntPoint GetSenderResolution() const;

	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Sender")
	FString GetSenderFormat() const;

	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Sender")
	float GetSenderFPS() const;

	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Sender")
	FCyGameCaptureStreamStats GetSenderStats() const;

	/** Render target currently used as source (user one, scene capture target or the managed one). */
	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Sender")
	UTextureRenderTarget2D* GetSourceRenderTarget() const;

	/** Scene capture in use (user one or managed). */
	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Sender")
	USceneCaptureComponent2D* GetActiveSceneCapture() const { return ActiveSceneCapture; }

	// ------------------------------------------------------------------ UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	FString ResolveRequestedName() const;
	bool PrepareSource(FString& OutError);
	void ReleaseManagedSource();
	void BindStreamEvents();
	void UnbindStreamEvents();

	TSharedPtr<FCySenderStream, ESPMode::ThreadSafe> Stream;

	/** Scene capture created by this component (managed mode). */
	UPROPERTY(Transient)
	USceneCaptureComponent2D* ManagedSceneCapture = nullptr;

	/** Render target created by this component for a scene capture without target. */
	UPROPERTY(Transient)
	UTextureRenderTarget2D* ManagedRenderTarget = nullptr;

	UPROPERTY(Transient)
	USceneCaptureComponent2D* ActiveSceneCapture = nullptr;

	bool bRestartRequested = false;
};
