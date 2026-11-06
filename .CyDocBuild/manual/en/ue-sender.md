---
id: ue-sender
title: Unreal sender: properties and API
description: Configure the sender component, inspect public calls and respond to lifecycle events.
section: Unreal Engine
order: 20
parent: unreal
revision: 4aaf4ab41141
---

## Configure the component {#properties}
Include `CyGameCaptureSpoutSenderComponent.h` and depend on the `CyGameCaptureUE` module in your game's Build.cs. The component is available in Blueprint as CyGameCapture Spout Sender.

| Property | Behaviour |
| --- | --- |
| StreamName | Short name for the default naming convention. |
| bUseCustomSenderName, SenderName | Use an exact custom Spout name. |
| NameCollisionPolicy | Fail, AutoRename, or ReplaceIfOwnedByThisProcess. Replacement never claims another process's sender. |
| SourceMode | RenderTarget, SceneCapture or GameViewport. |
| RenderTarget, SceneCapture | Existing source objects; an empty SceneCapture allows a managed camera. |
| SceneCaptureSize, SceneCaptureFormat | Size and format for a managed scene target. |
| FrameRateMode, CustomFrameRate | Every frame or a maximum custom rate. |
| bAutoStart | Start on BeginPlay. |
| bFlushAfterCopy | Submit D3D11 copies immediately; increases CPU work. |
| bDebug | Verbose stream messages. |

The sender's exposed bAutoReconnect property does not implement device-reset recovery in the current component. After a device reset, check the error and recreate/restart the source instead of relying on that flag.

## Public calls {#functions}
| Signature | Result and behaviour |
| --- | --- |
| `bool StartSender()` | Starts the configured source; false on setup failure. Observe OnError. |
| `void StopSender()` | Releases this component's stream. |
| `bool IsSenderActive() const` | Whether the component has an active sender. |
| `void SetSenderName(const FString& NewSenderName)` | Sets a full custom name; restarts an active sender. |
| `void SetRenderTarget(UTextureRenderTarget2D* NewRenderTarget)` | Selects RenderTarget mode and the supplied target. |
| `void SetSceneCapture(USceneCaptureComponent2D* NewSceneCapture)` | Selects SceneCapture mode. |
| `void UseGameViewport()` | Selects the final back buffer. |
| `FString GetActiveSenderName() const` | Actual published name, empty before startup. |
| `FIntPoint GetSenderResolution() const` | Current width and height. |
| `FString GetSenderFormat() const` | Current format label. |
| `float GetSenderFPS() const` | Stream frame rate. |
| `FCyGameCaptureStreamStats GetSenderStats() const` | Current stream statistics. |
| `UTextureRenderTarget2D* GetSourceRenderTarget() const` | Source target when one is used. |
| `USceneCaptureComponent2D* GetActiveSceneCapture() const` | Existing or managed capture component. |

## Events and lifetime {#events}
OnStarted, OnStopped, OnResolutionChanged(Width, Height) and OnError(Error) report state changes. EndPlay, unregistration and owner/world cleanup stop the stream. Use [the subsystem API](doc:ue-subsystem) when managing streams directly in C++.

```cpp
#include "CyGameCaptureSpoutSenderComponent.h"
// Sender and Target refer to live objects owned by your actor.
Sender->SetRenderTarget(Target);
if (Sender->StartSender()) {
    const FString PublishedName = Sender->GetActiveSenderName();
}
```

RGBA8, RGBA8 sRGB, RGB10A2, RGBA16f and RGBA32f targets are supported. Float targets contain linear HDR values and need a compatible receiver. Unsupported formats produce an error; changing a file name or format label does not convert colours.
