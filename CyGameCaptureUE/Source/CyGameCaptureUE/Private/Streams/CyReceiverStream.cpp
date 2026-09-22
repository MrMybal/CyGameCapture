// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

#include "CyGameCaptureStream.h"
#include "CyGameCaptureLog.h"
#include "CyGameCaptureSettings.h"
#include "RHI/CyRHICompat.h"
#include "Spout/CySpoutBackend.h"
#include "Spout/CySpoutRegistry.h"

#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "Misc/ScopeLock.h"

namespace
{
	void ReleaseOnRHIThread(ICyReceiverBackend* Backend, FCySpoutSenderRegistration* Access)
	{
		if (Backend == nullptr && Access == nullptr)
		{
			return;
		}
		ENQUEUE_RENDER_COMMAND(CySpoutReceiverRelease)([Backend, Access](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.EnqueueLambda([Backend, Access](FRHICommandListBase&)
			{
				if (Backend != nullptr)
				{
					Backend->Release();
					delete Backend;
				}
				if (Access != nullptr)
				{
					Access->Release();
					delete Access;
				}
			});
		});
	}
}

FCyReceiverStream::FCyReceiverStream(const FCyReceiverStreamConfig& InConfig) :
	Config(InConfig)
{
}

FCyReceiverStream::~FCyReceiverStream()
{
	State = ECyGameCaptureStreamState::Stopped;
	bConnected = false;
	ReleaseOnRHIThread(Backend.Release(), Access.Release());
}

bool FCyReceiverStream::Start(FString& OutError)
{
	check(IsInGameThread());
	if (IsActive())
	{
		return true;
	}
	Backend = CySpoutBackend::CreateReceiver(OutError);
	if (!Backend.IsValid())
	{
		SetError(OutError);
		State = ECyGameCaptureStreamState::Error;
		return false;
	}
	Access = MakeUnique<FCySpoutSenderRegistration>();
	ConnectedInfo = FCySpoutSenderInfo();
	ConnectedHandle = nullptr;
	NextPollTime = 0.0;
	FPSWindowStart = FPlatformTime::Seconds();
	FPSWindowFrames = 0;
	CurrentFPS = 0.0f;
	{
		FScopeLock Lock(&ErrorLock);
		LastError.Reset();
		bErrorPending = false;
	}
	State = ECyGameCaptureStreamState::Waiting;
	CYGC_LOG(Log, TEXT("Receiver '%s' starting, looking for Spout sender '%s'"), *Config.ReceiverName, *Config.SenderName);
	return true;
}

void FCyReceiverStream::Stop()
{
	check(IsInGameThread());
	if (State == ECyGameCaptureStreamState::Stopped)
	{
		return;
	}
	const bool bWasConnected = bConnected.load();
	State = ECyGameCaptureStreamState::Stopped;
	bConnected = false;
	ReleaseOnRHIThread(Backend.Release(), Access.Release());
	CYGC_LOG(Log, TEXT("Receiver '%s' stopped (%lld frames received)"), *Config.ReceiverName, FrameCount.load());
	if (bWasConnected)
	{
		OnDisconnected.Broadcast(*this);
	}
}

void FCyReceiverStream::SetSenderName(const FString& InSenderName)
{
	check(IsInGameThread());
	if (Config.SenderName == InSenderName)
	{
		return;
	}
	Config.SenderName = InSenderName;
	if (IsActive())
	{
		Disconnect(true);
		NextPollTime = 0.0;
	}
}

void FCyReceiverStream::SetTargetRenderTarget(UTextureRenderTarget2D* RenderTarget)
{
	TargetRenderTarget = RenderTarget;
}

FCySpoutSenderInfo FCyReceiverStream::GetConnectedSenderInfo() const
{
	return ConnectedInfo;
}

void FCyReceiverStream::Tick(float /*DeltaSeconds*/)
{
	check(IsInGameThread());
	if (!IsActive())
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();

	// Deferred notifications
	{
		FString Error;
		{
			FScopeLock Lock(&ErrorLock);
			if (bErrorPending)
			{
				Error = LastError;
				bErrorPending = false;
			}
		}
		if (!Error.IsEmpty())
		{
			OnError.Broadcast(*this, Error);
		}
	}
	if (bResolutionChangedPending)
	{
		bResolutionChangedPending = false;
		OnResolutionChanged.Broadcast(*this);
	}
	if (bFormatChangedPending)
	{
		bFormatChangedPending = false;
		OnFormatChanged.Broadcast(*this);
	}

	PollSender(Now);
	UpdateFPS(Now);

	if (!bConnected.load())
	{
		return;
	}

	UTextureRenderTarget2D* RenderTarget = TargetRenderTarget.Get();
	if (RenderTarget == nullptr || RenderTarget->SizeX <= 0 || RenderTarget->SizeY <= 0)
	{
		return;
	}
	// Size / format must match the sender: the component resizes the target, this only waits for it
	const uint32 TargetDxgi = CyPixelFormatToDxgi(RenderTarget->GetFormat());
	if (RenderTarget->SizeX != ConnectedInfo.Width || RenderTarget->SizeY != ConnectedInfo.Height || !CyDxgiCopyCompatible(TargetDxgi, static_cast<uint32>(ConnectedInfo.DxgiFormat)))
	{
		return;
	}
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (Resource == nullptr)
	{
		return;
	}
	if (bCopyInFlight.load())
	{
		DroppedFrames++;
		return;
	}
	bCopyInFlight = true;

	const FIntPoint Size(RenderTarget->SizeX, RenderTarget->SizeY);
	TSharedRef<FCyReceiverStream, ESPMode::ThreadSafe> Self = AsShared();
	ENQUEUE_RENDER_COMMAND(CySpoutReceiverCopy)([Self, Resource, Size, TargetDxgi](FRHICommandListImmediate& RHICmdList)
	{
		FCyTextureRef Texture = Resource->GetRenderTargetTexture();
		if (Texture.IsValid())
		{
			Self->Copy_RenderThread(RHICmdList, Texture.GetReference(), Size, static_cast<uint8>(TargetDxgi));
		}
		Self->bCopyInFlight = false;
	});

	const int64 Completed = Backend.IsValid() ? Backend->GetCompletedFrames() : 0;
	if (Completed != FrameCount.load())
	{
		FrameCount = Completed;
		OnFrameReceived.Broadcast(*this);
	}
}

void FCyReceiverStream::PollSender(double NowSeconds)
{
	const UCyGameCaptureSettings* Settings = UCyGameCaptureSettings::Get();
	const double Interval = Settings != nullptr ? FMath::Max(0.02f, Settings->ReceiverPollIntervalSeconds) : 0.25;
	if (NowSeconds < NextPollTime)
	{
		return;
	}
	NextPollTime = NowSeconds + Interval;

	FCySpoutSenderInfo Info;
	void* Handle = nullptr;
	const bool bFound = CySpout::GetSenderInfo(Config.SenderName, Info, Handle) && Handle != nullptr && Info.Width > 0 && Info.Height > 0;

	if (!bFound)
	{
		if (bConnected.load())
		{
			CYGC_LOG(Log, TEXT("Receiver '%s': sender '%s' disappeared"), *Config.ReceiverName, *Config.SenderName);
			Disconnect(true);
		}
		return;
	}

	const bool bChanged = !bConnected.load() || Handle != ConnectedHandle || Info.Width != ConnectedInfo.Width || Info.Height != ConnectedInfo.Height || Info.DxgiFormat != ConnectedInfo.DxgiFormat;
	if (bChanged)
	{
		Connect(Info, Handle);
	}
}

void FCyReceiverStream::Connect(const FCySpoutSenderInfo& Info, void* ShareHandle)
{
	const bool bWasConnected = bConnected.load();
	const FCySpoutSenderInfo Previous = ConnectedInfo;

	if (!Access->IsOpen() || Access->GetName() != Config.SenderName)
	{
		Access->OpenForReceiving(Config.SenderName);
	}

	FString Error;
	// Open synchronously on the render thread: the handle is a process-wide object, opening is cheap
	bool bOpened = false;
	ICyReceiverBackend* BackendPtr = Backend.Get();
	FString* ErrorPtr = &Error;
	bool* OpenedPtr = &bOpened;
	ENQUEUE_RENDER_COMMAND(CySpoutReceiverOpen)([BackendPtr, ShareHandle, Info, ErrorPtr, OpenedPtr](FRHICommandListImmediate&)
	{
		*OpenedPtr = BackendPtr->Open(ShareHandle, static_cast<uint32>(Info.Width), static_cast<uint32>(Info.Height), static_cast<uint32>(Info.DxgiFormat), *ErrorPtr);
	});
	FlushRenderingCommands();   // connection event only (not per frame)

	if (!bOpened)
	{
		SetError(Error);
		if (bWasConnected)
		{
			Disconnect(true);
		}
		return;
	}

	ConnectedInfo = Info;
	ConnectedHandle = ShareHandle;
	bConnected = true;
	State = ECyGameCaptureStreamState::Active;
	CYGC_LOG(Log, TEXT("Receiver '%s' connected to '%s' %dx%d %s"), *Config.ReceiverName, *Info.Name, Info.Width, Info.Height, *Info.FormatName);

	if (!bWasConnected)
	{
		OnConnected.Broadcast(*this);
	}
	else
	{
		if (Previous.Width != Info.Width || Previous.Height != Info.Height)
		{
			bResolutionChangedPending = true;
		}
		if (Previous.DxgiFormat != Info.DxgiFormat)
		{
			bFormatChangedPending = true;
		}
	}
}

void FCyReceiverStream::Disconnect(bool bNotify)
{
	if (!bConnected.load())
	{
		return;
	}
	bConnected = false;
	ConnectedHandle = nullptr;
	State = ECyGameCaptureStreamState::Waiting;
	if (bNotify)
	{
		OnDisconnected.Broadcast(*this);
	}
}

void FCyReceiverStream::Copy_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* Target, const FIntPoint& TargetSize, uint8 /*TargetPixelFormat*/)
{
	if (!Backend.IsValid() || !Access.IsValid() || Target == nullptr || !bConnected.load())
	{
		return;
	}
	const uint32 TargetDxgi = CyPixelFormatToDxgi(CyGetTextureFormat(Target));
	const FCyTextureRef TargetRef(Target);

	RHICmdList.Transition(FRHITransitionInfo(Target, ERHIAccess::Unknown, ERHIAccess::CopyDest));
	if (!Backend->Copy(RHICmdList, TargetRef, TargetSize, TargetDxgi, Access.Get()))
	{
		DroppedFrames++;
	}
	RHICmdList.Transition(FRHITransitionInfo(Target, ERHIAccess::CopyDest, ERHIAccess::SRVMask));
}

void FCyReceiverStream::UpdateFPS(double NowSeconds)
{
	const int64 Completed = Backend.IsValid() ? Backend->GetCompletedFrames() : 0;
	if (NowSeconds - FPSWindowStart >= 0.5)
	{
		CurrentFPS = static_cast<float>((Completed - FPSWindowFrames) / (NowSeconds - FPSWindowStart));
		FPSWindowFrames = Completed;
		FPSWindowStart = NowSeconds;
	}
}

void FCyReceiverStream::SetError(const FString& Error)
{
	FScopeLock Lock(&ErrorLock);
	LastError = Error;
	bErrorPending = true;
	CYGC_LOG(Warning, TEXT("Receiver '%s': %s"), *Config.ReceiverName, *Error);
}

FCyGameCaptureStreamStats FCyReceiverStream::GetStats() const
{
	FCyGameCaptureStreamStats Stats;
	Stats.Name = Config.SenderName;
	Stats.State = State;
	Stats.bConnected = bConnected.load();
	Stats.Width = ConnectedInfo.Width;
	Stats.Height = ConnectedInfo.Height;
	Stats.Format = ConnectedInfo.FormatName;
	Stats.FPS = CurrentFPS;
	Stats.FrameCount = Backend.IsValid() ? Backend->GetCompletedFrames() : 0;
	Stats.DroppedFrames = DroppedFrames.load() + (Backend.IsValid() ? Backend->GetDroppedFrames() : 0);
	Stats.CopyTimeMs = Backend.IsValid() ? Backend->GetLastCopyMs() : 0.0f;
	{
		FScopeLock Lock(const_cast<FCriticalSection*>(&ErrorLock));
		Stats.LastError = LastError;
	}
	return Stats;
}
