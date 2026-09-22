// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
//
// CyGameCaptureUE — automation tests.
//
//   Automation RunTests CyGameCapture
//
// The GPU loop-back test (UE sender -> Spout -> UE receiver) needs a D3D11 / D3D12 RHI, i.e. an editor
// or game running with a real device (not -nullrhi). Tests that cannot run log a warning and pass.
#include "Misc/AutomationTest.h"
#include "CyGameCaptureSubsystem.h"
#include "CyGameCaptureStream.h"
#include "CyGameCaptureLog.h"
#include "RHI/CyRHICompat.h"
#include "Spout/CySpoutRegistry.h"

#include "Engine/TextureRenderTarget2D.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// 5.5 turned EAutomationTestFlags into an enum class and moved the mask to a separate constant
#if CY_UE_AT_LEAST(5, 5)
#define CYGC_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
#else
#define CYGC_TEST_FLAGS (EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
#endif

// ------------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCyGameCaptureNamingTest, "CyGameCapture.Naming.DefaultSenderName", CYGC_TEST_FLAGS)
bool FCyGameCaptureNamingTest::RunTest(const FString& Parameters)
{
	UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get();
	if (!TestNotNull(TEXT("Subsystem"), Subsystem))
	{
		return false;
	}
	const FString Name = Subsystem->MakeDefaultSenderName(TEXT("Camera01"));
	TestTrue(TEXT("Prefix"), Name.StartsWith(TEXT("CyGameCaptureUE::")));
	TestTrue(TEXT("Stream suffix"), Name.EndsWith(TEXT("::Camera01")));
	TArray<FString> Parts;
	Name.ParseIntoArray(Parts, TEXT("::"));
	TestEqual(TEXT("Three parts"), Parts.Num(), 3);
	TestTrue(TEXT("Is CyGameCapture name"), CySpout::IsCyGameCaptureName(Name));
	return true;
}

// ------------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCyGameCaptureCollisionTest, "CyGameCapture.Naming.CollisionPolicy", CYGC_TEST_FLAGS)
bool FCyGameCaptureCollisionTest::RunTest(const FString& Parameters)
{
	UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get();
	if (!TestNotNull(TEXT("Subsystem"), Subsystem))
	{
		return false;
	}
	if (!Subsystem->IsSpoutSupported())
	{
		AddWarning(FString::Printf(TEXT("Spout not supported here (%s), skipping"), *Subsystem->GetUnsupportedReason()));
		return true;
	}

	const FString Base = TEXT("CyGameCaptureUE::AutomationTest::Collision");
	FString Final, Error;
	TestTrue(TEXT("Free name resolves to itself"), Subsystem->ResolveSenderName(Base, ECyGameCaptureNameCollisionPolicy::Fail, Final, Error));
	TestEqual(TEXT("Same name"), Final, Base);

	FCySenderStreamConfig Config;
	Config.SenderName = Base;
	UCyGameCaptureSubsystem::FSenderStreamPtr Stream = Subsystem->CreateSender(Config, nullptr, Error);
	if (!TestTrue(FString::Printf(TEXT("CreateSender (%s)"), *Error), Stream.IsValid()))
	{
		return false;
	}

	TestFalse(TEXT("Fail policy refuses a local duplicate"), Subsystem->ResolveSenderName(Base, ECyGameCaptureNameCollisionPolicy::Fail, Final, Error));
	TestFalse(TEXT("Replace policy refuses a local duplicate"), Subsystem->ResolveSenderName(Base, ECyGameCaptureNameCollisionPolicy::ReplaceIfOwnedByThisProcess, Final, Error));
	TestTrue(TEXT("AutoRename finds a free name"), Subsystem->ResolveSenderName(Base, ECyGameCaptureNameCollisionPolicy::AutoRename, Final, Error));
	TestEqual(TEXT("AutoRename suffix"), Final, Base + TEXT("::2"));

	FCySenderStreamConfig Dup = Config;
	TestFalse(TEXT("Duplicate CreateSender rejected"), Subsystem->CreateSender(Dup, nullptr, Error).IsValid());

	Subsystem->DestroySender(Stream);
	TestFalse(TEXT("Destroyed sender no longer found"), Subsystem->FindSender(Base).IsValid());
	return true;
}

// ------------------------------------------------------------------------------------------------
// GPU loop-back: render target -> sender -> Spout -> receiver -> render target (same process)
namespace
{
	struct FCyLoopbackState
	{
		TStrongObjectPtr<UTextureRenderTarget2D> SourceRT;
		TStrongObjectPtr<UTextureRenderTarget2D> TargetRT;
		UCyGameCaptureSubsystem::FSenderStreamPtr Sender;
		UCyGameCaptureSubsystem::FReceiverStreamPtr Receiver;
		FString SenderName;
		double StartTime = 0.0;
	};
	TSharedPtr<FCyLoopbackState> GLoopback;

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCyWaitSeconds, double, Seconds);
	bool FCyWaitSeconds::Update()
	{
		if (StartTime == 0.0)
		{
			StartTime = FPlatformTime::Seconds();
		}
		return FPlatformTime::Seconds() - StartTime >= Seconds;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCyLoopbackCheckSender, FAutomationTestBase*, Test);
	bool FCyLoopbackCheckSender::Update()
	{
		if (!GLoopback.IsValid())
		{
			return true;
		}
		FCySpoutSenderInfo Info;
		void* Handle = nullptr;
		const bool bVisible = CySpout::GetSenderInfo(GLoopback->SenderName, Info, Handle);
		Test->TestTrue(TEXT("Sender visible to Spout"), bVisible);
		if (bVisible)
		{
			Test->TestEqual(TEXT("Sender width"), Info.Width, 256);
			Test->TestEqual(TEXT("Sender height"), Info.Height, 128);
			Test->TestEqual(TEXT("Sender format"), Info.DxgiFormat, static_cast<int32>(CyDxgi::B8G8R8A8_UNORM));
			Test->TestTrue(TEXT("Owned by this process"), Info.bOwnedByThisProcess);
		}
		const FCyGameCaptureStreamStats Stats = GLoopback->Sender->GetStats();
		Test->TestTrue(FString::Printf(TEXT("Sender sent frames (%lld)"), Stats.FrameCount), Stats.FrameCount > 0);
		Test->TestEqual(TEXT("Sender state"), Stats.State, ECyGameCaptureStreamState::Active);

		// Now connect a receiver to our own sender
		UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get();
		GLoopback->TargetRT.Reset(NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient));
		GLoopback->TargetRT->InitCustomFormat(256, 128, PF_B8G8R8A8, false);
		GLoopback->TargetRT->UpdateResourceImmediate(true);

		FCyReceiverStreamConfig Config;
		Config.ReceiverName = TEXT("LoopbackReceiver");
		Config.SenderName = GLoopback->SenderName;
		FString Error;
		GLoopback->Receiver = Subsystem->CreateReceiver(Config, nullptr, Error);
		Test->TestTrue(FString::Printf(TEXT("CreateReceiver (%s)"), *Error), GLoopback->Receiver.IsValid());
		if (GLoopback->Receiver.IsValid())
		{
			GLoopback->Receiver->SetTargetRenderTarget(GLoopback->TargetRT.Get());
		}
		return true;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCyLoopbackCheckReceiver, FAutomationTestBase*, Test);
	bool FCyLoopbackCheckReceiver::Update()
	{
		if (!GLoopback.IsValid() || !GLoopback->Receiver.IsValid())
		{
			return true;
		}
		const FCyGameCaptureStreamStats Stats = GLoopback->Receiver->GetStats();
		Test->TestTrue(TEXT("Receiver connected"), Stats.bConnected);
		Test->TestTrue(FString::Printf(TEXT("Receiver received frames (%lld)"), Stats.FrameCount), Stats.FrameCount > 0);

		// Test-only CPU readback of the receiver target: the source was cleared to a known colour
		if (Stats.bConnected && GLoopback->TargetRT.IsValid())
		{
			FTextureRenderTargetResource* Resource = GLoopback->TargetRT->GameThread_GetRenderTargetResource();
			TArray<FColor> Pixels;
			if (Resource != nullptr && Resource->ReadPixels(Pixels) && Pixels.Num() > 0)
			{
				const FColor Center = Pixels[Pixels.Num() / 2 + 128];
				Test->TestTrue(FString::Printf(TEXT("Received pixel is the source colour (got %s)"), *Center.ToString()),
					FMath::Abs(static_cast<int32>(Center.R) - 255) <= 2 && FMath::Abs(static_cast<int32>(Center.G) - 128) <= 3 && Center.B <= 2);
			}
			else
			{
				Test->AddWarning(TEXT("ReadPixels failed; pixel check skipped"));
			}
		}

		UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get();
		const FString Name = GLoopback->SenderName;
		Subsystem->DestroyReceiver(GLoopback->Receiver);
		Subsystem->DestroySender(GLoopback->Sender);
		GLoopback.Reset();
		FlushRenderingCommands();
		Test->TestFalse(TEXT("Sender unregistered from Spout after destruction"), CySpout::SenderExists(Name));
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCyGameCaptureLoopbackTest, "CyGameCapture.GPU.Loopback", CYGC_TEST_FLAGS)
bool FCyGameCaptureLoopbackTest::RunTest(const FString& Parameters)
{
	UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get();
	if (!TestNotNull(TEXT("Subsystem"), Subsystem))
	{
		return false;
	}
	if (!Subsystem->IsSpoutSupported())
	{
		AddWarning(FString::Printf(TEXT("Spout not supported here (%s), skipping GPU loop-back"), *Subsystem->GetUnsupportedReason()));
		return true;
	}

	GLoopback = MakeShared<FCyLoopbackState>();
	GLoopback->SenderName = TEXT("CyGameCaptureUE::AutomationTest::Loopback");
	GLoopback->SourceRT.Reset(NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient));
	GLoopback->SourceRT->ClearColor = FLinearColor(1.0f, 0.2159f, 0.0f, 1.0f);   // sRGB (255, 128, 0)
	GLoopback->SourceRT->InitCustomFormat(256, 128, PF_B8G8R8A8, false);
	GLoopback->SourceRT->UpdateResourceImmediate(true);

	FCySenderStreamConfig Config;
	Config.SenderName = GLoopback->SenderName;
	FString Error;
	GLoopback->Sender = Subsystem->CreateSender(Config, nullptr, Error);
	if (!TestTrue(FString::Printf(TEXT("CreateSender (%s)"), *Error), GLoopback->Sender.IsValid()))
	{
		GLoopback.Reset();
		return false;
	}
	GLoopback->Sender->SetSourceRenderTarget(GLoopback->SourceRT.Get());

	ADD_LATENT_AUTOMATION_COMMAND(FCyWaitSeconds(1.0));
	ADD_LATENT_AUTOMATION_COMMAND(FCyLoopbackCheckSender(this));
	ADD_LATENT_AUTOMATION_COMMAND(FCyWaitSeconds(1.5));
	ADD_LATENT_AUTOMATION_COMMAND(FCyLoopbackCheckReceiver(this));
	return true;
}

// ------------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCyGameCaptureMultiStreamTest, "CyGameCapture.Streams.CreateDestroyMany", CYGC_TEST_FLAGS)
bool FCyGameCaptureMultiStreamTest::RunTest(const FString& Parameters)
{
	UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get();
	if (!TestNotNull(TEXT("Subsystem"), Subsystem))
	{
		return false;
	}
	if (!Subsystem->IsSpoutSupported())
	{
		AddWarning(TEXT("Spout not supported here, skipping"));
		return true;
	}
	const int32 Before = Subsystem->GetActiveStreamCount();
	TArray<UCyGameCaptureSubsystem::FSenderStreamPtr> Senders;
	TArray<UCyGameCaptureSubsystem::FReceiverStreamPtr> Receivers;
	FString Error;
	for (int32 i = 0; i < 4; ++i)
	{
		FCySenderStreamConfig S;
		S.SenderName = FString::Printf(TEXT("CyGameCaptureUE::AutomationTest::Multi%d"), i);
		Senders.Add(Subsystem->CreateSender(S, nullptr, Error));
		TestTrue(FString::Printf(TEXT("Sender %d created (%s)"), i, *Error), Senders.Last().IsValid());

		FCyReceiverStreamConfig R;
		R.ReceiverName = FString::Printf(TEXT("MultiReceiver%d"), i);
		R.SenderName = FString::Printf(TEXT("DoesNotExist%d"), i);
		Receivers.Add(Subsystem->CreateReceiver(R, nullptr, Error));
		TestTrue(FString::Printf(TEXT("Receiver %d created (%s)"), i, *Error), Receivers.Last().IsValid());
		if (Receivers.Last().IsValid())
		{
			TestFalse(TEXT("Receiver of a missing sender is not connected"), Receivers.Last()->IsConnected());
			TestEqual(TEXT("Receiver waits"), Receivers.Last()->GetState(), ECyGameCaptureStreamState::Waiting);
		}
	}
	TestEqual(TEXT("8 streams registered"), Subsystem->GetActiveStreamCount(), Before + 8);
	for (const auto& S : Senders) { Subsystem->DestroySender(S); }
	for (const auto& R : Receivers) { Subsystem->DestroyReceiver(R); }
	TestEqual(TEXT("All streams destroyed"), Subsystem->GetActiveStreamCount(), Before);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
