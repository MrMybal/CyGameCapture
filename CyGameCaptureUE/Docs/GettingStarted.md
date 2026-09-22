# CyGameCaptureUE — Getting started

CyGameCaptureUE streams Unreal images to any Spout2 application (OBS, Resolume, TouchDesigner, another
Unreal instance, custom tools) and receives Spout2 streams into Unreal render targets. Everything stays on
the GPU. Windows, D3D11 or D3D12, UE 4.27 → 5.8.

## Install

1. Copy the `CyGameCaptureUE` folder into `<YourProject>/Plugins/` (or package it with
   `RunUAT BuildPlugin -Plugin=.../CyGameCaptureUE.uplugin -Package=<out> -TargetPlatforms=Win64` and drop the
   result into `Engine/Plugins/Marketplace` or your project).
2. Enable it: Edit → Plugins → Rendering → **CyGameCaptureUE** (a C++ project is not required: the plugin is
   built by the editor / UBT like any code plugin; Blueprint-only projects work once the plugin binaries exist).
3. Make sure the game runs with D3D11 or D3D12 (Project Settings → Platforms → Windows → Default RHI, or `-dx11` / `-dx12`).
   Vulkan is not supported by Spout.
4. Optional: Project Settings → Plugins → **CyGameCaptureUE** (sender prefix, debug logging, backends, editor capture).

## Send something in 30 seconds (Blueprint)

1. Add a **CyGameCapture Spout Sender** component to any actor.
2. Set **Source Mode**:
   * *Render Target* + a `UTextureRenderTarget2D` asset,
   * *Scene Capture* (leave *Scene Capture* empty: the component creates and manages one at the actor location), or
   * *Game Viewport* (what the player sees, no second render).
3. Keep **Auto Start** on. Play. A Spout sender named `CyGameCaptureUE::<ProjectName>::<StreamName>` appears.
4. In OBS: *Add Source → Spout2 Capture → select the sender* (see `OBS.md`).

## Receive something in 30 seconds (Blueprint)

1. Add a **CyGameCapture Spout Receiver** component.
2. Type the **Sender Name** (use the *Get Available Spout Sender Names* node to list them).
3. Keep **Output Mode = Internal Render Target** and **Auto Connect** on.
4. On `OnConnected` (or any time later): *Get Receiver Texture* → *Set Texture Parameter Value* on a dynamic
   material instance, or use it in a UMG Image brush (see `Receiver.md`).

## C++

```cpp
#include "CyGameCaptureSubsystem.h"

UCyGameCaptureSubsystem* Capture = UCyGameCaptureSubsystem::Get();

FCySenderStreamConfig Config;
Config.SenderName = Capture->MakeDefaultSenderName(TEXT("Camera01"));   // or any custom name
FString Error;
TSharedPtr<FCySenderStream, ESPMode::ThreadSafe> Sender = Capture->CreateSender(Config, this, Error);
Sender->SetSourceRenderTarget(MyRenderTarget);      // every frame is sent from now on
...
Capture->DestroySender(Sender);
```

Receivers: `FCyReceiverStreamConfig` + `CreateReceiver`, then `SetTargetRenderTarget`. Statistics for every
stream: `GetAllStreamStats`. The components are thin wrappers over the same API.

## Console commands

| Command | Effect |
|---------|--------|
| `CyGameCapture.ListSenders` | senders of this process with resolution / format / fps / dropped frames |
| `CyGameCapture.ListReceivers` | receivers with connection state |
| `CyGameCapture.Spout.List` | every Spout sender on the machine (any application) |
| `CyGameCapture.Debug 1` | on-screen overlay with all streams |

## Editor, PIE, Standalone, Shipping

| Context | Senders / receivers |
|---------|--------------------|
| PIE (Play in Viewport) | work; *Game Viewport* sends the **whole editor window** (that is the window back buffer) |
| PIE (New Editor Window) | work; *Game Viewport* sends the PIE window |
| Standalone game / packaged Development / Shipping | work; *Game Viewport* sends the game window |
| Editor world (no play) | components only start on BeginPlay; use the C++ API for editor tools. `bAllowEditorCapture` can disable everything in the editor |

Streams are stopped automatically when PIE ends, the world is torn down or the engine exits, so no Spout
sender is left behind.
