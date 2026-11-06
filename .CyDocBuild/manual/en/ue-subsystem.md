---
id: ue-subsystem
title: C++ subsystem and Blueprint helpers
description: Discover senders, own streams safely and configure the engine subsystem.
section: Unreal Engine
order: 20
parent: unreal
revision: 7465db725128
---

## Includes and modules {#includes}
Depend on CyGameCaptureUE. Include `CyGameCaptureSubsystem.h` for direct stream ownership and `CyGameCaptureBlueprintLibrary.h` for Blueprint-style discovery helpers. The engine subsystem owns process-wide stream registrations; components are the usual owners of individual streams.

## Discover and inspect {#discovery}
`UCyGameCaptureSubsystem::Get()` returns the subsystem or null before engine initialisation. IsSpoutSupported(), GetRHIName() and GetUnsupportedReason() explain availability. GetAvailableSpoutSenders(bForceRefresh) lists producer information; FindSpoutSender(SenderName, OutInfo) returns whether an exact sender exists. MakeDefaultSenderName(StreamName) applies project naming. GetAllStreamStats(Senders, Receivers) fills both arrays. StopAllStreams() stops all streams in this process; avoid using it when only one component should stop.

The static UCyGameCaptureBlueprintLibrary helpers expose GetCyGameCaptureSubsystem(), IsSpoutSupported(), GetAvailableSpoutSenders(), GetAvailableSpoutSenderNames(), FindSpoutSender(), MakeDefaultSenderName() and GetAllStreamStats().

## Own a stream directly {#lifetime}
CreateSender(Config, Owner, OutError) and CreateReceiver(Config, Owner, OutError) return thread-safe shared stream pointers. Check the returned pointer and OutError before using it. Retain the pointer and pair it with DestroySender(Stream) or DestroyReceiver(Stream). Never dereference a failed creation result. ResolveSenderName(RequestedName, Policy, OutFinalName, OutError) checks collisions before startup.

```cpp
#include "CyGameCaptureSubsystem.h"
if (UCyGameCaptureSubsystem* Capture = UCyGameCaptureSubsystem::Get()) {
    const TArray<FCySpoutSenderInfo> Senders = Capture->GetAvailableSpoutSenders(true);
}
```

## Project settings {#settings}
Project Settings → Plugins → CyGameCapture controls bEnableSpout, bAllowEditorCapture, default sender prefix, sender limits, receiver polling and debug output. These settings do not make Vulkan compatible with Spout. SetDebugOverlayEnabled(bool) changes the on-screen diagnostic overlay; it does not change capture content.
