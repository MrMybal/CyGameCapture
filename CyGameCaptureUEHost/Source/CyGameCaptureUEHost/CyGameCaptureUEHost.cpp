// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

#include "CyGameCaptureUEHost.h"

#include "CyGameCaptureSubsystem.h"
#include "CyGameCaptureSpoutSenderComponent.h"
#include "CyGameCaptureSpoutReceiverComponent.h"
#include "CyGameCaptureBlueprintLibrary.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogCyGCHost, Log, All);

IMPLEMENT_PRIMARY_GAME_MODULE(FCyGameCaptureUEHostModule, CyGameCaptureUEHost, "CyGameCaptureUEHost");

void FCyGameCaptureUEHostModule::StartupModule() {}
void FCyGameCaptureUEHostModule::ShutdownModule() {}

// ------------------------------------------------------------------------------------------------
bool UCyGameCaptureHostTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return FParse::Param(FCommandLine::Get(), TEXT("CyGCTest"));
}

void UCyGameCaptureHostTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.IsGameWorld())
	{
		UE_LOG(LogCyGCHost, Log, TEXT("Spawning CyGameCapture host test actor"));
		InWorld.SpawnActor<ACyGameCaptureHostTestActor>();
	}
}

// ------------------------------------------------------------------------------------------------
ACyGameCaptureHostTestActor::ACyGameCaptureHostTestActor()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ACyGameCaptureHostTestActor::BeginPlay()
{
	Super::BeginPlay();
	StartTime = FPlatformTime::Seconds();
	FParse::Value(FCommandLine::Get(), TEXT("CyGCTestSeconds="), TestSeconds);

	UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get();
	UE_LOG(LogCyGCHost, Log, TEXT("Spout supported: %s (%s) RHI=%s"), Subsystem && Subsystem->IsSpoutSupported() ? TEXT("yes") : TEXT("no"), Subsystem ? *Subsystem->GetUnsupportedReason() : TEXT("no subsystem"), Subsystem ? *Subsystem->GetRHIName() : TEXT("?"));

	// Render targets driven from the game thread (colour animation)
	RenderTargetA = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_A"), RF_Transient);
	RenderTargetA->RenderTargetFormat = RTF_RGBA8;
	RenderTargetA->InitAutoFormat(1280, 720);
	RenderTargetA->UpdateResourceImmediate(true);

	RenderTargetB = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_B"), RF_Transient);
	RenderTargetB->RenderTargetFormat = RTF_RGBA16f;
	RenderTargetB->InitAutoFormat(640, 360);
	RenderTargetB->UpdateResourceImmediate(true);

	auto MakeSender = [this](const TCHAR* Name, ECyGameCaptureSourceMode Mode, UTextureRenderTarget2D* RT)
	{
		UCyGameCaptureSpoutSenderComponent* Sender = NewObject<UCyGameCaptureSpoutSenderComponent>(this, Name);
		Sender->StreamName = Name;
		Sender->SourceMode = Mode;
		Sender->RenderTarget = RT;
		Sender->bAutoStart = false;
		Sender->bDebug = true;
		Sender->SceneCaptureSize = FIntPoint(960, 540);
		Sender->RegisterComponent();
		Senders.Add(Sender);
		const bool bStarted = Sender->StartSender();
		UE_LOG(LogCyGCHost, Log, TEXT("Sender %s (%s): %s -> '%s'"), Name, *UEnum::GetValueAsString(Mode), bStarted ? TEXT("started") : TEXT("FAILED"), *Sender->GetActiveSenderName());
		return Sender;
	};

	MakeSender(TEXT("TestA"), ECyGameCaptureSourceMode::RenderTarget, RenderTargetA);
	MakeSender(TEXT("TestB"), ECyGameCaptureSourceMode::RenderTarget, RenderTargetB);
	MakeSender(TEXT("Camera"), ECyGameCaptureSourceMode::SceneCapture, nullptr);
	if (!FParse::Param(FCommandLine::Get(), TEXT("CyGCNoViewport")))
	{
		MakeSender(TEXT("Viewport"), ECyGameCaptureSourceMode::GameViewport, nullptr);
	}

	// Receivers: names from the command line, default = loop back on our own first sender
	FString ReceiveList;
	TArray<FString> Names;
	if (FParse::Value(FCommandLine::Get(), TEXT("CyGCReceive="), ReceiveList))
	{
		ReceiveList.ParseIntoArray(Names, TEXT(","));
	}
	if (Names.Num() == 0 && Senders.Num() > 0)
	{
		Names.Add(Senders[0]->GetActiveSenderName());
	}
	int32 Index = 0;
	for (const FString& Name : Names)
	{
		UCyGameCaptureSpoutReceiverComponent* Receiver = NewObject<UCyGameCaptureSpoutReceiverComponent>(this, *FString::Printf(TEXT("Receiver%d"), Index));
		Receiver->ReceiverName = FString::Printf(TEXT("Receiver%d"), Index);
		Receiver->SenderName = Name;
		Receiver->bAutoConnect = false;
		Receiver->bDebug = true;
		Receiver->RegisterComponent();
		Receivers.Add(Receiver);
		const bool bStarted = Receiver->StartReceiver();
		UE_LOG(LogCyGCHost, Log, TEXT("Receiver %d <- '%s': %s"), Index, *Name, bStarted ? TEXT("started") : TEXT("FAILED"));
		++Index;
	}
	if (Subsystem != nullptr)
	{
		Subsystem->SetDebugOverlayEnabled(true);
	}
}

void ACyGameCaptureHostTestActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Elapsed = FPlatformTime::Seconds() - StartTime;

	// Animated colours: A cycles hue, B pulses
	const float T = static_cast<float>(Elapsed);
	const FLinearColor ColorA = FLinearColor::MakeFromHSV8(static_cast<uint8>(FMath::Fmod(T * 40.0f, 255.0f)), 255, 255);
	const FLinearColor ColorB(0.5f + 0.5f * FMath::Sin(T * 3.0f), 0.2f, 0.5f + 0.5f * FMath::Cos(T * 2.0f), 1.0f);
	UKismetRenderingLibrary::ClearRenderTarget2D(this, RenderTargetA, ColorA);
	UKismetRenderingLibrary::ClearRenderTarget2D(this, RenderTargetB, ColorB);

	if (Elapsed >= NextReportTime)
	{
		NextReportTime = Elapsed + 2.0;
		LogReport();
	}
	if (TestSeconds > 0.0 && Elapsed >= TestSeconds)
	{
		UE_LOG(LogCyGCHost, Log, TEXT("Test duration reached, exiting"));
		FPlatformMisc::RequestExit(false);
	}
}

void ACyGameCaptureHostTestActor::LogReport()
{
	TArray<FCyGameCaptureStreamStats> SenderStats, ReceiverStats;
	UCyGameCaptureBlueprintLibrary::GetAllStreamStats(SenderStats, ReceiverStats);
	UE_LOG(LogCyGCHost, Log, TEXT("---- report %d (t=%.1fs) ----"), ReportIndex++, FPlatformTime::Seconds() - StartTime);
	for (const FCyGameCaptureStreamStats& S : SenderStats)
	{
		UE_LOG(LogCyGCHost, Log, TEXT("SENDER   %-48s %s %dx%d %-20s %6.1f fps frames=%lld dropped=%lld copy=%.3fms %s"), *S.Name, *UEnum::GetValueAsString(S.State), S.Width, S.Height, *S.Format, S.FPS, S.FrameCount, S.DroppedFrames, S.CopyTimeMs, *S.LastError);
	}
	for (int32 i = 0; i < ReceiverStats.Num(); ++i)
	{
		const FCyGameCaptureStreamStats& S = ReceiverStats[i];
		FString Pixel;
		// Test-only readback to prove the received image is not empty (never done by the plugin itself)
		if (S.bConnected && Receivers.IsValidIndex(i))
		{
			if (UTextureRenderTarget2D* RT = Receivers[i]->GetReceiverTexture())
			{
				if (FTextureRenderTargetResource* Res = RT->GameThread_GetRenderTargetResource())
				{
					TArray<FColor> Pixels;
					if (Res->ReadPixels(Pixels) && Pixels.Num() > 0)
					{
						uint64 R = 0, G = 0, B = 0;
						for (const FColor& C : Pixels) { R += C.R; G += C.G; B += C.B; }
						Pixel = FString::Printf(TEXT("mean RGB=(%.1f, %.1f, %.1f)"), static_cast<double>(R) / Pixels.Num(), static_cast<double>(G) / Pixels.Num(), static_cast<double>(B) / Pixels.Num());
					}
				}
			}
		}
		UE_LOG(LogCyGCHost, Log, TEXT("RECEIVER %-16s <- %-40s %s %dx%d %-20s %6.1f fps frames=%lld dropped=%lld %s %s"), *Receivers[i]->ReceiverName, *S.Name, S.bConnected ? TEXT("Connected") : TEXT("Waiting  "), S.Width, S.Height, *S.Format, S.FPS, S.FrameCount, S.DroppedFrames, *Pixel, *S.LastError);
	}
	const TArray<FCySpoutSenderInfo> All = UCyGameCaptureBlueprintLibrary::GetAvailableSpoutSenders(true);
	UE_LOG(LogCyGCHost, Log, TEXT("Spout senders on this machine: %d"), All.Num());
	for (const FCySpoutSenderInfo& Info : All)
	{
		UE_LOG(LogCyGCHost, Log, TEXT("   %-48s %dx%d %s %s"), *Info.Name, Info.Width, Info.Height, *Info.FormatName, Info.bOwnedByThisProcess ? TEXT("(this process)") : *Info.OwnerExecutable);
	}
}
