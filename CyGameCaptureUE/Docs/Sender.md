# Sender — `UCyGameCaptureSpoutSenderComponent`

Publishes an Unreal image as a Spout2 sender. One component = one independent stream; add as many as you need.

## Properties

| Category | Property | Meaning |
|----------|----------|---------|
| Sender | **Stream Name** | short name used by the default naming convention `<Prefix>::<ProjectName>::<StreamName>` |
| Sender | **Use Custom Sender Name** / **Sender Name** | use `Sender Name` verbatim as the Spout name (e.g. `Beyond_View`) |
| Sender | **Name Collision Policy** | `Fail`, `Auto Rename` (`::2`, `::3`…), `Replace If Owned By This Process` (takes over a leftover of a previous PIE session, never another application's sender) |
| Source | **Source Mode** | `Render Target`, `Scene Capture`, `Game Viewport` |
| Source | **Render Target** | the `UTextureRenderTarget2D` to send (Render Target mode) |
| Source | **Scene Capture** | existing `USceneCaptureComponent2D` (Scene Capture mode); empty = the component creates and manages one |
| Source | **Scene Capture Size / Format** | size and `ETextureRenderTargetFormat` of the render target created for a managed scene capture |
| Timing | **Frame Rate Mode** / **Custom Frame Rate** | `Every Frame`, or at most N frames per second (skipped frames cost nothing on the GPU) |
| Behaviour | **Auto Start** | start on BeginPlay |
| Behaviour | **Auto Reconnect** | reserved for device-reset recovery |
| Advanced | **Flush After Copy** | D3D11: flush the immediate context after the copy (Spout's own behaviour; costs CPU, lowers latency by at most one frame) |
| Debug | **Debug** | per-stream verbose logging |

## Blueprint nodes (category *CyGameCapture | Sender*)

`Start Sender`, `Stop Sender`, `Is Sender Active`, `Set Sender Name`, `Set Render Target`, `Set Scene Capture`,
`Use Game Viewport`, `Get Active Sender Name`, `Get Sender Resolution`, `Get Sender Format`, `Get Sender FPS`,
`Get Sender Stats`, `Get Source Render Target`, `Get Active Scene Capture`.

Events: `On Started` (Spout name registered, first frame sent), `On Stopped`, `On Resolution Changed (Width, Height)`,
`On Error (Error)`.

## Supported source formats

Render targets / scene capture targets in `RTF_RGBA8`, `RTF_RGBA8_SRGB`, `RTF_RGB10A2`, `RTF_RGBA16f`,
`RTF_RGBA32f` (and `PF_R8G8B8A8` custom formats). The shared texture uses the matching typed DXGI format
(`B8G8R8A8_UNORM`, `R10G10B10A2_UNORM`, `R16G16B16A16_FLOAT`, …), so receivers get the exact bits. Other formats
(`RTF_R8`, `RTF_RG16f`, `PF_FloatR11G11B10`, …) raise `On Error` "Unsupported source pixel format".

## Colour space

* 8-bit targets with `bForceLinearGamma = false` (the default `RTF_RGBA8` for scene captures / Canvas) contain
  sRGB-encoded values: sent as-is, OBS and other tools display them correctly.
* Float targets are linear HDR: sent as `R16G16B16A16_FLOAT`; receivers that expect SDR will show them
  "washed" / clipped — use an 8-bit target or a post-process material for now (a GPU conversion pass is planned).
* The game viewport back buffer is already display-encoded (SDR: sRGB; HDR output: scRGB / PQ as configured).

## Lifetime

The component stops its stream in `EndPlay`, `OnUnregister` and when the world is torn down. If a component is
destroyed without EndPlay (editor deletion) the subsystem detects the dead owner on the next tick and stops the
stream. `CyGameCapture.ListSenders` shows what is currently registered by this process.

## C++

```cpp
FCySenderStreamConfig Config;
Config.SenderName = TEXT("Beyond_CameraA");
Config.FrameRateMode = ECyGameCaptureFrameRateMode::CustomFrameRate;
Config.CustomFrameRate = 60.f;
FString Error;
auto Stream = UCyGameCaptureSubsystem::Get()->CreateSender(Config, Owner, Error);
Stream->SetSourceRenderTarget(RenderTarget);
Stream->OnResolutionChanged.AddLambda([](FCySenderStream& S){ /* ... */ });
```
