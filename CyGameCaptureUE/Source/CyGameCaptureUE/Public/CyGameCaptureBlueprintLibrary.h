// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — Blueprint function library (thin wrappers over the subsystem).
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CyGameCaptureTypes.h"
#include "CyGameCaptureBlueprintLibrary.generated.h"

class UCyGameCaptureSubsystem;

UCLASS()
class CYGAMECAPTUREUE_API UCyGameCaptureBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The CyGameCapture engine subsystem. */
	UFUNCTION(BlueprintPure, Category = "CyGameCapture", meta = (DisplayName = "Get CyGameCapture Subsystem"))
	static UCyGameCaptureSubsystem* GetCyGameCaptureSubsystem();

	/** True when Spout streams can be created (Windows, D3D11/D3D12, plugin enabled). */
	UFUNCTION(BlueprintPure, Category = "CyGameCapture")
	static bool IsSpoutSupported();

	/** Every Spout sender currently visible on this machine (OBS, Resolume, other Unreal instances, CyGameCaptureRS, ...). */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Spout")
	static TArray<FCySpoutSenderInfo> GetAvailableSpoutSenders(bool bForceRefresh = false);

	/** Names only, convenient for UI lists. */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Spout")
	static TArray<FString> GetAvailableSpoutSenderNames(bool bForceRefresh = false);

	/** Live lookup of one sender. */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Spout")
	static bool FindSpoutSender(const FString& SenderName, FCySpoutSenderInfo& OutInfo);

	/** "<Prefix>::<ProjectName>::<StreamName>" using the project settings prefix. */
	UFUNCTION(BlueprintPure, Category = "CyGameCapture")
	static FString MakeDefaultSenderName(const FString& StreamName);

	/** Statistics of all running streams. */
	UFUNCTION(BlueprintCallable, Category = "CyGameCapture|Stats")
	static void GetAllStreamStats(TArray<FCyGameCaptureStreamStats>& Senders, TArray<FCyGameCaptureStreamStats>& Receivers);
};
