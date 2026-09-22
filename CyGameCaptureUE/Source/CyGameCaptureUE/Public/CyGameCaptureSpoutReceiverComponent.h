// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — Spout receiver component.
//
//   Spout sender (OBS, Resolume, another Unreal, CyGameCaptureRS, ...) ─► shared texture ─► GPU copy ─► UTextureRenderTarget2D
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CyGameCaptureTypes.h"
#include "CyGameCaptureStream.h"
#include "CyGameCaptureSpoutReceiverComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCyGameCaptureReceiverEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCyGameCaptureReceiverResolutionEvent, int32, Width, int32, Height);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCyGameCaptureReceiverFormatEvent, const FString&, Format);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCyGameCaptureReceiverErrorEvent, const FString&, Error);

UCLASS(ClassGroup = (CyGameCapture), meta = (BlueprintSpawnableComponent, DisplayName = "CyGameCapture Spout Receiver"))
class CYGAMECAPTUREUE_API UCyGameCaptureSpoutReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCyGameCaptureSpoutReceiverComponent();

	// ------------------------------------------------------------------ configuration
	/** Local name shown in statistics / console. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receiver")
	FString ReceiverName = TEXT("Receiver");

	/** Spout sender to connect to (any application), e.g. "OBS_Program" or "CyGameCaptureRS::Beyond". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Receiver")
	FString SenderName;

	/** Connect on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	bool bAutoConnect = true;

	/** Keep looking for the sender when it is not there yet / reconnect when it comes back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	bool bAutoReconnect = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	ECyGameCaptureReceiverOutputMode OutputMode = ECyGameCaptureReceiverOutputMode::InternalRenderTarget;

	/** Destination (OutputMode = User Render Target). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (EditCondition = "OutputMode == ECyGameCaptureReceiverOutputMode::UserRenderTarget"))
	UTextureRenderTarget2D* TargetRenderTarget = nullptr;

	/** Resize / re-format the destination render target automatically to match the sender. Always on for the internal target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	bool bResizeTargetAutomatically = true;

	/** Rate of the OnFrameReceived Blueprint event in Hz (0 = disabled, avoids 120 Hz game-thread events). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour", meta = (ClampMin = "0", ClampMax = "240"))
	float FrameEventRateHz = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebug = false;

	// ------------------------------------------------------------------ events
	UPROPERTY(BlueprintAssignable, Category = "CyGameCapture|Events")
	FCyGameCaptureReceiverEvent OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "CyGameCapture|Events")
	FCyGameCaptureReceiverEvent OnDisconnected;

	/** Throttled by FrameEventRateHz. */
	UPROPERTY(BlueprintAssignable, Category = "CyGameCapture|Events")
	FCyGameCaptureReceiverEvent OnFrameReceived;

	UPROPERTY(BlueprintAssignable, Category = "CyGameCapture|Events")
	FCyGameCaptureReceiverResolutionEvent OnResolutionChanged;

	UPROPERTY(BlueprintAssignable, Category = "CyGameCapture|Events")
	FCyGameCaptureReceiverFormatEvent OnFormatChanged;

	UPROPERTY(BlueprintAssignable, Category = "CyGameCapture|Events")
	FCyGameCaptureReceiverErrorEvent OnError;

	// ------------------------------------------------------------------ Blueprint API
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Receiver")
	bool StartReceiver();

	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Receiver")
	void StopReceiver();

	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Receiver")
	bool IsReceiverActive() const;

	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Receiver")
	bool IsConnected() const;

	/** Changes the sender (reconnects when active). */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Receiver")
	void SetSenderName(const FString& NewSenderName);

	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Receiver")
	void SetTargetRenderTarget(UTextureRenderTarget2D* NewTarget);

	/** Texture holding the received image: the user target or the internal render target. Use it in materials / UMG brushes. */
	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Receiver")
	UTextureRenderTarget2D* GetReceiverTexture() const;

	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Receiver")
	FIntPoint GetReceiverResolution() const;

	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Receiver")
	FString GetReceiverFormat() const;

	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Receiver")
	float GetReceiverFPS() const;

	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Receiver")
	FCyGameCaptureStreamStats GetReceiverStats() const;

	// ------------------------------------------------------------------ UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void EnsureTargetMatches(const FCySpoutSenderInfo& Info);
	void BindStreamEvents();
	void UnbindStreamEvents();

	TSharedPtr<FCyReceiverStream, ESPMode::ThreadSafe> Stream;

	UPROPERTY(Transient)
	UTextureRenderTarget2D* InternalRenderTarget = nullptr;

	double NextFrameEventTime = 0.0;
};
