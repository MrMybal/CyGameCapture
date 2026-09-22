// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — Project Settings > Plugins > CyGameCaptureUE
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CyGameCaptureSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "CyGameCaptureUE"))
class CYGAMECAPTUREUE_API UCyGameCaptureSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCyGameCaptureSettings();

	static const UCyGameCaptureSettings* Get() { return GetDefault<UCyGameCaptureSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Master switch. When false no Spout sender/receiver is ever created (components silently do nothing). */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	bool bEnableSpout = true;

	/** Prefix of default sender names: <Prefix>::<ProjectName>::<StreamName>. */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	FString DefaultSenderPrefix = TEXT("CyGameCaptureUE");

	/** Receivers keep looking for their sender / senders re-register after a device reset. */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	bool bAutoReconnect = true;

	/** Verbose logging (LogCyGameCapture at Verbose level, per-stream lifecycle details). */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bDebugLogging = false;

	/** Allow the D3D11 backend. */
	UPROPERTY(Config, EditAnywhere, Category = "Backends")
	bool bEnableD3D11 = true;

	/** Allow the D3D12 backend (D3D11On12 bridge). */
	UPROPERTY(Config, EditAnywhere, Category = "Backends")
	bool bEnableD3D12 = true;

	/** Allow senders/receivers to run in the editor (PIE and editor world). Standalone/packaged builds are always allowed. */
	UPROPERTY(Config, EditAnywhere, Category = "Editor")
	bool bAllowEditorCapture = true;

	/** How often the list of available Spout senders is refreshed, in seconds. */
	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float SenderDiscoveryIntervalSeconds = 1.0f;

	/** How often a receiver re-reads its sender information (size/format/handle changes), in seconds. */
	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ClampMin = "0.02", ClampMax = "5.0"))
	float ReceiverPollIntervalSeconds = 0.25f;

	/**
	 * Flush the D3D11 immediate context right after each sender copy (like Spout's own sender).
	 * Costs CPU time on the RHI thread every frame; the copy is submitted with the frame anyway.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Performance")
	bool bFlushAfterCopy = false;

	/**
	 * No artificial cap on simultaneous streams. The real ceiling is Spout's machine-wide sender name
	 * table (64 by default, see CySpout::CanRegisterNewSender) plus the GPU cost of one copy per stream
	 * per frame; receivers are not listed in that table at all and are only bound by GPU memory.
	 * This value exists so a runaway Blueprint loop cannot create streams forever, and can be raised freely.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ClampMin = "1", ClampMax = "4096"))
	int32 StreamSanityLimit = 256;
};
