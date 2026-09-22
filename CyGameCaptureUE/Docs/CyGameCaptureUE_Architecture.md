# CyGameCaptureUE — Architecture

Multi-stream Spout2 sender / receiver plugin for Unreal Engine 4.27 → 5.8, Win64, D3D11 and D3D12.
Part of the CyGameCapture ecosystem (CyGameCaptureRS = ReShade add-on, CyGameCaptureOBS = OBS plugin,
CyGameCaptureCore = shared conventions). Nothing in the plugin depends on OBS: any Spout2 application
(OBS + Spout2 plugin, Resolume, TouchDesigner, another Unreal instance, CyGameCaptureRS, custom tools) is a peer.

```
                              CyGameCaptureUE (one runtime module)
                 ┌────────────────────────────────────────────────────────────┐
  RenderTarget ──┤ UCyGameCaptureSpoutSenderComponent ─┐                       │
  SceneCapture ──┤   (managed capture + RT)            ├─► FCySenderStream ──► ICySenderBackend ──► shared texture ──► Spout name
  GameViewport ──┤   (Slate back buffer hook)         ─┘        │                 D3D11 / D3D12(11on12)
                 │                                              │
                 │ UCyGameCaptureSubsystem (UEngineSubsystem) ◄─┘  registry, ticking, discovery, cleanup, console
                 │                                              │
  Spout sender ──┤ UCyGameCaptureSpoutReceiverComponent ──► FCyReceiverStream ──► ICyReceiverBackend ──► UTextureRenderTarget2D
                 └────────────────────────────────────────────────────────────┘
```

## 1. Spout2 integration

Facts verified in the Spout2 SDK sources (vendored subset in `Source/ThirdParty/Spout2`, BSD-2):

* A *sender* is a name in the shared-memory sender set plus a 280-byte `SharedTextureInfo` block per sender:
  32-bit share handle, width, height, DXGI format, owner executable path. Receivers read it with
  `spoutSenderNames::getSharedInfo`, sign-extend the handle (`LongToHandle`) and call
  `ID3D11Device::OpenSharedResource` on their own D3D11 device: **only legacy DXGI shared handles
  (`D3D11_RESOURCE_MISC_SHARED`) are usable by the Spout ecosystem**, NT handles are not.
* Every texture update is bracketed by the named mutex `<sender>_SpoutAccessMutex`
  (`spoutFrameCount::CheckAccess`, 67 ms timeout / `AllowAccess`). The frame counter semaphore is optional
  (only when SpoutSettings enabled "Framecount" in the registry).
* There is no cross-process GPU fence in the Spout protocol: a receiver copies what the shared texture
  contains when its copy executes.
* Several senders per process are fine: each is an independent shared-memory block + texture.

CyGameCaptureUE compiles `SpoutUtils`, `SpoutSharedMemory`, `SpoutSenderNames` and `SpoutFrameCount` straight
into the runtime module (`Private/ThirdPartyWrappers/*.wrap.cpp` include the SDK `.cpp` files between the engine's
`AllowWindowsPlatformTypes` / `HideWindowsPlatformTypes` guards, with the Windows API macros the engine undefines
restored locally). No prebuilt library, no DLL, no absolute path: `RunUAT BuildPlugin` works from any location.
`spoutDX` / `spoutDX12` are *not* used: they own their own devices and copies; the plugin performs the GPU work
itself on the engine device and only delegates the CPU bookkeeping (`FCySpoutSenderRegistration`).

## 2. Unreal RHI architecture (version compatibility)

Only public and stable entry points are used (`Private/RHI/CyRHICompat.h`):

| Need | 4.27 / 5.0 | 5.1 → 5.8 |
|------|------------|-----------|
| Which RHI runs | `GDynamicRHI->GetName()` | `RHIGetInterfaceType()` |
| Native device / graphics queue | `GDynamicRHI->RHIGetNativeDevice()` / `RHIGetNativeGraphicsQueue()` | same |
| Native resource of an RHI texture | `FRHITexture::GetNativeResource()` | same |
| Create an intermediate texture | `RHICreateTexture2D(...)` | `RHICreateTexture(FRHITextureCreateDesc)` |
| Copy / barriers | `RHICmdList.CopyTexture`, `Transition(FRHITransitionInfo)`, `EnqueueLambda` | same |
| Texture type of the back buffer delegate | `FTexture2DRHIRef` (≤ 5.4) | `FTextureRHIRef` (5.5–5.7), `ISlateViewportProvider&` (5.8) |
| D3D12 fence after a submission | `SubmitCommandsHint()` + native `ID3D12CommandQueue::Signal` (synchronous submission) | `ID3D12DynamicRHI::RHISignalManualFence` inside an RHI execution lambda |
| Ticker | `FTicker` | `FTSTicker` |

Rules followed everywhere:

* UObjects (render targets, scene captures, components) are only touched on the game thread. Render commands
  receive `FTextureRenderTargetResource*` / `FRHITexture*` for the frame (engine pattern), and capture a
  thread-safe `TSharedRef` of the stream so the stream cannot die while GPU work references it.
* Backends and Spout registrations are destroyed by an RHI-thread lambda queued *after* the last copy
  (`FCySenderStream::Stop`): no `FlushRenderingCommands` per frame, none at stop either. The only flushes are
  on receiver (re)connection (a rare event) and when the viewport hook is unbound.
* The RHI thread executes native D3D11 calls through `RHICmdList.EnqueueLambda` (same immediate context as the
  engine, in order with the engine's own commands).

## 3. D3D11 path

* **Sender**: the shared texture is created natively on the engine's `ID3D11Device` with
  `D3D11_RESOURCE_MISC_SHARED` (typed format, `BIND_RENDER_TARGET | BIND_SHADER_RESOURCE`), the legacy handle comes
  from `IDXGIResource::GetSharedHandle`. Per frame: `RHICmdList.Transition(source → CopySrc)`, RHI lambda
  `{ Spout mutex; CopyResource(shared ← sourceNative); optional Flush; release mutex }`, `Transition(→ SRVMask
  or Present)`. **One GPU copy, no intermediate.**
* **Receiver**: `OpenSharedResource(handle)` on the engine device (render thread, on connect), per frame RHI lambda
  `{ mutex; CopyResource(targetNative ← shared) }`. One GPU copy.
* Formats are typed (BGRA8, RGBA8, RGB10A2, RGBA16F, RGBA16, RGBA32F); render targets use the typeless family so
  `CopyResource` between e.g. `B8G8R8A8_TYPELESS` (RT) and `B8G8R8A8_UNORM` (shared) is legal.

## 4. D3D12 path (D3D11On12 bridge)

A process-wide `D3D11On12CreateDevice` on the engine's `ID3D12Device` + graphics queue (`FCyD3D11On12Bridge`,
immediate context serialized by a critical section). Same mechanism as Spout's own `spoutDX12` class.

* **Sender**: ring of 3 intermediate RHI textures (state `CopySrc`) each wrapped once with
  `CreateWrappedResource(COPY_SOURCE → COPY_SOURCE)`. Per frame on the render thread:
  `Transition(slot → CopyDest)`, `CopyTexture(source → slot)`, `Transition(→ CopySrc)`, fence signal (see table).
  A worker thread waits for the fence (never the render thread), then `{ mutex; Acquire; CopyResource(shared ←
  wrapped slot); Release; Flush }`. If a slot is still busy the frame is dropped instead of stalling.
  Cost: 2 GPU copies per frame (engine copy + bridge copy); the fence wait adds up to one frame of latency.
* **Receiver**: shared texture opened on the bridge's D3D11 device; one intermediate RHI texture (`CopyDest`) wrapped
  `COPY_DEST → COPY_DEST`. Per frame: RHI lambda `{ mutex; Acquire; CopyResource(wrapped ← shared); Release; Flush }`
  (executes on the queue immediately), then `Transition(intermediate → CopySrc)`, `CopyTexture(intermediate →
  target)`, `Transition(→ CopyDest)`. The engine's payload is submitted after the bridge flush, so the single queue
  keeps the order.
* Why the fence on the sender side: in 5.1+ the D3D12 RHI submits command lists from a dedicated submission
  thread, so an `EnqueueLambda` on the RHI thread can run *before* the engine copy is actually submitted. The manual
  fence (`RHISignalManualFence`) is part of the engine's payload and therefore signals after the copy executed.
  On 4.27 `SubmitCommandsHint` submits synchronously and a native `Signal` from the RHI thread is enough.

## 5. Sender lifecycle

`StartSender()` → resolve the name (§9) → `UCyGameCaptureSubsystem::CreateSender` → `FCySenderStream::Start`
(backend + registration objects created, state *Waiting*) → first frame with a valid source: shared texture
created, Spout name registered (`OnStarted`), state *Active* → every frame: copy → source size/format change:
shared texture re-created, Spout info updated (`OnResolutionChanged`) → `StopSender()` / `EndPlay` /
`OnUnregister` / world cleanup / engine pre-exit: name released, GPU objects deleted on the RHI thread
(`OnStopped`). Errors (unsupported format, device failure) put the stream in *Error* and fire `OnError` once.

Ghost prevention: the subsystem stops streams whose owner object was garbage collected, and stops every stream
of a world being cleaned up (PIE stop) and at `OnEnginePreExit`.

## 6. Receiver lifecycle

`StartReceiver()` → state *Waiting* → polled every `ReceiverPollIntervalSeconds` (0.25 s): sender info found →
backend opens the handle (render thread) → `OnConnected` → target render target resized / re-formatted by the
component (`InitCustomFormat`, sRGB for 8-bit, linear for float) → every frame one copy → sender changed size /
format / handle → reopen + `OnResolutionChanged` / `OnFormatChanged` → sender gone → `OnDisconnected`, keeps
polling (auto reconnect) → `StopReceiver()`.

`OnFrameReceived` is throttled by `FrameEventRateHz` (0 = off) so a 120 Hz stream never floods the game thread.

## 7. Viewport capture hook

`FSlateRenderer::OnBackBufferReadyToPresent` (public since 4.x) fires on the render thread for every window
right before Present with the final swap-chain texture: **game scene + HUD + UMG + Slate**, exactly what the
player sees. `FCyViewportCapture` binds it when the first GameViewport sender starts, matches the window with
`GEngine->GameViewport->GetWindow()` (refreshed every tick, so PIE windows work) and feeds every registered
sender with `FCySenderStream::SendTexture_RenderThread`. No second scene render: one GPU copy (D3D11) or the
bridge path (D3D12). The back buffer is typically `R10G10B10A2_UNORM` on UE5 (HDR-capable swap chain) or
`B8G8R8A8_UNORM`; both are Spout-compatible. Pre-UI capture would require engine changes and is out of scope.

## 8. Render target path

`UTextureRenderTarget2D` → `GameThread_GetRenderTargetResource()` (game thread) → render command →
`GetRenderTargetTexture()` (render thread) → backend copy. The stream never keeps a raw pointer to the UObject
(`TWeakObjectPtr`), the component re-assigns the source every tick, and a missing / not yet initialised target
just leaves the stream in *Waiting*. Supported formats: `RTF_RGBA8`, `RTF_RGBA8_SRGB`, `RTF_RGB10A2`,
`RTF_RGBA16f`, `RTF_RGBA32f` (and PF_R8G8B8A8 custom formats). `RTF_R8`, `RTF_RG16f`, ... are rejected with an error.

## 9. Scene capture path

`SourceMode = SceneCapture`: an existing `USceneCaptureComponent2D` is used as-is (its `TextureTarget` is the
source; one is created when missing), otherwise the component creates a managed capture attached to the owner
(`bCaptureEveryFrame`, `SCS_FinalColorLDR`, size / format from the component). The capture keeps every native
option (FOV, show flags, post-process, projection). The component ticks in `TG_PostUpdateWork`, after scene
captures updated, so the frame sent is the one rendered this frame.

## 10. GPU synchronisation summary

| | Sender | Receiver |
|--|--|--|
| D3D11 | RHI thread, native copy in engine order, mutex around the copy | idem |
| D3D12 | engine copy → fence → worker → bridge copy; ring of 3 slots, drop when busy | bridge copy (immediate) → engine copy (later payload) |
| Cross process | Spout access mutex only (protocol limitation) | idem |

No `FlushRenderingCommands`, no `WaitForGPU`, no readback anywhere in the plugin (the host test harness and the
automation test read pixels back *only to verify* results).

## 11. Format conversions and colour space

V1 performs bitwise copies only. The receiver creates its target in the sender's format family, so an OBS BGRA8
stream lands in a `PF_B8G8R8A8` sRGB render target (sampled linear by materials, correct colours), a float sender
in a linear `PF_FloatRGBA` target. Sender side, 8-bit sRGB render targets are sent as-is (the encoded bytes are what
OBS expects), float targets are sent linear. Planned (milestone 4): GPU conversion pass (R11G11B10 / R8 / RG16F sources,
linear ↔ sRGB, HDR → SDR tone mapping, alpha handling modes) through a fullscreen RHI pass — never on the CPU.

## 12. Multi-stream design

* Streams are independent objects; each sender owns its shared texture, Spout name, registration and (D3D12)
  worker; each receiver its opened texture and intermediate. Starting / stopping one never touches the others.
* Frame-rate limiter per sender (`EveryFrame` / `CustomFrameRate`): skipped frames issue no GPU work at all.
* Sender names: `<Prefix>::<ProjectName>::<StreamName>` by default, fully customisable. Collision policy:
  `Fail`, `AutoRename` (`::2`, `::3`…), `ReplaceIfOwnedByThisProcess` (takes over leftovers of a previous PIE
  session — Spout stores the owner executable — but never a sender of another application).
* Validated: 4 senders (2 render targets, 1 scene capture, 1 game viewport) + 1 loop-back receiver at 120–175 fps
  on D3D11 and D3D12 without dropped frames (host harness `-CyGCTest`), external reception with the SpoutDX based
  test receiver, 4 senders + 4 receivers create/destroy in the automation tests.

## 13. Known limitations

* Win64 only; Vulkan / OpenGL RHIs report "unsupported" (start the game with `-dx11` or `-dx12`).
* Receiver target must be copy-compatible with the sender: with `bResizeTargetAutomatically = false` and a
  mismatching user target nothing is copied (the stats show 0 fps); no scaling / conversion yet.
* D3D12 costs one extra GPU copy and up to one frame of latency compared with D3D11.
* Viewport capture takes the whole window back buffer: in PIE with "Play in Viewport" that is the editor window;
  use "New Editor Window (PIE)" or Standalone for a clean game image.
* GPU copy time is not measured (would need timestamp queries with readback); `CopyTimeMs` is the CPU cost of
  issuing the copy.
* 5.0, 5.1 and 5.2 are supported by the code paths but were not compiled (not installed); 4.27, 5.3–5.8 are built
  with `RunUAT BuildPlugin` (see `Docs/Performance.md` / build notes).
