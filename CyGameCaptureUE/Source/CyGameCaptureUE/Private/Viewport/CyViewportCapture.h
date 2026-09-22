// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — Game viewport capture through the Slate back buffer hook.
//
// FSlateRenderer::OnBackBufferReadyToPresent fires on the render thread for every window right
// before Present, with the final back buffer: game scene + HUD + UMG + Slate, i.e. exactly what
// the player sees. No second scene render, one GPU copy (D3D11) or two (D3D12 bridge).
// Delegate signature per engine version:
//   4.27 .. 5.3 : (SWindow&, const FTexture2DRHIRef&)
//   5.4 .. 5.7  : (SWindow&, const FTextureRHIRef&)      (5.4 still FTexture2DRHIRef alias)
//   5.8+        : (SWindow&, ISlateViewportProvider&)     -> GetBackBufferResource()
#pragma once

#include "CoreMinimal.h"
#include "RHI/CyRHICompat.h"
#include "CyGameCaptureStream.h"
#include <atomic>

class SWindow;
class ISlateViewportProvider;

class FCyViewportCapture
{
public:
	FCyViewportCapture();
	~FCyViewportCapture();

	using FSenderStreamPtr = TSharedPtr<FCySenderStream, ESPMode::ThreadSafe>;

	// Game thread
	void AddSender(const FSenderStreamPtr& Stream);
	void RemoveSender(const FSenderStreamPtr& Stream);
	bool HasSenders() const { return NumSenders.load() > 0; }
	/** Refreshes the game window pointer (PIE windows change). */
	void Tick();

private:
	void Bind();
	void Unbind();
	void HandleBackBuffer(SWindow& Window, FRHITexture* BackBuffer);

#if CY_UE_AT_LEAST(5, 8)
	void OnBackBufferReady(SWindow& Window, ISlateViewportProvider& Provider);
#else
	void OnBackBufferReady(SWindow& Window, const FCyTextureRef& BackBuffer);
#endif

	mutable FCriticalSection Lock;
	TArray<FSenderStreamPtr> Senders;
	std::atomic<int32> NumSenders { 0 };
	std::atomic<SWindow*> GameWindow { nullptr };
	FDelegateHandle DelegateHandle;
	bool bBound = false;
};
