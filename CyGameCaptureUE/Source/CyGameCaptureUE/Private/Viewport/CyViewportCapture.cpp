// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

#include "Viewport/CyViewportCapture.h"
#include "CyGameCaptureLog.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/SWindow.h"
#include "RenderingThread.h"
#include "Misc/ScopeLock.h"

#if CY_UE_AT_LEAST(5, 8)
#include "Slate/SlateViewportProvider.h"
#endif

FCyViewportCapture::FCyViewportCapture()
{
}

FCyViewportCapture::~FCyViewportCapture()
{
	Unbind();
}

void FCyViewportCapture::AddSender(const FSenderStreamPtr& Stream)
{
	check(IsInGameThread());
	if (!Stream.IsValid())
	{
		return;
	}
	{
		FScopeLock ScopeLock(&Lock);
		Senders.AddUnique(Stream);
		NumSenders = Senders.Num();
	}
	Bind();
}

void FCyViewportCapture::RemoveSender(const FSenderStreamPtr& Stream)
{
	check(IsInGameThread());
	bool bEmpty;
	{
		FScopeLock ScopeLock(&Lock);
		Senders.Remove(Stream);
		NumSenders = Senders.Num();
		bEmpty = Senders.Num() == 0;
	}
	if (bEmpty)
	{
		Unbind();
	}
}

void FCyViewportCapture::Tick()
{
	SWindow* Window = nullptr;
	if (GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		const TSharedPtr<SWindow> GameWindowPtr = GEngine->GameViewport->GetWindow();
		Window = GameWindowPtr.Get();
	}
	GameWindow = Window;
}

void FCyViewportCapture::Bind()
{
	if (bBound || !FSlateApplication::IsInitialized())
	{
		return;
	}
	FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
	if (Renderer == nullptr)
	{
		return;
	}
	DelegateHandle = Renderer->OnBackBufferReadyToPresent().AddRaw(this, &FCyViewportCapture::OnBackBufferReady);
	bBound = true;
	CYGC_LOG(Log, TEXT("Game viewport capture: back buffer hook bound"));
}

void FCyViewportCapture::Unbind()
{
	if (!bBound)
	{
		return;
	}
	if (FSlateApplication::IsInitialized())
	{
		if (FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer())
		{
			Renderer->OnBackBufferReadyToPresent().Remove(DelegateHandle);
		}
	}
	bBound = false;
	// The delegate may be executing on the render thread right now: wait for it before going away
	FlushRenderingCommands();
	CYGC_LOG(Log, TEXT("Game viewport capture: back buffer hook unbound"));
}

#if CY_UE_AT_LEAST(5, 8)
void FCyViewportCapture::OnBackBufferReady(SWindow& Window, ISlateViewportProvider& Provider)
{
	HandleBackBuffer(Window, Provider.GetBackBufferResource());
}
#else
void FCyViewportCapture::OnBackBufferReady(SWindow& Window, const FCyTextureRef& BackBuffer)
{
	HandleBackBuffer(Window, BackBuffer.GetReference());
}
#endif

void FCyViewportCapture::HandleBackBuffer(SWindow& Window, FRHITexture* BackBuffer)
{
	if (BackBuffer == nullptr || &Window != GameWindow.load())
	{
		return;
	}
	TArray<FSenderStreamPtr, TInlineAllocator<8>> Local;
	{
		FScopeLock ScopeLock(&Lock);
		Local.Append(Senders);
	}
	if (Local.Num() == 0)
	{
		return;
	}
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	const FIntPoint Size = CyGetTextureSize(BackBuffer);
	const uint8 PixelFormat = static_cast<uint8>(CyGetTextureFormat(BackBuffer));
	for (const FSenderStreamPtr& Sender : Local)
	{
		Sender->SendTexture_RenderThread(RHICmdList, BackBuffer, Size, PixelFormat);
	}
}
