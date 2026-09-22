// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — engine subsystem: registry of every sender/receiver stream, Spout sender
// discovery, name collision policy, per-frame ticking, world/engine shutdown cleanup.
//
// An engine subsystem (not a world subsystem) is used on purpose: Spout senders are process-wide
// resources, several PIE worlds may exist at once, and every stream must be released exactly once
// when the engine exits. Components remain the natural owners; the subsystem guarantees that
// streams whose owner (component / world) disappeared are stopped ("no ghost senders").
#pragma once

#include "CoreMinimal.h"
#include "Misc/EngineVersionComparison.h"
#include "Subsystems/EngineSubsystem.h"
#include "Containers/Ticker.h"
#include "CyGameCaptureTypes.h"
#include "CyGameCaptureStream.h"
#include "CyGameCaptureSubsystem.generated.h"

class FCyViewportCapture;

UCLASS()
class CYGAMECAPTUREUE_API UCyGameCaptureSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor (nullptr before engine init). */
	static UCyGameCaptureSubsystem* Get();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------ Blueprint API
	/** True when the plugin can create Spout streams on this machine (Windows, D3D11 or D3D12 RHI, Spout enabled in settings). */
	UFUNCTION(BlueprintPure, Category = "CyGameCapture")
	bool IsSpoutSupported() const;

	/** "D3D11", "D3D12", "Vulkan", ... */
	UFUNCTION(BlueprintPure, Category = "CyGameCapture")
	FString GetRHIName() const;

	/** Why IsSpoutSupported() is false (empty when supported). */
	UFUNCTION(BlueprintPure, Category = "CyGameCapture")
	FString GetUnsupportedReason() const { return UnsupportedReason; }

	/** Spout senders currently visible on this machine (all applications). Cached and refreshed at the configured interval. */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Spout")
	TArray<FCySpoutSenderInfo> GetAvailableSpoutSenders(bool bForceRefresh = false);

	/** Live lookup of one sender. */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Spout")
	bool FindSpoutSender(const FString& SenderName, FCySpoutSenderInfo& OutInfo) const;

	/** "<Prefix>::<ProjectName>::<StreamName>" (prefix from settings). */
	UFUNCTION(BlueprintPure, Category = "CyGameCapture")
	FString MakeDefaultSenderName(const FString& StreamName) const;

	/** Statistics of every running stream. */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Stats")
	void GetAllStreamStats(TArray<FCyGameCaptureStreamStats>& Senders, TArray<FCyGameCaptureStreamStats>& Receivers) const;

	UFUNCTION(BlueprintPure, Category = "CyGameCapture|Stats")
	int32 GetActiveStreamCount() const { return Senders.Num() + Receivers.Num(); }

	/** Stops and destroys every stream (senders and receivers). */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture")
	void StopAllStreams();

	/** On-screen debug overlay (also console: CyGameCapture.Debug 0/1). */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Debug")
	void SetDebugOverlayEnabled(bool bEnabled) { bDebugOverlay = bEnabled; }

	// ------------------------------------------------------------------ C++ API
	using FSenderStreamPtr = TSharedPtr<FCySenderStream, ESPMode::ThreadSafe>;
	using FReceiverStreamPtr = TSharedPtr<FCyReceiverStream, ESPMode::ThreadSafe>;

	/**
	 * Resolves the requested sender name against what already exists (other processes, other
	 * components, leftovers of a previous PIE session) according to the policy.
	 */
	bool ResolveSenderName(const FString& RequestedName, ECyGameCaptureNameCollisionPolicy Policy, FString& OutFinalName, FString& OutError) const;

	FSenderStreamPtr CreateSender(const FCySenderStreamConfig& Config, UObject* Owner, FString& OutError);
	void DestroySender(const FSenderStreamPtr& Stream);
	FSenderStreamPtr FindSender(const FString& SenderName) const;
	const TArray<FSenderStreamPtr>& GetSenders() const { return Senders; }

	FReceiverStreamPtr CreateReceiver(const FCyReceiverStreamConfig& Config, UObject* Owner, FString& OutError);
	void DestroyReceiver(const FReceiverStreamPtr& Stream);
	FReceiverStreamPtr FindReceiver(const FString& ReceiverName) const;
	const TArray<FReceiverStreamPtr>& GetReceivers() const { return Receivers; }

	/** Game viewport capture (SourceMode GameViewport): senders subscribe to the back buffer hook. */
	void RegisterViewportSender(const FSenderStreamPtr& Stream);
	void UnregisterViewportSender(const FSenderStreamPtr& Stream);

	bool IsDebugOverlayEnabled() const { return bDebugOverlay; }

private:
	bool Tick(float DeltaTime);
	void RefreshAvailableSenders();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void OnEnginePreExit();
	void DrawDebugOverlay();
	void DetectBackend();

	TArray<FSenderStreamPtr> Senders;
	TArray<FReceiverStreamPtr> Receivers;
	TArray<FCySpoutSenderInfo> CachedSenders;
	double NextDiscoveryTime = 0.0;
	bool bSpoutSupported = false;
	FString RHIName;
	FString UnsupportedReason;
	bool bDebugOverlay = false;

	TSharedPtr<FCyViewportCapture> ViewportCapture;

#if ENGINE_MAJOR_VERSION >= 5
	FTSTicker::FDelegateHandle TickHandle;
#else
	FDelegateHandle TickHandle;
#endif
	FDelegateHandle WorldCleanupHandle;
	FDelegateHandle PreExitHandle;
};
