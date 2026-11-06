---
id: ue-receiver
title: Unreal receiver: properties and API
description: Connect an exact sender name and display its texture through a dynamic material.
section: Unreal Engine
order: 20
parent: unreal
revision: a6962ad4778c
---

## Connect a sender {#properties}
Include `CyGameCaptureSpoutReceiverComponent.h` from module `CyGameCaptureUE`. Set SenderName to the exact published name. ReceiverName is only the local display name. bAutoConnect starts at BeginPlay; bAutoReconnect keeps looking when the producer disappears.

OutputMode chooses InternalRenderTarget or UserRenderTarget. The internal target automatically matches the producer. User mode uses TargetRenderTarget and bResizeTargetAutomatically. FrameEventRateHz throttles OnFrameReceived; zero disables this event without stopping GPU reception.

## Public calls {#functions}
| Signature | Result and behaviour |
| --- | --- |
| `bool StartReceiver()` | Starts reception; a missing producer can still be awaited with automatic reconnect. |
| `void StopReceiver()` | Releases the connection. |
| `bool IsReceiverActive() const` | Whether reception is running. |
| `bool IsConnected() const` | Whether a producer is connected. |
| `void SetSenderName(const FString& NewSenderName)` | Changes producer and reconnects an active receiver. |
| `void SetTargetRenderTarget(UTextureRenderTarget2D* NewTarget)` | Selects an output target. |
| `UTextureRenderTarget2D* GetReceiverTexture() const` | Output target to use in a material; check for null. |
| `FIntPoint GetReceiverResolution() const` | Current size. |
| `FString GetReceiverFormat() const` | Current format label. |
| `float GetReceiverFPS() const` | Reception rate. |
| `FCyGameCaptureStreamStats GetReceiverStats() const` | Reception statistics. |

## Display the texture {#material}
Create a material with a Texture Sample Parameter 2D called SpoutTexture. OnConnected obtains Get Receiver Texture and assigns it through Set Texture Parameter Value on a dynamic material instance. For UMG, use a material with Material Domain = User Interface on the Image widget's brush. Do not pass a render target to Set Brush from Texture, whose texture input is not a render-target input.

```cpp
#include "CyGameCaptureSpoutReceiverComponent.h"
if (UTextureRenderTarget2D* Texture = Receiver->GetReceiverTexture()) {
    MaterialInstance->SetTextureParameterValue(TEXT("SpoutTexture"), Texture);
}
```

## Events and changes {#events}
OnConnected, OnDisconnected, OnFrameReceived, OnResolutionChanged, OnFormatChanged and OnError describe the receiver state. The subsystem polls sender information at ReceiverPollIntervalSeconds. With automatic resizing disabled, an incompatible target can leave the previous image frozen. Keep both applications on the same GPU adapter and use D3D11/D3D12.
