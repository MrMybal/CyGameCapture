// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — D3D12 backend through a D3D11On12 bridge (the approach used by Spout's own spoutDX12).
//
// Why a bridge: Spout receivers (OBS Spout2 plugin, Resolume, ...) open legacy DXGI shared handles with
// ID3D11Device::OpenSharedResource. D3D12 can only create NT-handle shared heaps, which those receivers
// cannot open. A D3D11On12 device created on the engine's D3D12 device + graphics queue can create a
// classic D3D11 shared texture and copy to/from D3D12 resources wrapped as D3D11 resources.
//
// Sender (per frame):
//   RHI  : Transition(intermediate[i] -> CopyDest), CopyTexture(source -> intermediate[i]), Transition(-> CopySrc),
//          signal fence (manual fence on 5.1+, SubmitCommandsHint + queue signal on 4.27 / 5.0)
//   Worker thread: wait fence -> Spout mutex -> 11on12 Acquire -> CopyResource(shared <- wrapped[i]) -> Release -> Flush
//   Waiting for the fence on a worker thread guarantees the engine copy finished before the bridge reads the
//   intermediate, without stalling the render thread. Cost: 2 GPU copies per frame (engine copy + bridge copy).
// Receiver (per frame, RHI thread):
//   11on12 Acquire -> CopyResource(wrapped intermediate <- shared) -> Release -> Flush (executes on the queue now),
//   then RHI Transition(intermediate -> CopySrc), CopyTexture(intermediate -> target), Transition(-> CopyDest).
//   The engine submits its command list after the bridge flushed, so ordering on the single queue is preserved.
#include "Spout/CySpoutBackend.h"
#include "Spout/CySpoutRegistry.h"
#include "CyGameCaptureLog.h"
#include "HAL/PlatformProcess.h"
#include "RenderingThread.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/Event.h"
#include "Misc/ScopeLock.h"
#include "Containers/Queue.h"
#include <atomic>

#if CYGC_WITH_SPOUT

#if CYGC_HAS_ID3D12DYNAMICRHI
#include "ID3D12DynamicRHI.h"
#endif

namespace
{
	// ------------------------------------------------------------------------------------------------
	// Process wide D3D11On12 bridge
	// ------------------------------------------------------------------------------------------------
	class FCyD3D11On12Bridge
	{
	public:
		static FCyD3D11On12Bridge& Get()
		{
			static FCyD3D11On12Bridge Instance;
			return Instance;
		}

		bool Init(FString& OutError)
		{
			FScopeLock Lock(&Mutex);
			if (Device11)
			{
				return true;
			}
			Device12 = static_cast<ID3D12Device*>(CyGetNativeDevice());
			Queue = static_cast<ID3D12CommandQueue*>(CyGetNativeGraphicsQueue());
			if (!Device12 || !Queue)
			{
				OutError = TEXT("No native D3D12 device / graphics queue");
				return false;
			}
			IUnknown* Queues[] = { Queue.Get() };
			const HRESULT Hr = D3D11On12CreateDevice(Device12.Get(), D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, Queues, 1, 0, Device11.GetAddressOf(), Context11.GetAddressOf(), nullptr);
			if (FAILED(Hr) || !Device11)
			{
				OutError = FString::Printf(TEXT("D3D11On12CreateDevice failed: 0x%08X"), static_cast<uint32>(Hr));
				Device11.Reset();
				Context11.Reset();
				return false;
			}
			if (FAILED(Device11.As(&On12)))
			{
				OutError = TEXT("ID3D11On12Device interface not available");
				Device11.Reset();
				Context11.Reset();
				return false;
			}
			CYGC_LOG(Log, TEXT("D3D11On12 bridge created on the engine D3D12 device"));
			return true;
		}

		void Shutdown()
		{
			FScopeLock Lock(&Mutex);
			if (Context11)
			{
				Context11->Flush();
			}
			On12.Reset();
			Context11.Reset();
			Device11.Reset();
			Queue.Reset();
			Device12.Reset();
		}

		/** The D3D11 immediate context of the bridge is single threaded: every use goes through this lock. */
		FCriticalSection Mutex;
		TCyComPtr<ID3D12Device> Device12;
		TCyComPtr<ID3D12CommandQueue> Queue;
		TCyComPtr<ID3D11Device> Device11;
		TCyComPtr<ID3D11DeviceContext> Context11;
		TCyComPtr<ID3D11On12Device> On12;
	};

	bool CreateSharedTexture11(ID3D11Device* Device, uint32 Width, uint32 Height, uint32 DxgiFormat, TCyComPtr<ID3D11Texture2D>& OutTexture, HANDLE& OutHandle, FString& OutError)
	{
		D3D11_TEXTURE2D_DESC Desc = {};
		Desc.Width = Width;
		Desc.Height = Height;
		Desc.MipLevels = 1;
		Desc.ArraySize = 1;
		Desc.Format = static_cast<DXGI_FORMAT>(DxgiFormat);
		Desc.SampleDesc.Count = 1;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		Desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

		TCyComPtr<ID3D11Texture2D> Texture;
		HRESULT Hr = Device->CreateTexture2D(&Desc, nullptr, Texture.GetAddressOf());
		if (FAILED(Hr))
		{
			OutError = FString::Printf(TEXT("11on12 CreateTexture2D(shared %ux%u %s) failed: 0x%08X"), Width, Height, *CyDxgiFormatName(DxgiFormat), static_cast<uint32>(Hr));
			return false;
		}
		TCyComPtr<IDXGIResource> DxgiResource;
		HANDLE Handle = nullptr;
		if (FAILED(Texture.As(&DxgiResource)) || FAILED(DxgiResource->GetSharedHandle(&Handle)) || Handle == nullptr)
		{
			OutError = TEXT("GetSharedHandle failed (11on12)");
			return false;
		}
		OutTexture = Texture;
		OutHandle = Handle;
		return true;
	}

	struct FCyWrappedSlot
	{
		FTextureRHIRef Intermediate;
		TCyComPtr<ID3D11Resource> Wrapped;
		std::atomic<bool> bBusy { false };
	};

	bool WrapResource(FCyD3D11On12Bridge& Bridge, FRHITexture* Texture, D3D12_RESOURCE_STATES State, TCyComPtr<ID3D11Resource>& OutWrapped, FString& OutError)
	{
		ID3D12Resource* Native = static_cast<ID3D12Resource*>(CyGetNativeResource(Texture));
		if (Native == nullptr)
		{
			OutError = TEXT("Intermediate texture has no native D3D12 resource");
			return false;
		}
		D3D11_RESOURCE_FLAGS Flags = {};
		const HRESULT Hr = Bridge.On12->CreateWrappedResource(Native, &Flags, State, State, IID_PPV_ARGS(OutWrapped.GetAddressOf()));
		if (FAILED(Hr))
		{
			OutError = FString::Printf(TEXT("CreateWrappedResource failed: 0x%08X"), static_cast<uint32>(Hr));
			return false;
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------------
	// Sender
	// ------------------------------------------------------------------------------------------------
	class FCyD3D12SenderBackend final : public ICySenderBackend, public FRunnable
	{
	public:
		static constexpr int32 NumSlots = 3;

		bool Init(FString& OutError)
		{
			FCyD3D11On12Bridge& Bridge = FCyD3D11On12Bridge::Get();
			if (!Bridge.Init(OutError))
			{
				return false;
			}
			if (FAILED(Bridge.Device12->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(Fence.GetAddressOf()))))
			{
				OutError = TEXT("CreateFence failed");
				return false;
			}
			FenceEvent = CreateEventW(nullptr, 0, 0, nullptr);
			WorkEvent = FPlatformProcess::GetSynchEventFromPool(false);
			bStopThread = false;
			Thread.Reset(FRunnableThread::Create(this, TEXT("CySpoutD3D12Sender"), 0, TPri_AboveNormal));
			return Thread.IsValid();
		}

		virtual bool EnsureSharedTexture(uint32 Width, uint32 Height, uint32 DxgiFormat, FString& OutError) override
		{
			if (Shared && SharedWidth == Width && SharedHeight == Height && SharedFormat == DxgiFormat)
			{
				return true;
			}
			FCyD3D11On12Bridge& Bridge = FCyD3D11On12Bridge::Get();

			// Wait for the worker to finish with the current slots before replacing everything
			WaitWorkerIdle();

			TCyComPtr<ID3D11Texture2D> NewShared;
			HANDLE NewHandle = nullptr;
			{
				FScopeLock Lock(&Bridge.Mutex);
				if (!CreateSharedTexture11(Bridge.Device11.Get(), Width, Height, DxgiFormat, NewShared, NewHandle, OutError))
				{
					return false;
				}
			}

			const EPixelFormat PixelFormat = CyDxgiToPixelFormat(DxgiFormat);
			for (int32 i = 0; i < NumSlots; ++i)
			{
				FCyWrappedSlot& Slot = Slots[i];
				Slot.Wrapped.Reset();
				Slot.Intermediate = CyCreateTexture2D(TEXT("CySpoutSenderIntermediate"), Width, Height, PixelFormat, TexCreate_ShaderResource, ERHIAccess::CopySrc);
				if (!Slot.Intermediate.IsValid())
				{
					OutError = TEXT("Failed to create the D3D12 intermediate texture");
					return false;
				}
				FScopeLock Lock(&Bridge.Mutex);
				if (!WrapResource(Bridge, Slot.Intermediate.GetReference(), D3D12_RESOURCE_STATE_COPY_SOURCE, Slot.Wrapped, OutError))
				{
					return false;
				}
			}

			{
				FScopeLock Lock(&Bridge.Mutex);
				Shared = NewShared;
			}
			ShareHandle = NewHandle;
			SharedWidth = Width;
			SharedHeight = Height;
			SharedFormat = DxgiFormat;
			CYGC_DEBUG(TEXT("D3D12 shared texture %ux%u %s handle=0x%p (11on12)"), Width, Height, *CyDxgiFormatName(DxgiFormat), ShareHandle);
			return true;
		}

		virtual void* GetShareHandle() const override { return ShareHandle; }

		virtual bool Copy(FRHICommandListImmediate& RHICmdList, const FCyTextureRef& Source, FIntPoint SourceSize, FCySpoutSenderRegistration* Registration, bool /*bFlush*/) override
		{
			if (!Shared || !Source.IsValid() || Registration == nullptr || static_cast<uint32>(SourceSize.X) != SharedWidth || static_cast<uint32>(SourceSize.Y) != SharedHeight)
			{
				Dropped++;
				return false;
			}
			const int32 SlotIndex = NextSlot;
			FCyWrappedSlot& Slot = Slots[SlotIndex];
			if (Slot.bBusy.load())
			{
				Dropped++;   // worker still copying this slot: skip the frame instead of stalling
				return false;
			}
			NextSlot = (NextSlot + 1) % NumSlots;
			Slot.bBusy = true;

			const uint64 Start = FPlatformTime::Cycles64();
			RHICmdList.Transition(FRHITransitionInfo(Slot.Intermediate, ERHIAccess::CopySrc, ERHIAccess::CopyDest));
			RHICmdList.CopyTexture(Source, Slot.Intermediate, FRHICopyTextureInfo());
			RHICmdList.Transition(FRHITransitionInfo(Slot.Intermediate, ERHIAccess::CopyDest, ERHIAccess::CopySrc));

			const uint64 Value = ++FenceValue;
#if CYGC_HAS_ID3D12DYNAMICRHI
			// Signalled by the D3D12 RHI submission after the command list containing the copy. The D3D12 context
			// is only bound while RHI commands execute, so the call must happen inside an RHI lambda.
			{
				ID3D12Fence* FencePtr = Fence.Get();
				RHICmdList.EnqueueLambda([FencePtr, Value](FRHICommandListBase& ExecutingCmdList)
				{
					GetID3D12DynamicRHI()->RHISignalManualFence(static_cast<FRHICommandList&>(ExecutingCmdList), FencePtr, Value);
				});
			}
#else
			// 4.27 / 5.0: command lists are submitted synchronously on the RHI thread by SubmitCommandsHint
			RHICmdList.SubmitCommandsHint();
			{
				ID3D12CommandQueue* QueuePtr = FCyD3D11On12Bridge::Get().Queue.Get();
				ID3D12Fence* FencePtr = Fence.Get();
				RHICmdList.EnqueueLambda([QueuePtr, FencePtr, Value](FRHICommandListBase&) { QueuePtr->Signal(FencePtr, Value); });
			}
#endif
			Jobs.Enqueue(FJob { SlotIndex, Value, Registration });
			PendingJobs++;
			WorkEvent->Trigger();
			LastCopyMs = CyCyclesToMs(Start, FPlatformTime::Cycles64());
			return true;
		}

		virtual int64 GetCompletedFrames() const override { return Completed.load(); }
		virtual int64 GetDroppedFrames() const override { return Dropped.load(); }
		virtual float GetLastCopyMs() const override { return LastCopyMs.load(); }

		virtual void Release() override
		{
			StopThread();
			FCyD3D11On12Bridge& Bridge = FCyD3D11On12Bridge::Get();
			FScopeLock Lock(&Bridge.Mutex);
			for (FCyWrappedSlot& Slot : Slots)
			{
				Slot.Wrapped.Reset();
				Slot.Intermediate.SafeRelease();
			}
			Shared.Reset();
			ShareHandle = nullptr;
			if (FenceEvent)
			{
				CloseHandle(FenceEvent);
				FenceEvent = nullptr;
			}
			Fence.Reset();
		}

		// ---- FRunnable (worker thread) ----------------------------------------------------------
		virtual uint32 Run() override
		{
			while (!bStopThread)
			{
				FJob Job;
				if (!Jobs.Dequeue(Job))
				{
					WorkEvent->Wait(5);
					continue;
				}
				ProcessJob(Job);
				PendingJobs--;
			}
			return 0;
		}

	private:
		struct FJob
		{
			int32 Slot = 0;
			uint64 FenceValue = 0;
			FCySpoutSenderRegistration* Registration = nullptr;
		};

		void ProcessJob(const FJob& Job)
		{
			FCyWrappedSlot& Slot = Slots[Job.Slot];
			// Wait for the engine copy to finish on the GPU (worker thread: the render thread is never blocked)
			if (Fence->GetCompletedValue() < Job.FenceValue)
			{
				Fence->SetEventOnCompletion(Job.FenceValue, FenceEvent);
				while (!bStopThread && WaitForSingleObject(FenceEvent, 50) == WAIT_TIMEOUT)
				{
					if (Fence->GetCompletedValue() >= Job.FenceValue)
					{
						break;
					}
				}
			}
			if (bStopThread)
			{
				Slot.bBusy = false;
				return;
			}

			FCyD3D11On12Bridge& Bridge = FCyD3D11On12Bridge::Get();
			const uint64 Start = FPlatformTime::Cycles64();
			bool bSent = false;
			if (Job.Registration->BeginAccess())
			{
				FScopeLock Lock(&Bridge.Mutex);
				if (Shared && Slot.Wrapped)
				{
					ID3D11Resource* Wrapped = Slot.Wrapped.Get();
					Bridge.On12->AcquireWrappedResources(&Wrapped, 1);
					Bridge.Context11->CopyResource(Shared.Get(), Wrapped);
					Bridge.On12->ReleaseWrappedResources(&Wrapped, 1);
					Bridge.Context11->Flush();
					bSent = true;
				}
				Job.Registration->EndAccess(bSent);
			}
			if (bSent)
			{
				Completed++;
			}
			else
			{
				Dropped++;
			}
			LastBridgeMs = CyCyclesToMs(Start, FPlatformTime::Cycles64());
			Slot.bBusy = false;
		}

		void WaitWorkerIdle()
		{
			const double Deadline = FPlatformTime::Seconds() + 1.0;
			while (PendingJobs.load() > 0 && FPlatformTime::Seconds() < Deadline)
			{
				FPlatformProcess::Sleep(0.001f);
			}
		}

		void StopThread()
		{
			bStopThread = true;
			if (WorkEvent)
			{
				WorkEvent->Trigger();
			}
			if (Thread.IsValid())
			{
				Thread->WaitForCompletion();
				Thread.Reset();
			}
			if (WorkEvent)
			{
				FPlatformProcess::ReturnSynchEventToPool(WorkEvent);
				WorkEvent = nullptr;
			}
			FJob Dummy;
			while (Jobs.Dequeue(Dummy)) {}
			PendingJobs = 0;
		}

		TCyComPtr<ID3D11Texture2D> Shared;
		HANDLE ShareHandle = nullptr;
		uint32 SharedWidth = 0;
		uint32 SharedHeight = 0;
		uint32 SharedFormat = 0;
		FCyWrappedSlot Slots[NumSlots];
		int32 NextSlot = 0;

		TCyComPtr<ID3D12Fence> Fence;
		uint64 FenceValue = 0;
		HANDLE FenceEvent = nullptr;

		TUniquePtr<FRunnableThread> Thread;
		FEvent* WorkEvent = nullptr;
		std::atomic<bool> bStopThread { false };
		TQueue<FJob, EQueueMode::Spsc> Jobs;
		std::atomic<int32> PendingJobs { 0 };

		std::atomic<int64> Completed { 0 };
		std::atomic<int64> Dropped { 0 };
		std::atomic<float> LastCopyMs { 0.0f };
		std::atomic<float> LastBridgeMs { 0.0f };
	};

	// ------------------------------------------------------------------------------------------------
	// Receiver
	// ------------------------------------------------------------------------------------------------
	class FCyD3D12ReceiverBackend final : public ICyReceiverBackend
	{
	public:
		bool Init(FString& OutError)
		{
			return FCyD3D11On12Bridge::Get().Init(OutError);
		}

		virtual bool Open(void* InShareHandle, uint32 Width, uint32 Height, uint32 DxgiFormat, FString& OutError) override
		{
			FCyD3D11On12Bridge& Bridge = FCyD3D11On12Bridge::Get();
			FScopeLock Lock(&Bridge.Mutex);
			Shared.Reset();
			Wrapped.Reset();
			Intermediate.SafeRelease();

			TCyComPtr<ID3D11Texture2D> Texture;
			const HRESULT Hr = Bridge.Device11->OpenSharedResource(static_cast<HANDLE>(InShareHandle), IID_PPV_ARGS(Texture.GetAddressOf()));
			if (FAILED(Hr) || !Texture)
			{
				OutError = FString::Printf(TEXT("11on12 OpenSharedResource(0x%p) failed: 0x%08X"), InShareHandle, static_cast<uint32>(Hr));
				return false;
			}
			D3D11_TEXTURE2D_DESC Desc = {};
			Texture->GetDesc(&Desc);
			SharedWidth = Desc.Width;
			SharedHeight = Desc.Height;
			SharedFormat = static_cast<uint32>(Desc.Format);
			(void)Width; (void)Height; (void)DxgiFormat;

			const EPixelFormat PixelFormat = CyDxgiToPixelFormat(SharedFormat);
			if (PixelFormat == PF_Unknown)
			{
				OutError = FString::Printf(TEXT("Unsupported sender format %s"), *CyDxgiFormatName(SharedFormat));
				return false;
			}
			Intermediate = CyCreateTexture2D(TEXT("CySpoutReceiverIntermediate"), SharedWidth, SharedHeight, PixelFormat, TexCreate_ShaderResource, ERHIAccess::CopyDest);
			if (!Intermediate.IsValid() || !WrapResource(Bridge, Intermediate.GetReference(), D3D12_RESOURCE_STATE_COPY_DEST, Wrapped, OutError))
			{
				return false;
			}
			Shared = Texture;
			CYGC_DEBUG(TEXT("D3D12 opened shared texture 0x%p %ux%u %s (11on12)"), InShareHandle, SharedWidth, SharedHeight, *CyDxgiFormatName(SharedFormat));
			return true;
		}

		virtual bool Copy(FRHICommandListImmediate& RHICmdList, const FCyTextureRef& Target, FIntPoint TargetSize, uint32 TargetDxgiFormat, FCySpoutSenderRegistration* Access) override
		{
			if (!Shared || !Wrapped || !Intermediate.IsValid() || !Target.IsValid() || Access == nullptr)
			{
				Dropped++;
				return false;
			}
			if (static_cast<uint32>(TargetSize.X) != SharedWidth || static_cast<uint32>(TargetSize.Y) != SharedHeight || !CyDxgiCopyCompatible(TargetDxgiFormat, SharedFormat))
			{
				Dropped++;
				return false;
			}

			// 1. bridge copy: shared -> wrapped intermediate (executes on the queue right away, RHI thread)
			ID3D11Texture2D* SharedPtr = Shared.Get();
			ID3D11Resource* WrappedPtr = Wrapped.Get();
			SharedPtr->AddRef();
			WrappedPtr->AddRef();
			RHICmdList.EnqueueLambda([this, SharedPtr, WrappedPtr, Access](FRHICommandListBase&)
			{
				const uint64 Start = FPlatformTime::Cycles64();
				FCyD3D11On12Bridge& Bridge = FCyD3D11On12Bridge::Get();
				if (Access->BeginAccess())
				{
					FScopeLock Lock(&Bridge.Mutex);
					Bridge.On12->AcquireWrappedResources(&WrappedPtr, 1);
					Bridge.Context11->CopyResource(WrappedPtr, SharedPtr);
					Bridge.On12->ReleaseWrappedResources(&WrappedPtr, 1);
					Bridge.Context11->Flush();
					Access->EndAccess(false);
					Completed++;
				}
				else
				{
					Dropped++;
				}
				LastCopyMs = CyCyclesToMs(Start, FPlatformTime::Cycles64());
				WrappedPtr->Release();
				SharedPtr->Release();
			});

			// 2. engine copy: intermediate -> target (submitted after the bridge flush)
			RHICmdList.Transition(FRHITransitionInfo(Intermediate, ERHIAccess::CopyDest, ERHIAccess::CopySrc));
			RHICmdList.CopyTexture(Intermediate, Target, FRHICopyTextureInfo());
			RHICmdList.Transition(FRHITransitionInfo(Intermediate, ERHIAccess::CopySrc, ERHIAccess::CopyDest));
			return true;
		}

		virtual int64 GetCompletedFrames() const override { return Completed.load(); }
		virtual int64 GetDroppedFrames() const override { return Dropped.load(); }
		virtual float GetLastCopyMs() const override { return LastCopyMs.load(); }

		virtual void Release() override
		{
			FCyD3D11On12Bridge& Bridge = FCyD3D11On12Bridge::Get();
			FScopeLock Lock(&Bridge.Mutex);
			Wrapped.Reset();
			Shared.Reset();
			Intermediate.SafeRelease();
		}

	private:
		TCyComPtr<ID3D11Texture2D> Shared;
		TCyComPtr<ID3D11Resource> Wrapped;
		FTextureRHIRef Intermediate;
		uint32 SharedWidth = 0;
		uint32 SharedHeight = 0;
		uint32 SharedFormat = 0;
		std::atomic<int64> Completed { 0 };
		std::atomic<int64> Dropped { 0 };
		std::atomic<float> LastCopyMs { 0.0f };
	};
}

TUniquePtr<ICySenderBackend> CyCreateD3D12SenderBackend(FString& OutError)
{
	TUniquePtr<FCyD3D12SenderBackend> Backend = MakeUnique<FCyD3D12SenderBackend>();
	if (!Backend->Init(OutError))
	{
		Backend->Release();
		return nullptr;
	}
	return Backend;
}

TUniquePtr<ICyReceiverBackend> CyCreateD3D12ReceiverBackend(FString& OutError)
{
	TUniquePtr<FCyD3D12ReceiverBackend> Backend = MakeUnique<FCyD3D12ReceiverBackend>();
	if (!Backend->Init(OutError))
	{
		return nullptr;
	}
	return Backend;
}

void CyShutdownD3D11On12Bridge()
{
	FCyD3D11On12Bridge::Get().Shutdown();
}

#endif // CYGC_WITH_SPOUT
