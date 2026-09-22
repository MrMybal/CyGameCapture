// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT

#include "CyGameCaptureSubsystem.h"
#include "CyGameCaptureSettings.h"
#include "CyGameCaptureLog.h"
#include "RHI/CyRHICompat.h"
#include "Spout/CySpoutBackend.h"
#include "Spout/CySpoutRegistry.h"
#include "Viewport/CyViewportCapture.h"

#include "Engine/Engine.h"
#include "UObject/Class.h"
#include "Engine/World.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "HAL/IConsoleManager.h"
#include "RenderingThread.h"

namespace
{
	FAutoConsoleCommand GCyCmdListSenders(TEXT("CyGameCapture.ListSenders"), TEXT("Lists the CyGameCaptureUE sender streams of this process"), FConsoleCommandDelegate::CreateLambda([]()
	{
		if (UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get())
		{
			for (const auto& Stream : Subsystem->GetSenders())
			{
				const FCyGameCaptureStreamStats Stats = Stream->GetStats();
				CYGC_LOG(Display, TEXT("Sender   %-48s %5dx%-5d %-20s %6.1f fps  frames %lld  dropped %lld  copy %.3f ms  %s"), *Stats.Name, Stats.Width, Stats.Height, *Stats.Format, Stats.FPS, Stats.FrameCount, Stats.DroppedFrames, Stats.CopyTimeMs, *UEnum::GetValueAsString(Stats.State));
			}
			CYGC_LOG(Display, TEXT("%d sender stream(s)"), Subsystem->GetSenders().Num());
		}
	}));

	FAutoConsoleCommand GCyCmdListReceivers(TEXT("CyGameCapture.ListReceivers"), TEXT("Lists the CyGameCaptureUE receiver streams of this process"), FConsoleCommandDelegate::CreateLambda([]()
	{
		if (UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get())
		{
			for (const auto& Stream : Subsystem->GetReceivers())
			{
				const FCyGameCaptureStreamStats Stats = Stream->GetStats();
				CYGC_LOG(Display, TEXT("Receiver %-24s <- %-40s %5dx%-5d %-20s %6.1f fps  frames %lld  dropped %lld  %s"), *Stream->GetConfig().ReceiverName, *Stats.Name, Stats.Width, Stats.Height, *Stats.Format, Stats.FPS, Stats.FrameCount, Stats.DroppedFrames, Stats.bConnected ? TEXT("Connected") : TEXT("Waiting"));
			}
			CYGC_LOG(Display, TEXT("%d receiver stream(s)"), Subsystem->GetReceivers().Num());
		}
	}));

	FAutoConsoleCommand GCyCmdSpoutList(TEXT("CyGameCapture.Spout.List"), TEXT("Lists every Spout sender visible on this machine"), FConsoleCommandDelegate::CreateLambda([]()
	{
		const TArray<FCySpoutSenderInfo> Senders = CySpout::EnumerateSenders();
		for (const FCySpoutSenderInfo& Info : Senders)
		{
			CYGC_LOG(Display, TEXT("Spout    %-48s %5dx%-5d %-20s %s%s"), *Info.Name, Info.Width, Info.Height, *Info.FormatName, *Info.OwnerExecutable, Info.bOwnedByThisProcess ? TEXT(" (this process)") : TEXT(""));
		}
		CYGC_LOG(Display, TEXT("%d Spout sender(s)"), Senders.Num());
	}));

	FAutoConsoleCommand GCyCmdDebug(TEXT("CyGameCapture.Debug"), TEXT("CyGameCapture.Debug 0|1 : on-screen stream overlay"), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (UCyGameCaptureSubsystem* Subsystem = UCyGameCaptureSubsystem::Get())
		{
			const bool bEnable = Args.Num() == 0 ? !Subsystem->IsDebugOverlayEnabled() : (Args[0] != TEXT("0"));
			Subsystem->SetDebugOverlayEnabled(bEnable);
		}
	}));
}

UCyGameCaptureSubsystem* UCyGameCaptureSubsystem::Get()
{
	return GEngine != nullptr ? GEngine->GetEngineSubsystem<UCyGameCaptureSubsystem>() : nullptr;
}

void UCyGameCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ViewportCapture = MakeShared<FCyViewportCapture>();

#if ENGINE_MAJOR_VERSION >= 5
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCyGameCaptureSubsystem::Tick), 0.0f);
#else
	TickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCyGameCaptureSubsystem::Tick), 0.0f);
#endif
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UCyGameCaptureSubsystem::OnWorldCleanup);
	PreExitHandle = FCoreDelegates::OnEnginePreExit.AddUObject(this, &UCyGameCaptureSubsystem::OnEnginePreExit);

	DetectBackend();
	CYGC_LOG(Log, TEXT("Subsystem initialized. RHI: %s. Spout: %s%s%s. Spout senders visible: %d"), *RHIName,
		bSpoutSupported ? TEXT("available") : TEXT("unavailable"), bSpoutSupported ? TEXT("") : TEXT(" - "), bSpoutSupported ? TEXT("") : *UnsupportedReason,
		CySpout::IsCompiledIn() ? CySpout::EnumerateSenders().Num() : 0);
}

void UCyGameCaptureSubsystem::Deinitialize()
{
	StopAllStreams();
	ViewportCapture.Reset();

#if ENGINE_MAJOR_VERSION >= 5
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
#else
	FTicker::GetCoreTicker().RemoveTicker(TickHandle);
#endif
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	FCoreDelegates::OnEnginePreExit.Remove(PreExitHandle);

	Super::Deinitialize();
}

void UCyGameCaptureSubsystem::DetectBackend()
{
	bSpoutSupported = CySpoutBackend::IsSupported(UnsupportedReason, RHIName);
	if (bSpoutSupported)
	{
		UnsupportedReason.Reset();
	}
}

bool UCyGameCaptureSubsystem::IsSpoutSupported() const
{
	if (!bSpoutSupported && CyGetRHIType() != ECyRHIType::Unknown)
	{
		// Re-evaluate once the RHI exists (Initialize may run before GDynamicRHI in some configurations)
		const_cast<UCyGameCaptureSubsystem*>(this)->DetectBackend();
	}
	return bSpoutSupported;
}

FString UCyGameCaptureSubsystem::GetRHIName() const
{
	return CyRHITypeName(CyGetRHIType());
}

TArray<FCySpoutSenderInfo> UCyGameCaptureSubsystem::GetAvailableSpoutSenders(bool bForceRefresh)
{
	if (bForceRefresh || CachedSenders.Num() == 0)
	{
		RefreshAvailableSenders();
	}
	return CachedSenders;
}

bool UCyGameCaptureSubsystem::FindSpoutSender(const FString& SenderName, FCySpoutSenderInfo& OutInfo) const
{
	void* Handle = nullptr;
	return CySpout::GetSenderInfo(SenderName, OutInfo, Handle);
}

void UCyGameCaptureSubsystem::RefreshAvailableSenders()
{
	CachedSenders = CySpout::EnumerateSenders();
	const UCyGameCaptureSettings* Settings = UCyGameCaptureSettings::Get();
	NextDiscoveryTime = FPlatformTime::Seconds() + (Settings != nullptr ? FMath::Max(0.1f, Settings->SenderDiscoveryIntervalSeconds) : 1.0f);
}

FString UCyGameCaptureSubsystem::MakeDefaultSenderName(const FString& StreamName) const
{
	const UCyGameCaptureSettings* Settings = UCyGameCaptureSettings::Get();
	FString Prefix = Settings != nullptr ? Settings->DefaultSenderPrefix : TEXT("CyGameCaptureUE");
	if (Prefix.IsEmpty())
	{
		Prefix = TEXT("CyGameCaptureUE");
	}
	FString Project = FApp::GetProjectName();
	if (Project.IsEmpty())
	{
		Project = TEXT("Unreal");
	}
	FString Stream = StreamName.IsEmpty() ? TEXT("Stream") : StreamName;
	// "::" is our separator, keep the parts clean
	Project.ReplaceInline(TEXT("::"), TEXT("_"));
	Stream.ReplaceInline(TEXT("::"), TEXT("_"));
	return FString::Printf(TEXT("%s::%s::%s"), *Prefix, *Project, *Stream);
}

bool UCyGameCaptureSubsystem::ResolveSenderName(const FString& RequestedName, ECyGameCaptureNameCollisionPolicy Policy, FString& OutFinalName, FString& OutError) const
{
	OutFinalName.Reset();
	if (RequestedName.IsEmpty())
	{
		OutError = TEXT("Sender name is empty");
		return false;
	}
	// Spout sender names are limited to 255 characters
	FString Base = RequestedName.Left(200);

	auto TakenLocally = [this](const FString& Name)
	{
		return FindSender(Name).IsValid();
	};

	FCySpoutSenderInfo Existing;
	void* Handle = nullptr;
	const bool bExistsGlobally = CySpout::GetSenderInfo(Base, Existing, Handle);
	const bool bExistsLocally = TakenLocally(Base);

	if (!bExistsGlobally && !bExistsLocally)
	{
		OutFinalName = Base;
		return true;
	}

	switch (Policy)
	{
	case ECyGameCaptureNameCollisionPolicy::Fail:
		OutError = FString::Printf(TEXT("A Spout sender named '%s' already exists%s"), *Base, bExistsGlobally && !Existing.OwnerExecutable.IsEmpty() ? *FString::Printf(TEXT(" (owner: %s)"), *Existing.OwnerExecutable) : TEXT(""));
		return false;

	case ECyGameCaptureNameCollisionPolicy::ReplaceIfOwnedByThisProcess:
		if (bExistsLocally)
		{
			OutError = FString::Printf(TEXT("Another CyGameCaptureUE sender of this process already uses '%s'"), *Base);
			return false;
		}
		if (bExistsGlobally && !Existing.bOwnedByThisProcess)
		{
			OutError = FString::Printf(TEXT("'%s' belongs to another application (%s); refusing to replace it"), *Base, *Existing.OwnerExecutable);
			return false;
		}
		// Leftover of this process (previous PIE session): take it over
		OutFinalName = Base;
		return true;

	case ECyGameCaptureNameCollisionPolicy::AutoRename:
	default:
		for (int32 Index = 2; Index < 100; ++Index)
		{
			const FString Candidate = FString::Printf(TEXT("%s::%d"), *Base, Index);
			if (!CySpout::SenderExists(Candidate) && !TakenLocally(Candidate))
			{
				OutFinalName = Candidate;
				return true;
			}
		}
		OutError = FString::Printf(TEXT("No free name found for '%s'"), *Base);
		return false;
	}
}

UCyGameCaptureSubsystem::FSenderStreamPtr UCyGameCaptureSubsystem::CreateSender(const FCySenderStreamConfig& Config, UObject* Owner, FString& OutError)
{
	check(IsInGameThread());
	if (!IsSpoutSupported())
	{
		OutError = UnsupportedReason.IsEmpty() ? TEXT("Spout not supported") : UnsupportedReason;
		return nullptr;
	}
	const UCyGameCaptureSettings* Settings = UCyGameCaptureSettings::Get();
	if (Settings != nullptr && GetActiveStreamCount() >= Settings->StreamSanityLimit)
	{
		OutError = FString::Printf(TEXT("Stream sanity limit reached (%d). Raise StreamSanityLimit in the project settings if this is intended."), Settings->StreamSanityLimit);
		return nullptr;
	}
	// The one real limit: Spout's machine-wide sender table, shared with every other Spout application
	FString CapacityReason;
	if (!CySpout::CanRegisterNewSender(CapacityReason))
	{
		OutError = CapacityReason;
		return nullptr;
	}
	if (FindSender(Config.SenderName).IsValid())
	{
		OutError = FString::Printf(TEXT("Sender '%s' already exists in this process"), *Config.SenderName);
		return nullptr;
	}
	FSenderStreamPtr Stream = MakeShared<FCySenderStream, ESPMode::ThreadSafe>(Config);
	Stream->Owner = Owner;
	if (!Stream->Start(OutError))
	{
		return nullptr;
	}
	Senders.Add(Stream);
	if (Config.SourceMode == ECyGameCaptureSourceMode::GameViewport)
	{
		RegisterViewportSender(Stream);
	}
	return Stream;
}

void UCyGameCaptureSubsystem::DestroySender(const FSenderStreamPtr& Stream)
{
	check(IsInGameThread());
	if (!Stream.IsValid())
	{
		return;
	}
	UnregisterViewportSender(Stream);
	Stream->Stop();
	Senders.Remove(Stream);
}

UCyGameCaptureSubsystem::FSenderStreamPtr UCyGameCaptureSubsystem::FindSender(const FString& SenderName) const
{
	for (const FSenderStreamPtr& Stream : Senders)
	{
		if (Stream->GetSenderName() == SenderName)
		{
			return Stream;
		}
	}
	return nullptr;
}

UCyGameCaptureSubsystem::FReceiverStreamPtr UCyGameCaptureSubsystem::CreateReceiver(const FCyReceiverStreamConfig& Config, UObject* Owner, FString& OutError)
{
	check(IsInGameThread());
	if (!IsSpoutSupported())
	{
		OutError = UnsupportedReason.IsEmpty() ? TEXT("Spout not supported") : UnsupportedReason;
		return nullptr;
	}
	// Receivers take no slot in Spout's sender table: only the sanity limit applies to them
	const UCyGameCaptureSettings* Settings = UCyGameCaptureSettings::Get();
	if (Settings != nullptr && GetActiveStreamCount() >= Settings->StreamSanityLimit)
	{
		OutError = FString::Printf(TEXT("Stream sanity limit reached (%d). Raise StreamSanityLimit in the project settings if this is intended."), Settings->StreamSanityLimit);
		return nullptr;
	}
	FReceiverStreamPtr Stream = MakeShared<FCyReceiverStream, ESPMode::ThreadSafe>(Config);
	Stream->Owner = Owner;
	if (!Stream->Start(OutError))
	{
		return nullptr;
	}
	Receivers.Add(Stream);
	return Stream;
}

void UCyGameCaptureSubsystem::DestroyReceiver(const FReceiverStreamPtr& Stream)
{
	check(IsInGameThread());
	if (!Stream.IsValid())
	{
		return;
	}
	Stream->Stop();
	Receivers.Remove(Stream);
}

UCyGameCaptureSubsystem::FReceiverStreamPtr UCyGameCaptureSubsystem::FindReceiver(const FString& ReceiverName) const
{
	for (const FReceiverStreamPtr& Stream : Receivers)
	{
		if (Stream->GetConfig().ReceiverName == ReceiverName)
		{
			return Stream;
		}
	}
	return nullptr;
}

void UCyGameCaptureSubsystem::RegisterViewportSender(const FSenderStreamPtr& Stream)
{
	if (ViewportCapture.IsValid())
	{
		ViewportCapture->Tick();
		ViewportCapture->AddSender(Stream);
	}
}

void UCyGameCaptureSubsystem::UnregisterViewportSender(const FSenderStreamPtr& Stream)
{
	if (ViewportCapture.IsValid())
	{
		ViewportCapture->RemoveSender(Stream);
	}
}

void UCyGameCaptureSubsystem::GetAllStreamStats(TArray<FCyGameCaptureStreamStats>& OutSenders, TArray<FCyGameCaptureStreamStats>& OutReceivers) const
{
	OutSenders.Reset();
	OutReceivers.Reset();
	for (const FSenderStreamPtr& Stream : Senders)
	{
		OutSenders.Add(Stream->GetStats());
	}
	for (const FReceiverStreamPtr& Stream : Receivers)
	{
		FCyGameCaptureStreamStats Stats = Stream->GetStats();
		OutReceivers.Add(Stats);
	}
}

void UCyGameCaptureSubsystem::StopAllStreams()
{
	check(IsInGameThread());
	for (const FSenderStreamPtr& Stream : Senders)
	{
		UnregisterViewportSender(Stream);
		Stream->Stop();
	}
	for (const FReceiverStreamPtr& Stream : Receivers)
	{
		Stream->Stop();
	}
	Senders.Reset();
	Receivers.Reset();
}

bool UCyGameCaptureSubsystem::Tick(float DeltaTime)
{
	// Ghost prevention: streams whose owner object died are stopped here
	for (int32 i = Senders.Num() - 1; i >= 0; --i)
	{
		if (!Senders[i]->Owner.IsValid() && Senders[i]->Owner.IsStale())
		{
			CYGC_LOG(Warning, TEXT("Sender '%s' owner destroyed without stopping the stream; stopping it"), *Senders[i]->GetSenderName());
			DestroySender(Senders[i]);
		}
	}
	for (int32 i = Receivers.Num() - 1; i >= 0; --i)
	{
		if (!Receivers[i]->Owner.IsValid() && Receivers[i]->Owner.IsStale())
		{
			CYGC_LOG(Warning, TEXT("Receiver '%s' owner destroyed without stopping the stream; stopping it"), *Receivers[i]->GetConfig().ReceiverName);
			DestroyReceiver(Receivers[i]);
		}
	}

	if (ViewportCapture.IsValid() && ViewportCapture->HasSenders())
	{
		ViewportCapture->Tick();
	}

	// Copy the arrays: a tick callback may destroy a stream
	TArray<FSenderStreamPtr> SendersCopy = Senders;
	for (const FSenderStreamPtr& Stream : SendersCopy)
	{
		Stream->Tick(DeltaTime);
	}
	TArray<FReceiverStreamPtr> ReceiversCopy = Receivers;
	for (const FReceiverStreamPtr& Stream : ReceiversCopy)
	{
		Stream->Tick(DeltaTime);
	}

	if (FPlatformTime::Seconds() >= NextDiscoveryTime && (Receivers.Num() > 0 || CachedSenders.Num() > 0))
	{
		RefreshAvailableSenders();
	}

	if (bDebugOverlay)
	{
		DrawDebugOverlay();
	}
	return true;
}

void UCyGameCaptureSubsystem::OnWorldCleanup(UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
	if (World == nullptr)
	{
		return;
	}
	// Components stop their streams in EndPlay; this catches everything else (streams created from C++ with a world owner)
	for (int32 i = Senders.Num() - 1; i >= 0; --i)
	{
		UObject* Owner = Senders[i]->Owner.Get();
		if (Owner != nullptr && Owner->GetWorld() == World)
		{
			CYGC_LOG(Log, TEXT("World cleanup: stopping sender '%s'"), *Senders[i]->GetSenderName());
			DestroySender(Senders[i]);
		}
	}
	for (int32 i = Receivers.Num() - 1; i >= 0; --i)
	{
		UObject* Owner = Receivers[i]->Owner.Get();
		if (Owner != nullptr && Owner->GetWorld() == World)
		{
			CYGC_LOG(Log, TEXT("World cleanup: stopping receiver '%s'"), *Receivers[i]->GetConfig().ReceiverName);
			DestroyReceiver(Receivers[i]);
		}
	}
}

void UCyGameCaptureSubsystem::OnEnginePreExit()
{
	StopAllStreams();
	FlushRenderingCommands();
	CySpoutBackend::ShutdownShared();
}

void UCyGameCaptureSubsystem::DrawDebugOverlay()
{
	if (GEngine == nullptr)
	{
		return;
	}
	int32 Key = 0x43790000;
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Cyan, FString::Printf(TEXT("CyGameCaptureUE  [%s]  senders %d  receivers %d"), *GetRHIName(), Senders.Num(), Receivers.Num()));
	for (const FSenderStreamPtr& Stream : Senders)
	{
		const FCyGameCaptureStreamStats S = Stream->GetStats();
		GEngine->AddOnScreenDebugMessage(Key++, 0.0f, S.State == ECyGameCaptureStreamState::Error ? FColor::Red : FColor::Green,
			FString::Printf(TEXT("SEND  %-40s %dx%d %s  %.0f fps  drop %lld  %.2f ms"), *S.Name, S.Width, S.Height, *S.Format, S.FPS, S.DroppedFrames, S.CopyTimeMs));
	}
	for (const FReceiverStreamPtr& Stream : Receivers)
	{
		const FCyGameCaptureStreamStats S = Stream->GetStats();
		GEngine->AddOnScreenDebugMessage(Key++, 0.0f, S.bConnected ? FColor::Yellow : FColor::Orange,
			FString::Printf(TEXT("RECV  %-20s <- %-30s %dx%d %s  %.0f fps  %s"), *Stream->GetConfig().ReceiverName, *S.Name, S.Width, S.Height, *S.Format, S.FPS, S.bConnected ? TEXT("Connected") : TEXT("Waiting")));
	}
}
