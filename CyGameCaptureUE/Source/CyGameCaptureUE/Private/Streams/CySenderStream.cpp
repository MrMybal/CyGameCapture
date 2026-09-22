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
#include "UObject/Class.h"

namespace
{
	/** Hands backend + registration to the RHI thread for destruction after every queued copy ran. */
	void ReleaseOnRHIThread(ICySenderBackend* Backend, FCySpoutSenderRegistration* Registration)
	{
		if (Backend == nullptr && Registration == nullptr)
		{
			return;
		}
		ENQUEUE_RENDER_COMMAND(CySpoutSenderRelease)([Backend, Registration](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.EnqueueLambda([Backend, Registration](FRHICommandListBase&)
			{
				if (Backend != nullptr)
				{
					Backend->Release();
					delete Backend;
				}
				if (Registration != nullptr)
				{
					Registration->Release();
					delete Registration;
				}
			});
		});
	}
}

FCySenderStream::FCySenderStream(const FCySenderStreamConfig& InConfig) :
	Config(InConfig)
{
}

FCySenderStream::~FCySenderStream()
{
	State = ECyGameCaptureStreamState::Stopped;
	ReleaseOnRHIThread(Backend.Release(), Registration.Release());
}

bool FCySenderStream::Start(FString& OutError)
{
	check(IsInGameThread());
	if (IsActive())
	{
		return true;
	}
	if (Config.SenderName.IsEmpty())
	{
		OutError = TEXT("Empty sender name");
		return false;
	}

	Backend = CySpoutBackend::CreateSender(OutError);
	if (!Backend.IsValid())
	{
		SetError(OutError);
		State = ECyGameCaptureStreamState::Error;
		return false;
	}
	Registration = MakeUnique<FCySpoutSenderRegistration>();
	SharedWidth = SharedHeight = SharedDxgiFormat = 0;
	NextFrameDueTime = 0.0;
	FPSWindowStart = FPlatformTime::Seconds();
	FPSWindowFrames = 0;
	CurrentFPS = 0.0f;
	bStartedNotified = false;
	{
		FScopeLock Lock(&ErrorLock);
		LastError.Reset();
		bErrorPending = false;
	}
	State = ECyGameCaptureStreamState::Waiting;
	CYGC_LOG(Log, TEXT("Sender '%s' starting (%s)"), *Config.SenderName, *UEnum::GetValueAsString(Config.SourceMode));
	return true;
}

void FCySenderStream::Stop()
{
	check(IsInGameThread());
	if (State == ECyGameCaptureStreamState::Stopped)
	{
		return;
	}
	State = ECyGameCaptureStreamState::Stopped;
	ReleaseOnRHIThread(Backend.Release(), Registration.Release());
	CYGC_LOG(Log, TEXT("Sender '%s' stopped (%lld frames sent)"), *Config.SenderName, FrameCount.load());
	OnStopped.Broadcast(*this);
}

void FCySenderStream::SetSourceRenderTarget(UTextureRenderTarget2D* RenderTarget)
{
	SourceRenderTarget = RenderTarget;
}

bool FCySenderStream::IsFrameDue(double NowSeconds)
{
	if (Config.FrameRateMode == ECyGameCaptureFrameRateMode::EveryFrame || Config.CustomFrameRate <= 0.0f)
	{
		return true;
	}
	if (NowSeconds < NextFrameDueTime)
	{
		return false;
	}
	const double Interval = 1.0 / static_cast<double>(Config.CustomFrameRate);
	// Keep the cadence stable, but never accumulate a backlog after a stall
	NextFrameDueTime = (NowSeconds - NextFrameDueTime > Interval) ? NowSeconds + Interval : NextFrameDueTime + Interval;
	return true;
}

void FCySenderStream::Tick(float /*DeltaSeconds*/)
{
	check(IsInGameThread());
	if (!IsActive())
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();

	// Pending notifications from the render thread
	if (bStartedPending.exchange(false))
	{
		bStartedNotified = true;
		OnStarted.Broadcast(*this);
	}
	if (bResolutionChangedPending.exchange(false))
	{
		OnResolutionChanged.Broadcast(*this);
	}
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

	UpdateFPS(Now);

	// Viewport senders are fed from the back buffer hook, nothing to issue here
	if (Config.SourceMode == ECyGameCaptureSourceMode::GameViewport)
	{
		return;
	}

	UTextureRenderTarget2D* RenderTarget = SourceRenderTarget.Get();
	if (RenderTarget == nullptr)
	{
		if (State == ECyGameCaptureStreamState::Active)
		{
			State = ECyGameCaptureStreamState::Waiting;
		}
		return;
	}
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (Resource == nullptr || RenderTarget->SizeX <= 0 || RenderTarget->SizeY <= 0)
	{
		return;
	}
	if (!IsFrameDue(Now))
	{
		return;
	}
	if (bCopyInFlight.load())
	{
		DroppedFrames++;   // render thread more than a frame behind: do not pile up commands
		return;
	}

	const FIntPoint Size(RenderTarget->SizeX, RenderTarget->SizeY);
	const uint8 PixelFormat = static_cast<uint8>(RenderTarget->GetFormat());
	bCopyInFlight = true;

	TSharedRef<FCySenderStream, ESPMode::ThreadSafe> Self = AsShared();
	ENQUEUE_RENDER_COMMAND(CySpoutSenderCopy)([Self, Resource, Size, PixelFormat](FRHICommandListImmediate& RHICmdList)
	{
		FCyTextureRef Texture = Resource->GetRenderTargetTexture();
		if (Texture.IsValid())
		{
			Self->Copy_RenderThread(RHICmdList, Texture.GetReference(), Size, PixelFormat);
		}
		Self->bCopyInFlight = false;
	});
}

void FCySenderStream::SendTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, const FIntPoint& Size, uint8 PixelFormat)
{
	check(IsInRenderingThread());
	if (!IsActive() || Texture == nullptr)
	{
		return;
	}
	if (!IsFrameDue(FPlatformTime::Seconds()))
	{
		return;
	}
	Copy_RenderThread(RHICmdList, Texture, Size, PixelFormat);
}

void FCySenderStream::Copy_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, const FIntPoint& Size, uint8 PixelFormat)
{
	if (!Backend.IsValid() || !Registration.IsValid() || Texture == nullptr || Size.X <= 0 || Size.Y <= 0)
	{
		DroppedFrames++;
		return;
	}

	const uint32 Dxgi = CyPixelFormatToDxgi(static_cast<EPixelFormat>(PixelFormat));
	if (Dxgi == CyDxgi::Unknown)
	{
		if (State != ECyGameCaptureStreamState::Error)
		{
			SetError(FString::Printf(TEXT("Unsupported source pixel format %s (use RGBA8, BGRA8, RGB10A2, RGBA16F or RGBA32F render targets)"), GPixelFormats[PixelFormat].Name));
			State = ECyGameCaptureStreamState::Error;
		}
		DroppedFrames++;
		return;
	}
	if (State == ECyGameCaptureStreamState::Error)
	{
		return;
	}

	const uint32 Width = static_cast<uint32>(Size.X);
	const uint32 Height = static_cast<uint32>(Size.Y);
	if (Width != SharedWidth || Height != SharedHeight || Dxgi != SharedDxgiFormat)
	{
		FString Error;
		if (!Backend->EnsureSharedTexture(Width, Height, Dxgi, Error))
		{
			SetError(Error);
			State = ECyGameCaptureStreamState::Error;
			return;
		}
		bool bOk;
		if (!Registration->IsCreated())
		{
			bOk = Registration->Create(Config.SenderName, Width, Height, Backend->GetShareHandle(), Dxgi, Error);
			if (bOk)
			{
				bStartedPending = true;
			}
		}
		else
		{
			bOk = Registration->Update(Width, Height, Backend->GetShareHandle(), Dxgi);
			if (!bOk)
			{
				Error = TEXT("Spout sender update failed");
			}
			CYGC_LOG(Log, TEXT("Sender '%s' resolution changed: %ux%u %s -> %ux%u %s"), *Config.SenderName, SharedWidth, SharedHeight, *CyDxgiFormatName(SharedDxgiFormat), Width, Height, *CyDxgiFormatName(Dxgi));
			bResolutionChangedPending = true;
		}
		if (!bOk)
		{
			SetError(Error);
			State = ECyGameCaptureStreamState::Error;
			return;
		}
		SharedWidth = Width;
		SharedHeight = Height;
		SharedDxgiFormat = Dxgi;
		StatsWidth = Size.X;
		StatsHeight = Size.Y;
		StatsDxgiFormat = Dxgi;
		State = ECyGameCaptureStreamState::Active;
	}

	const ERHIAccess RestoreState = Config.SourceMode == ECyGameCaptureSourceMode::GameViewport ? ERHIAccess::Present : ERHIAccess::SRVMask;
	const FCyTextureRef TextureRef(Texture);

	RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::Unknown, ERHIAccess::CopySrc));
	const bool bFlush = Config.bFlushAfterCopy || (UCyGameCaptureSettings::Get() != nullptr && UCyGameCaptureSettings::Get()->bFlushAfterCopy);
	if (Backend->Copy(RHICmdList, TextureRef, Size, Registration.Get(), bFlush))
	{
		FrameCount++;
	}
	else
	{
		DroppedFrames++;
	}
	RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::CopySrc, RestoreState));
}

void FCySenderStream::UpdateFPS(double NowSeconds)
{
	const int64 Completed = Backend.IsValid() ? Backend->GetCompletedFrames() : 0;
	if (NowSeconds - FPSWindowStart >= 0.5)
	{
		CurrentFPS = static_cast<float>((Completed - FPSWindowFrames) / (NowSeconds - FPSWindowStart));
		FPSWindowFrames = Completed;
		FPSWindowStart = NowSeconds;
	}
}

void FCySenderStream::SetError(const FString& Error)
{
	FScopeLock Lock(&ErrorLock);
	LastError = Error;
	bErrorPending = true;
	CYGC_LOG(Warning, TEXT("Sender '%s': %s"), *Config.SenderName, *Error);
}

FString FCySenderStream::GetFormatName() const
{
	return CyDxgiFormatName(StatsDxgiFormat.load());
}

FCyGameCaptureStreamStats FCySenderStream::GetStats() const
{
	FCyGameCaptureStreamStats Stats;
	Stats.Name = Config.SenderName;
	Stats.State = State;
	Stats.bConnected = Registration.IsValid() && Registration->IsCreated();
	Stats.Width = StatsWidth.load();
	Stats.Height = StatsHeight.load();
	Stats.Format = GetFormatName();
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
