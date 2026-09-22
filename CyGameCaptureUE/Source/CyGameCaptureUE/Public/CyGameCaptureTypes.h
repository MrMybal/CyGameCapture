// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — public types shared by components, subsystem and Blueprint library.
#pragma once

#include "CoreMinimal.h"
#include "CyGameCaptureTypes.generated.h"

/** Where a sender takes its image from. */
UENUM(BlueprintType)
enum class ECyGameCaptureSourceMode : uint8
{
	/** A UTextureRenderTarget2D (drawn by anything: SceneCapture, Canvas, materials, ...). */
	RenderTarget    UMETA(DisplayName = "Render Target"),
	/** A USceneCaptureComponent2D. Either an existing one (with its own render target) or one managed by the component. */
	SceneCapture    UMETA(DisplayName = "Scene Capture"),
	/** The final game viewport (back buffer after Slate/UMG): exactly what the player sees, no second scene render. */
	GameViewport    UMETA(DisplayName = "Game Viewport"),
};

UENUM(BlueprintType)
enum class ECyGameCaptureFrameRateMode : uint8
{
	/** Send every rendered frame. */
	EveryFrame,
	/** Send at most CustomFrameRate frames per second (skipped frames cost no GPU work). */
	CustomFrameRate,
};

/** What to do when a Spout sender with the requested name already exists. */
UENUM(BlueprintType)
enum class ECyGameCaptureNameCollisionPolicy : uint8
{
	/** Do not start, report an error. */
	Fail,
	/** Append ::2, ::3 ... until a free name is found. */
	AutoRename,
	/** Take the name over only when the existing sender belongs to this process (e.g. a previous PIE session), otherwise fail. */
	ReplaceIfOwnedByThisProcess,
};

UENUM(BlueprintType)
enum class ECyGameCaptureReceiverOutputMode : uint8
{
	/** Copy into the render target assigned by the user (TargetRenderTarget). */
	UserRenderTarget,
	/** The receiver owns a transient render target that follows the sender size/format; get it with GetReceiverTexture(). */
	InternalRenderTarget,
};

UENUM(BlueprintType)
enum class ECyGameCaptureStreamState : uint8
{
	Stopped,
	/** Sender: waiting for the source / first frame. Receiver: waiting for the Spout sender to appear. */
	Waiting,
	Active,
	Error,
};

/** Statistics of one stream (sender or receiver). Updated once per frame, read on the game thread. */
USTRUCT(BlueprintType)
struct CYGAMECAPTUREUE_API FCyGameCaptureStreamStats
{
	GENERATED_BODY()

	/** Spout sender name (for a receiver: the name it is connected to). */
	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	ECyGameCaptureStreamState State = ECyGameCaptureStreamState::Stopped;

	/** Sender: shared texture registered. Receiver: sender found and shared texture opened. */
	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	bool bConnected = false;

	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	int32 Width = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	int32 Height = 0;

	/** DXGI format name of the shared texture (e.g. B8G8R8A8_UNORM). */
	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	FString Format;

	/** Frames actually sent / received per second (measured over the last second). */
	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	float FPS = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	int64 FrameCount = 0;

	/** Frames skipped because a receiver/sender held the Spout access mutex too long or the source was not ready. */
	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	int64 DroppedFrames = 0;

	/** CPU time spent issuing the GPU copy on the render/RHI thread, in milliseconds (last frame). */
	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	float CopyTimeMs = 0.0f;

	/** Last error message, empty when everything is fine. */
	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	FString LastError;
};

/** Description of a Spout sender visible on this machine (any application). */
USTRUCT(BlueprintType)
struct CYGAMECAPTUREUE_API FCySpoutSenderInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	int32 Width = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	int32 Height = 0;

	/** DXGI_FORMAT value published by the sender (28 = R8G8B8A8_UNORM, 87 = B8G8R8A8_UNORM, 10 = R16G16B16A16_FLOAT, 24 = R10G10B10A2_UNORM). */
	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	int32 DxgiFormat = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	FString FormatName;

	/** Executable that created the sender, when published (Spout writes it into the sender description). */
	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	FString OwnerExecutable;

	/** True when the sender was created by this very process (e.g. another component, or a previous PIE session). */
	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	bool bOwnedByThisProcess = false;

	/** True for senders following the CyGameCapture naming convention (CyGameCaptureUE::..., CyGameCaptureRS::...). */
	UPROPERTY(BlueprintReadOnly, Category = "CyGameCapture")
	bool bIsCyGameCaptureSender = false;
};
