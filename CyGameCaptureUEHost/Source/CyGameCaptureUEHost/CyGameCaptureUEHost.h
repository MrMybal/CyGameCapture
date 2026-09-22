// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUEHost — test harness for the CyGameCaptureUE plugin.
//
// Launch the game with -CyGCTest to spawn the test actor automatically:
//   -CyGCTest                       spawn the harness
//   -CyGCReceive=Name1,Name2        Spout senders to receive (default: loop back on our own first sender)
//   -CyGCTestSeconds=20             quit after N seconds (0 = run forever)
//   -CyGCNoViewport                 do not create the game viewport sender
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "CyGameCaptureUEHost.generated.h"

class UTextureRenderTarget2D;
class UCyGameCaptureSpoutSenderComponent;
class UCyGameCaptureSpoutReceiverComponent;

class FCyGameCaptureUEHostModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

/** Spawns 4 senders (2 render targets, 1 scene capture, 1 game viewport) and up to 4 receivers, logs statistics. */
UCLASS()
class ACyGameCaptureHostTestActor : public AActor
{
	GENERATED_BODY()

public:
	ACyGameCaptureHostTestActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Transient)
	UTextureRenderTarget2D* RenderTargetA = nullptr;

	UPROPERTY(Transient)
	UTextureRenderTarget2D* RenderTargetB = nullptr;

	UPROPERTY(Transient)
	TArray<UCyGameCaptureSpoutSenderComponent*> Senders;

	UPROPERTY(Transient)
	TArray<UCyGameCaptureSpoutReceiverComponent*> Receivers;

private:
	void LogReport();
	double StartTime = 0.0;
	double NextReportTime = 0.0;
	double TestSeconds = 20.0;
	int32 ReportIndex = 0;
};

/** Creates the test actor when -CyGCTest is on the command line. */
UCLASS()
class UCyGameCaptureHostTestSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
