// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — GPU backends (D3D11 native, D3D12 through a D3D11On12 bridge).
//
// A backend owns the Spout shared texture (sender) or the opened shared texture (receiver) and
// performs GPU-to-GPU copies. Backends live on the render thread; they are destroyed through an
// RHI-thread lambda queued after the last copy so that no queued GPU work can reference a dead
// backend (see FCySenderStream::Stop).
#pragma once

#include "CoreMinimal.h"
#include "RHI/CyRHICompat.h"

class FCySpoutSenderRegistration;

class ICySenderBackend
{
public:
	virtual ~ICySenderBackend() = default;

	/** Render thread. (Re)creates the shared texture when size / format changed. */
	virtual bool EnsureSharedTexture(uint32 Width, uint32 Height, uint32 DxgiFormat, FString& OutError) = 0;
	virtual void* GetShareHandle() const = 0;

	/**
	 * Render thread, inside a render command. Source is expected in the CopySrc RHI state and to match the
	 * shared texture size / format family. The backend takes the Spout access mutex around the copy.
	 * Returns false when the frame was dropped synchronously (error / busy).
	 */
	virtual bool Copy(FRHICommandListImmediate& RHICmdList, const FCyTextureRef& Source, FIntPoint SourceSize, FCySpoutSenderRegistration* Registration, bool bFlush) = 0;

	virtual int64 GetCompletedFrames() const = 0;     // frames that reached the shared texture (asynchronous on D3D12)
	virtual int64 GetDroppedFrames() const = 0;
	virtual float GetLastCopyMs() const = 0;          // CPU cost of the last copy issue (RHI thread / worker)

	/** RHI thread (queued after the last copy). */
	virtual void Release() = 0;
};

class ICyReceiverBackend
{
public:
	virtual ~ICyReceiverBackend() = default;

	/** Render thread. Opens the sender's shared texture. */
	virtual bool Open(void* ShareHandle, uint32 Width, uint32 Height, uint32 DxgiFormat, FString& OutError) = 0;

	/**
	 * Render thread, inside a render command. Target is expected in the CopyDest RHI state, same size as the
	 * sender and a copy compatible format. Returns false when the frame was dropped synchronously.
	 */
	virtual bool Copy(FRHICommandListImmediate& RHICmdList, const FCyTextureRef& Target, FIntPoint TargetSize, uint32 TargetDxgiFormat, FCySpoutSenderRegistration* Access) = 0;

	virtual int64 GetCompletedFrames() const = 0;
	virtual int64 GetDroppedFrames() const = 0;
	virtual float GetLastCopyMs() const = 0;

	virtual void Release() = 0;
};

namespace CySpoutBackend
{
	/** Availability of a backend for the running RHI (evaluated once GDynamicRHI exists). */
	bool IsSupported(FString& OutReason, FString& OutRHIName);

	TUniquePtr<ICySenderBackend> CreateSender(FString& OutError);
	TUniquePtr<ICyReceiverBackend> CreateReceiver(FString& OutError);

	/** Releases process-wide helpers (D3D11On12 bridge). Called at engine pre-exit after all streams stopped. */
	void ShutdownShared();
}
