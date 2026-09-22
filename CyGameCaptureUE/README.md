# CyGameCaptureUE

**Multi-stream Spout2 sender & receiver for Unreal Engine.** Part of the
[CyGameCapture](../README.md) ecosystem (CyGameCaptureRS = ReShade add-on, CyGameCaptureOBS = OBS plugin).

```
UNREAL                                                        ANY SPOUT APPLICATION
├── Sender  Game Viewport  ──► "CyGameCaptureUE::Proj::View"  ──►  OBS / Resolume / TouchDesigner
├── Sender  SceneCapture A ──► "...::CameraA"                 ──►  another Unreal instance
├── Sender  RenderTarget   ──► "...::Debug"                   ──►  custom tools
└── Receiver  ◄── "OBS_Program" ──► UTextureRenderTarget2D ──► material / UMG / Composure
```

GPU-first (no readback, no `FlushRenderingCommands` per frame), no engine modification, Blueprint and C++ APIs,
several senders and receivers at the same time. OBS is a use case, never a dependency.

| | |
|---|---|
| Platform | Win64 |
| RHI | D3D11 (native shared texture), D3D12 (D3D11On12 bridge) |
| Engine | UE 4.27 → 5.8 (single code base, compile-time shims) |
| Third party | Spout2 SDK subset compiled into the module (BSD-2), no DLL, no absolute paths |
| Licence | [MIT](LICENSE) |

## Quick start

1. Copy this folder into `<YourProject>/Plugins/CyGameCaptureUE` (a release archive extracted into `<YourProject>/Plugins`
   gives exactly that), enable the plugin, run with D3D11 or D3D12. The release archives hold the editor binaries of
   one engine version; the project compiles the plugin from `Source` for its game targets, as for any code plugin.
2. Add a **CyGameCapture Spout Sender** component, pick a *Source Mode*, press Play.
3. In OBS: *Add Source → Spout2 Capture → the sender name*.

Full walkthrough: [Docs/GettingStarted.md](Docs/GettingStarted.md).

## Documentation

| File | Content |
|------|---------|
| [Docs/CyGameCaptureUE_Architecture.md](Docs/CyGameCaptureUE_Architecture.md) | Spout2 integration, RHI compatibility, D3D11 / D3D12 paths, lifecycles, synchronisation, multi-stream design, limitations |
| [Docs/GettingStarted.md](Docs/GettingStarted.md) | install, first sender, first receiver, console commands, editor vs standalone |
| [Docs/Sender.md](Docs/Sender.md) | sender component: properties, nodes, events, formats, colour space |
| [Docs/Receiver.md](Docs/Receiver.md) | receiver component, material and UMG usage, resolution changes |
| [Docs/MultiStream.md](Docs/MultiStream.md) | several senders / receivers, costs, naming, collisions |
| [Docs/ViewportCapture.md](Docs/ViewportCapture.md) | game viewport hook, what the image contains, PIE vs standalone |
| [Docs/SceneCapture.md](Docs/SceneCapture.md) | managed and user scene captures, capture sources, cost |
| [Docs/RenderTarget.md](Docs/RenderTarget.md) | render target sources, formats, recipes, alpha |
| [Docs/OBS.md](Docs/OBS.md) | Unreal → OBS and OBS → Unreal |
| [Docs/Performance.md](Docs/Performance.md) | design rules, measurements, benchmark procedure |
| [Docs/Troubleshooting.md](Docs/Troubleshooting.md) | symptoms → causes |

## Project settings

Project Settings → Plugins → **CyGameCaptureUE**: enable Spout, default sender prefix, auto reconnect, debug
logging, D3D11 / D3D12 backends, editor capture, discovery and poll intervals, flush after copy, max streams.

## Console

```
CyGameCapture.ListSenders     CyGameCapture.ListReceivers
CyGameCapture.Spout.List      CyGameCapture.Debug 1
```

## Tests

`Automation RunTests CyGameCapture` (editor or game with a real RHI):

* `CyGameCapture.Naming.DefaultSenderName`
* `CyGameCapture.Naming.CollisionPolicy`
* `CyGameCapture.GPU.Loopback` — render target → sender → Spout → receiver → render target, with a pixel check
* `CyGameCapture.Streams.CreateDestroyMany` — 4 senders + 4 receivers created and destroyed

The host project `CyGameCaptureUEHost` reaches this plugin through a junction, so the plugin exists once in the
repository. After cloning, create it from the repository root (`cmd`):

```
mklink /J CyGameCaptureUEHost\Plugins\CyGameCaptureUE CyGameCaptureUE
```

Manual harness (in `CyGameCaptureUEHost`): `-CyGCTest -CyGCTestSeconds=25` spawns 4 senders + receivers and logs
a report every 2 seconds.

## Verified

| Engine | Build | Runtime test |
|--------|-------|--------------|
| 5.5 | host project (Build.bat) | D3D11 and D3D12: 4 senders + 1 receiver, external Spout reception, automation tests pass |
| 5.3, 5.8 | `RunUAT BuildPlugin` ✔ | release archive extracted into a blank project: the editor loads the plugin, naming tests pass (`-nullrhi`) |
| 5.4, 5.6, 5.7 | `RunUAT BuildPlugin` ✔ | not run |
| 4.27 | see below | not run |
| 5.0, 5.1, 5.2 | not installed on the dev machine | — |

The release archives (`Tools\MakeRelease.cmd`) hold the `RunUAT BuildPlugin` output of 5.3 to 5.8, without
`Intermediate` and symbol files. 5.8 reports `RHICreateTexture` with an implied immediate command list as deprecated:
it still builds and works, and will need the command-list overload before the engine removes it.

Release packages are built with `RunUAT BuildPlugin -Plugin=<repo>\CyGameCaptureUE\CyGameCaptureUE.uplugin -Package=<folder outside the repository> -TargetPlatforms=Win64 -Rocket -NoHostProject`: the package folder ends up in the binaries' debug information and source paths, so it is kept short and neutral (for example `C:\CyGCBuild\UE_5.5`) rather than inside a personal folder.

The 4.27 installation available here is missing the Win64 editor generated headers
(`Engine/Intermediate/Build/Win64/UE4Editor/Inc`), so no plugin can be compiled with it on this machine. Every
4.27 API the plugin uses was verified against the 4.27 sources (`RHICreateTexture2D`, `FRHIResourceCreateInfo(const TCHAR*)`,
`GetNativeResource`, `RHIGetNativeDevice` / `RHIGetNativeGraphicsQueue`, `EnqueueLambda`, `FRHITransitionInfo`,
`ERHIAccess::CopySrc/CopyDest/SRVMask`, `TexCreate_Shared`, `OnBackBufferReadyToPresent(SWindow&, const FTexture2DRHIRef&)`,
`SubmitCommandsHint`, `FTicker`, `UEngineSubsystem`, `DeveloperSettings`), but a real 4.27 compile is still pending.
