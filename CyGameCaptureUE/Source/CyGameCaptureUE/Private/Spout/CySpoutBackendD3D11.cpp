// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — D3D11 backend.
//
// Sender  : shared texture created natively on the engine's ID3D11Device with D3D11_RESOURCE_MISC_SHARED
//           (legacy DXGI handle, what every Spout receiver opens), CopyResource(shared <- source) on the
//           engine's immediate context from the RHI thread. One GPU copy per frame, no intermediate.
// Receiver: OpenSharedResource on the engine device, CopyResource(target <- shared) on the RHI thread.
#include "Spout/CySpoutBackend.h"
#include "Spout/CySpoutRegistry.h"
#include "CyGameCaptureLog.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "Misc/ScopeLock.h"
#include <atomic>

#if CYGC_WITH_SPOUT

namespace
{
	bool GetD3D11DeviceAndContext(TCyComPtr<ID3D11Device>& OutDevice, TCyComPtr<ID3D11DeviceContext>& OutContext, FString& OutError)
	{
		ID3D11Device* Device = static_cast<ID3D11Device*>(CyGetNativeDevice());
		if (Device == nullptr)
		{
			OutError = TEXT("No native D3D11 device");
			return false;
		}
		OutDevice = Device;
		OutDevice->GetImmediateContext(OutContext.GetAddressOf());
		if (!OutContext)
		{
			OutError = TEXT("No D3D11 immediate context");
			return false;
		}
		return true;
	}

	bool CreateSharedTexture(ID3D11Device* Device, uint32 Width, uint32 Height, uint32 DxgiFormat, TCyComPtr<ID3D11Texture2D>& OutTexture, HANDLE& OutHandle, FString& OutError)
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
		Desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;   // legacy handle: OpenSharedResource compatible

		TCyComPtr<ID3D11Texture2D> Texture;
		HRESULT Hr = Device->CreateTexture2D(&Desc, nullptr, Texture.GetAddressOf());
		if (FAILED(Hr))
		{
			OutError = FString::Printf(TEXT("CreateTexture2D(shared %ux%u %s) failed: 0x%08X"), Width, Height, *CyDxgiFormatName(DxgiFormat), static_cast<uint32>(Hr));
			return false;
		}
		TCyComPtr<IDXGIResource> DxgiResource;
		Hr = Texture.As(&DxgiResource);
		HANDLE Handle = nullptr;
		if (FAILED(Hr) || FAILED(DxgiResource->GetSharedHandle(&Handle)) || Handle == nullptr)
		{
			OutError = TEXT("GetSharedHandle failed");
			return false;
		}
		OutTexture = Texture;
		OutHandle = Handle;
		return true;
	}

	// ------------------------------------------------------------------------------------------------
	class FCyD3D11SenderBackend final : public ICySenderBackend
	{
	public:
		bool Init(FString& OutError)
		{
			return GetD3D11DeviceAndContext(Device, Context, OutError);
		}

		virtual bool EnsureSharedTexture(uint32 Width, uint32 Height, uint32 DxgiFormat, FString& OutError) override
		{
			if (Shared && SharedWidth == Width && SharedHeight == Height && SharedFormat == DxgiFormat)
			{
				return true;
			}
			TCyComPtr<ID3D11Texture2D> NewTexture;
			HANDLE NewHandle = nullptr;
			if (!CreateSharedTexture(Device.Get(), Width, Height, DxgiFormat, NewTexture, NewHandle, OutError))
			{
				return false;
			}
			// The previous texture may still be referenced by queued RHI lambdas: hand it over to the RHI thread for release
			if (Shared)
			{
				ID3D11Texture2D* Old = Shared.Detach();
				ENQUEUE_RENDER_COMMAND(CyReleaseOldShared)([Old](FRHICommandListImmediate& RHICmdList)
				{
					RHICmdList.EnqueueLambda([Old](FRHICommandListBase&) { Old->Release(); });
				});
			}
			Shared = NewTexture;
			ShareHandle = NewHandle;
			SharedWidth = Width;
			SharedHeight = Height;
			SharedFormat = DxgiFormat;
			CYGC_DEBUG(TEXT("D3D11 shared texture %ux%u %s handle=0x%p"), Width, Height, *CyDxgiFormatName(DxgiFormat), ShareHandle);
			return true;
		}

		virtual void* GetShareHandle() const override { return ShareHandle; }

		virtual bool Copy(FRHICommandListImmediate& RHICmdList, const FCyTextureRef& Source, FIntPoint SourceSize, FCySpoutSenderRegistration* Registration, bool bFlush) override
		{
			if (!Shared || !Source.IsValid() || Registration == nullptr)
			{
				Dropped++;
				return false;
			}
			if (static_cast<uint32>(SourceSize.X) != SharedWidth || static_cast<uint32>(SourceSize.Y) != SharedHeight)
			{
				Dropped++;
				return false;
			}

			ID3D11Texture2D* SharedPtr = Shared.Get();
			SharedPtr->AddRef();   // keep alive until the RHI lambda ran (EnsureSharedTexture may replace it meanwhile)
			ID3D11DeviceContext* Ctx = Context.Get();

			RHICmdList.EnqueueLambda([this, Source, SharedPtr, Ctx, Registration, bFlush](FRHICommandListBase&)
			{
				const uint64 Start = FPlatformTime::Cycles64();
				ID3D11Resource* Native = static_cast<ID3D11Resource*>(CyGetNativeResource(Source.GetReference()));
				if (Native != nullptr && Registration->BeginAccess())
				{
					Ctx->CopyResource(SharedPtr, Native);
					if (bFlush)
					{
						Ctx->Flush();
					}
					Registration->EndAccess(true);
					Completed++;
				}
				else
				{
					Dropped++;
				}
				LastCopyMs = CyCyclesToMs(Start, FPlatformTime::Cycles64());
				SharedPtr->Release();
			});
			return true;
		}

		virtual int64 GetCompletedFrames() const override { return Completed.load(); }
		virtual int64 GetDroppedFrames() const override { return Dropped.load(); }
		virtual float GetLastCopyMs() const override { return LastCopyMs.load(); }

		virtual void Release() override
		{
			Shared.Reset();
			ShareHandle = nullptr;
			Context.Reset();
			Device.Reset();
		}

	private:
		TCyComPtr<ID3D11Device> Device;
		TCyComPtr<ID3D11DeviceContext> Context;
		TCyComPtr<ID3D11Texture2D> Shared;
		HANDLE ShareHandle = nullptr;
		uint32 SharedWidth = 0;
		uint32 SharedHeight = 0;
		uint32 SharedFormat = 0;
		std::atomic<int64> Completed { 0 };
		std::atomic<int64> Dropped { 0 };
		std::atomic<float> LastCopyMs { 0.0f };
	};

	// ------------------------------------------------------------------------------------------------
	class FCyD3D11ReceiverBackend final : public ICyReceiverBackend
	{
	public:
		bool Init(FString& OutError)
		{
			return GetD3D11DeviceAndContext(Device, Context, OutError);
		}

		virtual bool Open(void* ShareHandle, uint32 Width, uint32 Height, uint32 DxgiFormat, FString& OutError) override
		{
			ReleaseSharedDeferred();
			TCyComPtr<ID3D11Texture2D> Texture;
			const HRESULT Hr = Device->OpenSharedResource(static_cast<HANDLE>(ShareHandle), IID_PPV_ARGS(Texture.GetAddressOf()));
			if (FAILED(Hr) || !Texture)
			{
				OutError = FString::Printf(TEXT("OpenSharedResource(0x%p) failed: 0x%08X (sender on another GPU adapter?)"), ShareHandle, static_cast<uint32>(Hr));
				return false;
			}
			D3D11_TEXTURE2D_DESC Desc = {};
			Texture->GetDesc(&Desc);
			Shared = Texture;
			SharedWidth = Desc.Width;
			SharedHeight = Desc.Height;
			SharedFormat = static_cast<uint32>(Desc.Format);
			if (SharedWidth != Width || SharedHeight != Height)
			{
				CYGC_LOG(Warning, TEXT("Spout sender info (%ux%u) differs from the shared texture (%ux%u); using the texture"), Width, Height, SharedWidth, SharedHeight);
			}
			CYGC_DEBUG(TEXT("D3D11 opened shared texture 0x%p %ux%u %s"), ShareHandle, SharedWidth, SharedHeight, *CyDxgiFormatName(SharedFormat));
			return true;
		}

		virtual bool Copy(FRHICommandListImmediate& RHICmdList, const FCyTextureRef& Target, FIntPoint TargetSize, uint32 TargetDxgiFormat, FCySpoutSenderRegistration* Access) override
		{
			if (!Shared || !Target.IsValid() || Access == nullptr)
			{
				Dropped++;
				return false;
			}
			if (static_cast<uint32>(TargetSize.X) != SharedWidth || static_cast<uint32>(TargetSize.Y) != SharedHeight || !CyDxgiCopyCompatible(TargetDxgiFormat, SharedFormat))
			{
				Dropped++;
				return false;
			}

			ID3D11Texture2D* SharedPtr = Shared.Get();
			SharedPtr->AddRef();
			ID3D11DeviceContext* Ctx = Context.Get();

			RHICmdList.EnqueueLambda([this, Target, SharedPtr, Ctx, Access](FRHICommandListBase&)
			{
				const uint64 Start = FPlatformTime::Cycles64();
				ID3D11Resource* Native = static_cast<ID3D11Resource*>(CyGetNativeResource(Target.GetReference()));
				if (Native != nullptr && Access->BeginAccess())
				{
					Ctx->CopyResource(Native, SharedPtr);
					Access->EndAccess(false);
					Completed++;
				}
				else
				{
					Dropped++;
				}
				LastCopyMs = CyCyclesToMs(Start, FPlatformTime::Cycles64());
				SharedPtr->Release();
			});
			return true;
		}

		virtual int64 GetCompletedFrames() const override { return Completed.load(); }
		virtual int64 GetDroppedFrames() const override { return Dropped.load(); }
		virtual float GetLastCopyMs() const override { return LastCopyMs.load(); }

		virtual void Release() override
		{
			Shared.Reset();
			Context.Reset();
			Device.Reset();
		}

	private:
		void ReleaseSharedDeferred()
		{
			if (Shared)
			{
				ID3D11Texture2D* Old = Shared.Detach();
				ENQUEUE_RENDER_COMMAND(CyReleaseOldOpened)([Old](FRHICommandListImmediate& RHICmdList)
				{
					RHICmdList.EnqueueLambda([Old](FRHICommandListBase&) { Old->Release(); });
				});
			}
		}

		TCyComPtr<ID3D11Device> Device;
		TCyComPtr<ID3D11DeviceContext> Context;
		TCyComPtr<ID3D11Texture2D> Shared;
		uint32 SharedWidth = 0;
		uint32 SharedHeight = 0;
		uint32 SharedFormat = 0;
		std::atomic<int64> Completed { 0 };
		std::atomic<int64> Dropped { 0 };
		std::atomic<float> LastCopyMs { 0.0f };
	};
}

TUniquePtr<ICySenderBackend> CyCreateD3D11SenderBackend(FString& OutError)
{
	TUniquePtr<FCyD3D11SenderBackend> Backend = MakeUnique<FCyD3D11SenderBackend>();
	if (!Backend->Init(OutError))
	{
		return nullptr;
	}
	return Backend;
}

TUniquePtr<ICyReceiverBackend> CyCreateD3D11ReceiverBackend(FString& OutError)
{
	TUniquePtr<FCyD3D11ReceiverBackend> Backend = MakeUnique<FCyD3D11ReceiverBackend>();
	if (!Backend->Init(OutError))
	{
		return nullptr;
	}
	return Backend;
}

#endif // CYGC_WITH_SPOUT
