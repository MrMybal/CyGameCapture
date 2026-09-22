// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — native (non-UObject) stream objects.
//
// A stream is the render-thread safe core of a sender or a receiver. Components and the
// subsystem own them through thread-safe shared pointers, render commands capture the same
// shared pointers, so a stream can never be destroyed while GPU work referencing it is queued.
// UObjects (render targets, scene captures) are only ever touched on the game thread; the
// render thread receives raw FTextureRenderTargetResource / FRHITexture pointers that stay
// valid for the frame they were captured in (standard engine pattern).
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "CyGameCaptureTypes.h"

class UTextureRenderTarget2D;
class FRHITexture;
class FRHICommandListImmediate;
class ICySenderBackend;
class ICyReceiverBackend;
class FCySpoutSenderRegistration;

struct CYGAMECAPTUREUE_API FCySenderStreamConfig
{
	/** Final Spout sender name (already resolved against the collision policy). */
	FString SenderName;
	ECyGameCaptureSourceMode SourceMode = ECyGameCaptureSourceMode::RenderTarget;
	ECyGameCaptureFrameRateMode FrameRateMode = ECyGameCaptureFrameRateMode::EveryFrame;
	float CustomFrameRate = 60.0f;
	bool bFlushAfterCopy = false;
	bool bDebug = false;
};

struct CYGAMECAPTUREUE_API FCyReceiverStreamConfig
{
	/** Local name, for statistics / console listing. */
	FString ReceiverName;
	/** Spout sender to connect to. */
	FString SenderName;
	bool bAutoReconnect = true;
	bool bDebug = false;
};

/**
 * Sender stream: takes an RHI texture every frame and publishes it through Spout.
 * Game thread: Start/Stop/SetSource*, Tick. Render thread: the backend copy.
 */
class CYGAMECAPTUREUE_API FCySenderStream : public TSharedFromThis<FCySenderStream, ESPMode::ThreadSafe>
{
public:
	explicit FCySenderStream(const FCySenderStreamConfig& InConfig);
	~FCySenderStream();

	FCySenderStream(const FCySenderStream&) = delete;
	FCySenderStream& operator=(const FCySenderStream&) = delete;

	// ---- game thread API --------------------------------------------------------------------
	bool Start(FString& OutError);
	void Stop();
	bool IsActive() const { return State == ECyGameCaptureStreamState::Active || State == ECyGameCaptureStreamState::Waiting; }
	ECyGameCaptureStreamState GetState() const { return State; }

	/** Render target source (SourceMode RenderTarget / SceneCapture). Safe to call every frame. */
	void SetSourceRenderTarget(UTextureRenderTarget2D* RenderTarget);
	UTextureRenderTarget2D* GetSourceRenderTarget() const { return SourceRenderTarget.Get(); }

	/** Called by the subsystem every game frame. Issues the render command when a frame is due. */
	void Tick(float DeltaSeconds);

	/**
	 * Feed a texture from the render thread (viewport mode: back buffer delegate).
	 * Applies the frame-rate limiter and performs the copy immediately.
	 */
	void SendTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, const FIntPoint& Size, uint8 PixelFormat);

	const FCySenderStreamConfig& GetConfig() const { return Config; }
	FCyGameCaptureStreamStats GetStats() const;
	FString GetSenderName() const { return Config.SenderName; }
	FIntPoint GetResolution() const { return FIntPoint(StatsWidth.load(), StatsHeight.load()); }
	FString GetFormatName() const;
	float GetFPS() const { return CurrentFPS; }

	/** Owner object used for automatic cleanup (component / world). */
	TWeakObjectPtr<UObject> Owner;

	// Events (game thread, fired from Tick)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSenderStreamEvent, FCySenderStream&);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSenderStreamError, FCySenderStream&, const FString&);
	FOnSenderStreamEvent OnStarted;
	FOnSenderStreamEvent OnStopped;
	FOnSenderStreamEvent OnResolutionChanged;
	FOnSenderStreamError OnError;

private:
	friend class UCyGameCaptureSubsystem;

	bool IsFrameDue(double NowSeconds);
	void Copy_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, const FIntPoint& Size, uint8 PixelFormat);
	void UpdateFPS(double NowSeconds);
	void SetError(const FString& Error);

	FCySenderStreamConfig Config;
	std::atomic<ECyGameCaptureStreamState> State { ECyGameCaptureStreamState::Stopped };

	TWeakObjectPtr<UTextureRenderTarget2D> SourceRenderTarget;

	// Backend (render thread owned)
	TUniquePtr<ICySenderBackend> Backend;
	TUniquePtr<FCySpoutSenderRegistration> Registration;
	uint32 SharedWidth = 0;
	uint32 SharedHeight = 0;
	uint32 SharedDxgiFormat = 0;

	// Frame pacing (game thread for RT sources, render thread for viewport)
	double NextFrameDueTime = 0.0;
	std::atomic<bool> bCopyInFlight { false };

	// Statistics (written on the render thread, read on the game thread)
	std::atomic<int32> StatsWidth { 0 };
	std::atomic<int32> StatsHeight { 0 };
	std::atomic<uint32> StatsDxgiFormat { 0 };
	std::atomic<int64> FrameCount { 0 };
	std::atomic<int64> DroppedFrames { 0 };
	std::atomic<float> LastCopyMs { 0.0f };
	int64 FPSWindowFrames = 0;
	double FPSWindowStart = 0.0;
	float CurrentFPS = 0.0f;
	std::atomic<bool> bResolutionChangedPending { false };
	std::atomic<bool> bStartedPending { false };
	FCriticalSection ErrorLock;
	FString LastError;
	bool bErrorPending = false;
	bool bStartedNotified = false;
};

/**
 * Receiver stream: opens a Spout sender's shared texture and copies it into a render target every frame.
 */
class CYGAMECAPTUREUE_API FCyReceiverStream : public TSharedFromThis<FCyReceiverStream, ESPMode::ThreadSafe>
{
public:
	explicit FCyReceiverStream(const FCyReceiverStreamConfig& InConfig);
	~FCyReceiverStream();

	FCyReceiverStream(const FCyReceiverStream&) = delete;
	FCyReceiverStream& operator=(const FCyReceiverStream&) = delete;

	// ---- game thread API --------------------------------------------------------------------
	bool Start(FString& OutError);
	void Stop();
	bool IsActive() const { return State == ECyGameCaptureStreamState::Active || State == ECyGameCaptureStreamState::Waiting; }
	bool IsConnected() const { return bConnected.load(); }
	ECyGameCaptureStreamState GetState() const { return State; }

	void SetSenderName(const FString& InSenderName);
	FString GetSenderName() const { return Config.SenderName; }

	/** Destination render target. The stream never resizes it; the component does (AutoResize). */
	void SetTargetRenderTarget(UTextureRenderTarget2D* RenderTarget);
	UTextureRenderTarget2D* GetTargetRenderTarget() const { return TargetRenderTarget.Get(); }

	/** Current sender description (valid when connected). */
	FCySpoutSenderInfo GetConnectedSenderInfo() const;

	/** Called by the subsystem every game frame: polls the sender info, issues the copy command. */
	void Tick(float DeltaSeconds);

	const FCyReceiverStreamConfig& GetConfig() const { return Config; }
	FCyGameCaptureStreamStats GetStats() const;
	float GetFPS() const { return CurrentFPS; }

	TWeakObjectPtr<UObject> Owner;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnReceiverStreamEvent, FCyReceiverStream&);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnReceiverStreamError, FCyReceiverStream&, const FString&);
	FOnReceiverStreamEvent OnConnected;
	FOnReceiverStreamEvent OnDisconnected;
	FOnReceiverStreamEvent OnFrameReceived;      // throttled by the component
	FOnReceiverStreamEvent OnResolutionChanged;
	FOnReceiverStreamEvent OnFormatChanged;
	FOnReceiverStreamError OnError;

private:
	friend class UCyGameCaptureSubsystem;

	void PollSender(double NowSeconds);
	void Connect(const FCySpoutSenderInfo& Info, void* ShareHandle);
	void Disconnect(bool bNotify);
	void Copy_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* Target, const FIntPoint& TargetSize, uint8 TargetPixelFormat);
	void UpdateFPS(double NowSeconds);
	void SetError(const FString& Error);

	FCyReceiverStreamConfig Config;
	std::atomic<ECyGameCaptureStreamState> State { ECyGameCaptureStreamState::Stopped };
	std::atomic<bool> bConnected { false };

	TWeakObjectPtr<UTextureRenderTarget2D> TargetRenderTarget;

	TUniquePtr<ICyReceiverBackend> Backend;
	TUniquePtr<FCySpoutSenderRegistration> Access;   // access mutex / frame count of the remote sender
	FCySpoutSenderInfo ConnectedInfo;
	void* ConnectedHandle = nullptr;
	double NextPollTime = 0.0;
	std::atomic<bool> bCopyInFlight { false };

	std::atomic<int64> FrameCount { 0 };
	std::atomic<int64> DroppedFrames { 0 };
	std::atomic<float> LastCopyMs { 0.0f };
	std::atomic<bool> bFrameReceivedPending { false };
	int64 FPSWindowFrames = 0;
	double FPSWindowStart = 0.0;
	float CurrentFPS = 0.0f;
	FCriticalSection ErrorLock;
	FString LastError;
	bool bErrorPending = false;
	bool bResolutionChangedPending = false;
	bool bFormatChangedPending = false;
};
