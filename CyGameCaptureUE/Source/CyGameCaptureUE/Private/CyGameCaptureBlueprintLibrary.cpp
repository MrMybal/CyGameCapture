// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

#include "CyGameCaptureBlueprintLibrary.h"
#include "CyGameCaptureSubsystem.h"

UCyGameCaptureSubsystem* UCyGameCaptureBlueprintLibrary::GetCyGameCaptureSubsystem()
{
	return UCyGameCaptureSubsystem::Get();
}

bool UCyGameCaptureBlueprintLibrary::IsSpoutSupported()
{
	UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get();
	return Subsystem != nullptr && Subsystem->IsSpoutSupported();
}

TArray<FCySpoutSenderInfo> UCyGameCaptureBlueprintLibrary::GetAvailableSpoutSenders(bool bForceRefresh)
{
	UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get();
	return Subsystem != nullptr ? Subsystem->GetAvailableSpoutSenders(bForceRefresh) : TArray<FCySpoutSenderInfo>();
}

TArray<FString> UCyGameCaptureBlueprintLibrary::GetAvailableSpoutSenderNames(bool bForceRefresh)
{
	TArray<FString> Names;
	for (const FCySpoutSenderInfo& Info : GetAvailableSpoutSenders(bForceRefresh))
	{
		Names.Add(Info.Name);
	}
	return Names;
}

bool UCyGameCaptureBlueprintLibrary::FindSpoutSender(const FString& SenderName, FCySpoutSenderInfo& OutInfo)
{
	UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get();
	return Subsystem != nullptr && Subsystem->FindSpoutSender(SenderName, OutInfo);
}

FString UCyGameCaptureBlueprintLibrary::MakeDefaultSenderName(const FString& StreamName)
{
	UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get();
	return Subsystem != nullptr ? Subsystem->MakeDefaultSenderName(StreamName) : FString::Printf(TEXT("CyGameCaptureUE::%s"), *StreamName);
}

void UCyGameCaptureBlueprintLibrary::GetAllStreamStats(TArray<FCyGameCaptureStreamStats>& Senders, TArray<FCyGameCaptureStreamStats>& Receivers)
{
	if (UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get())
	{
		Subsystem->GetAllStreamStats(Senders, Receivers);
	}
}
