# CyGameCapture — Architecture

Version 0.1.0 — D3D11 pipeline validated 2026-09-16; D3D12 (D3D11On12 bridge) and CyGameCaptureOBS validated 2026-09-17.

## 1. Global architecture

```
Game
 ↓ D3D11 / D3D12 calls
ReShade 6.8.0 (add-on build, API 20)           hooks the API, exposes reshade::api + events
 ↓ events
CyGameCaptureRS.addon64
 ├── ResourceTracker      which 2D targets exist, who writes them and when (per frame)
 ├── InspectorModel/UI    filters, selection, GPU preview, actions (Dear ImGui via ReShade's function table)
 ├── PreviewManager       selected resource → GPU copy → add-on owned texture → ImGui::Image
 ├── CaptureManager[]     selected resource → GPU copy → persistent shared texture → Spout sender (1..4 streams)
 └── View mode            selected resource → GPU copy → back buffer (player sees the clean image)
 ↓ shared DXGI texture handle + Spout shared-memory sender info
Spout2 receivers (OBS Spout2 plugin, CyGameCaptureSpoutReceiverTest, future CyGameCaptureOBS)
```

Nothing in the video path touches system memory: every step is a GPU copy or resolve recorded on the
game's own immediate context / presenting queue.

## 2. CyGameCaptureCore responsibilities

Header-only, dependency-free (`CyGameCaptureCore/Include/CyGameCaptureCore/`):

| Header | Content | Used by |
|--------|---------|---------|
| `Version.hpp` | version macros / constants (also consumed by the `.rc` resource) | RS today, OBS later |
| `SenderNaming.hpp` | `SanitizeGameName`, `MakeSenderName`, `IsCyGameCaptureSender`, the `CyGameCaptureRS::` prefix | RS (sender side), receiver test and OBS plugin (discovery side) |
| `StreamInfo.hpp` | `StreamKind` enumeration and the `StreamInfo` metadata record reserved for the future RS ↔ OBS IPC channel | reserved |

Only code that is really shared goes here (the user explicitly asked to avoid a premature abstraction layer).
Profile format, resource signatures and IPC structures will be added when they exist on both sides.

## 3. CyGameCaptureRS architecture

```
CyGameCaptureRS/Source
├── Addon/            AddonMain (exports, registration), Log, Config ([CYGAMECAPTURE] in ReShade.ini),
│                     DeviceContext (per device state + per-frame orchestration), CyGameCaptureRS.rc
├── ResourceTracker/  ResourceTracker (+ CommandListState), FormatUtils
├── BufferInspector/  InspectorModel (filters, selection, navigation; no UI code)
├── Preview/          PreviewManager
├── Capture/          SpoutSender (Spout bookkeeping), CaptureManager (one per stream)
├── Backends/         Backend (capability query per graphics API), D3D11/, D3D12/ (reserved)
└── UI/               Overlay (Dear ImGui)
ThirdParty/           reshade/include (SDK headers), imgui (header only), Spout2 (DirectX subset), licences
```

Per device (`DeviceContext`, attached with `device->set_private_data`): `ResourceTracker`, `PreviewManager`,
`std::vector<CaptureManager>` (stream slots), `InspectorModel`, view mode state, list of effect runtimes.
Per command list (`CommandListState`, private data too): bound render targets / depth-stencil and a compact list of
access events recorded while the game records commands.

### Frame flow (all in the `present` event, which ReShade fires *before* it renders effects and the overlay)

1. Merge the immediate command list state(s) into the tracker.
2. `ResourceTracker::EndFrame()`: statistics of the frame become "last frame", timeline is swapped, frame index++.
3. `AutoCapture` bootstrap (test / profile precursor), then `CaptureManager::BeginCopy` for every active stream
   (Spout mutex + GPU copy), optional single flush, `EndCopy` (mutex release + stats).
4. Hotkey polling + view mode: copy the selected buffer over the current back buffer.
5. Preview refresh for the inspector selection, only while the overlay showed the preview in the last frames.

## 4. ReShade callbacks used

Verified against `include/reshade_events.hpp` of ReShade 6.8.0 (API 20) and the official examples
(`09-depth`, `10-texture_overlay`, `11-obs_capture`). Key facts learned from the sources:

* `device::create_resource(desc, initial_data, initial_state, out, HANDLE *shared_handle)` with
  `resource_flags::shared` creates, in D3D11, a texture with `D3D11_RESOURCE_MISC_SHARED` and returns the legacy
  DXGI handle from `IDXGIResource::GetSharedHandle` — exactly what Spout receivers open with `OpenSharedResource`.
  In D3D12 only `shared_nt_handle` is supported (NT handles cannot be opened by legacy receivers).
* `ImTextureID` of the overlay is a `reshade::api::resource_view` handle (pushed via `push_descriptors`), so any
  shader resource view created through the API can be displayed with `ImGui::Image`.
* In D3D11 the immediate device context is both `command_queue` and `command_list`; private data is shared.
* `reshade_begin_effects` is only fired while effects are loaded, so it cannot be relied upon for frame work.
* ReShade calls the `reshade_overlay` event every frame as soon as an add-on registered it — even while its overlay is
  closed — so the open state must be tracked with `reshade_open_overlay`.
* Registered overlays (`register_overlay(title)`) are docked as tabs of the main ReShade window in 6.8.0.
* Mouse position for the overlay comes from `MSG.pt` (real cursor), keys from `WM_KEYDOWN` and friends.

| Event | Used for |
|-------|----------|
| `init_device` / `destroy_device` | create / destroy `DeviceContext` |
| `init_command_list` / `destroy_command_list` / `init_command_queue` / `destroy_command_queue` | `CommandListState` private data, queue list |
| `init_swapchain` / `destroy_swapchain` | back buffer identification, back buffer description |
| `init_effect_runtime` / `destroy_effect_runtime` | runtime pointers (hotkey polling) |
| `init_resource` / `destroy_resource` / `init_resource_view` / `destroy_resource_view` | tracked resources, view → resource map, destruction notifications |
| `bind_render_targets_and_depth_stencil`, `begin_render_pass`, `end_render_pass` | current bindings per command list (+ clear load ops) |
| `draw`, `draw_indexed`, `draw_or_dispatch_indirect`, `dispatch_mesh` | write events for bound targets |
| `clear_render_target_view`, `clear_depth_stencil_view`, `clear_unordered_access_view_uint/float` | clear events |
| `copy_resource`, `copy_texture_region`, `copy_buffer_to_texture`, `copy_texture_to_buffer`, `resolve_texture_region`, `generate_mipmaps` | copy / resolve reads and writes |
| `barrier` | compute write hint (transition away from `unordered_access`) — D3D12 / Vulkan |
| `reset_command_list`, `execute_command_list`, `execute_secondary_command_list` | state reset and merge (deferred contexts, D3D12 lists, bundles) |
| `present` | per-frame work described above |
| `reshade_overlay`, `reshade_open_overlay` | standalone inspector window |
| `register_overlay("CyGameCaptureRS")`, `register_overlay(nullptr)` | docked tab + settings section |

Not used on purpose: `push_descriptors` / `bind_descriptor_tables` (read tracking through descriptors is expensive;
reads are only counted for copies and resolves in V1).

## 5. Resource tracking strategy

* Tracked: `texture_2d` (and `surface`) resources whose usage includes `render_target`, `depth_stencil` or
  `unordered_access`, plus anything discovered lazily as a copy / resolve destination or created before the add-on
  loaded (registered from `device->get_resource_desc` at merge time, flagged `late_registered`).
* Each `TrackedResource` keeps: stable id (`#001`…), handle, full `resource_desc` (size, format, mips, layers,
  samples, usage, flags), created frame, last written / read frame, totals, `current` and `last` per-frame stats
  (writes, reads, draw calls, first / last write event index, last write kind, cleared flag), back buffer flag,
  debug name when available.
* Events are coalesced per command list (consecutive identical accesses become one entry with a count) and merged
  into the device tracker at execution / present time, where each gets a global event index (= position in the frame).
  A capped timeline (4096 entries) of the last frame feeds the Timeline tab.
* Resource lifetime: `destroy_resource` removes the entry and synchronously notifies listeners; capture streams
  referencing it go to **Capture Source Lost** and stop copying (the sender stays registered so OBS keeps its source).
  No GPU pointer is ever kept beyond a destruction notification; ImGui never receives a game-owned view.
* Threading: command list state is single-threaded by construction; the device map is protected by a
  `shared_mutex`; destruction listeners run outside the tracker lock.

## 6. Buffer preview strategy

* `PreviewManager` owns a default-heap texture (`shader_resource | copy_dest | resolve_dest`, typed non-sRGB
  variant of the source format) and its SRV; the selected resource is copied (or resolved when multisampled) into it
  at present time, only while the overlay displayed the preview during the last two frames.
* When the selected resource is also a capture stream source, the preview shows the capture output texture's SRV
  directly (no additional copy).
* **Freeze Preview** stops refreshing the add-on owned texture (if the preview was showing the capture output, one
  more copy is made first so the frozen image belongs to the preview texture).
* HDR / float formats are displayed raw (values above 1 clip); tone mapping for the preview is a later item.
  Depth and block-compressed formats are not previewed yet (milestone 3: depth visualisation shader).

### Contact sheet (`Preview/ThumbnailGrid`)

Clicking through forty render targets to find the one holding the scene was the slow part of choosing a buffer, so
the inspector also draws every listed buffer at once, 256x144, as a grid.

* One small `r8g8b8a8_unorm` render target per visible entry, filled by the same scaling blit the capture uses
  (`GpuPass/BlitPass`), on the presenting queue, at present time. Only the active rectangle is sampled, so a target
  left at full size by dynamic resolution shows its picture rather than its unused margin.
* Buffers that cannot be sampled directly (a back buffer, usually) go through `GpuPass/PassInput`, shared with the
  capture: a readable copy owned by the add-on, then the blit. Multisampled and block-compressed buffers get a text
  cell with the reason instead of an image.
* `kThumbnailBudget` (32) thumbnails are drawn per frame and the grid pages through longer lists. It is a GPU budget
  while the overlay is open, not a limit on what can be inspected.
* Nothing runs while the section is closed; after one second without it, the full-resolution readable copies are
  released and only the small thumbnails are kept.
* Every texture it creates is reported to `ResourceTracker::Forget`, like the capture's own, so none of them ever
  appears in the list or calls back into the tracker when destroyed.

### Steadiness

`TrackedResource::frames_written` counts the frames during which a resource received at least one write. Divided by
its age it gives the **Steady** column: 100 % for a target the engine fills every frame, a low value for one taken from
a transient pool for a frame or two. `IsPersistent` (at least `kPersistenceWindow` = 30 frames old and written during at
least half of them) backs the **Steady buffers only** filter, which is what stops the list from flickering: on Stray's
main menu, 30 of the 70 buffers written in a frame pass it. Back buffers are always kept (a flip chain alternates them,
so each one is written half of the time). **Written recently** keeps a buffer listed for 8 frames after its last write
for the same reason.

## 7. Capture strategy

* "Use As Capture Source" → `CaptureManager::Start`: validates the resource (2D, not depth, Spout compatible format),
  creates / updates the persistent output texture and the Spout sender, records the source handle + description.
* Every frame: source still tracked? description changed (dynamic resolution, format change) → output recreated and
  sender updated (`Capture texture recreated` / `Resolution changed` logs). Then one GPU copy (`copy_texture_region`
  subresource 0) or one resolve (MSAA) on the presenting queue.
* Multiple streams: `DeviceContext::captures` grows on demand, one `CaptureManager` per stream. Slot 1 is
  `CyGameCaptureRS::<Game>`, slot N is `CyGameCaptureRS::<Game>::N`; names are made unique across processes by
  incrementing the index. Each active slot costs one GPU copy per frame (two on D3D12). There is no product limit:
  `SpoutSender::CanRegisterNewSender` checks Spout's machine-wide name table (64 entries by default, shared with
  every Spout application, and `CreateSender` fails silently when it is full) and reports how many slots are in use,
  while `DeviceContext::kSanityStreamLimit` (256) only guards against a runaway loop.
* Capture FPS follows the game (one copy per presented frame). Rate limiting is a later option.
* View mode ("show selected buffer in game"): after the capture copies, the selected buffer is copied over the current
  back buffer (same size + copy-compatible format required for now). ReShade effects and overlay still render on top.

## 8. GPU copy strategy

| Case | Operation | Requirement |
|------|-----------|-------------|
| same size, copy-compatible formats (same typeless family, e.g. `RGBA8_SRGB` → `RGBA8`) | `copy_texture_region` / `CopySubresourceRegion` | — |
| multisampled source | `resolve_texture_region` into the single-sample output | typed format |
| different size or incompatible format | **not yet**: needs a fullscreen conversion pass (ReShade pipeline API) | milestone 2/3 |

Barriers are issued around each copy with a best-guess source state (`present` for back buffers,
`render_target`, `unordered_access` or `shader_resource` otherwise). They are no-ops in D3D11; D3D12 will track the real
state through `barrier` events (milestone 3).

## 9. Spout2 integration

Spout SDK subset compiled into the add-on: `SpoutSenderNames`, `SpoutSharedMemory`, `SpoutFrameCount`, `SpoutUtils`
(BSD 2-clause, see `ThirdParty/THIRD_PARTY_NOTICES.md`). `SpoutDX`, `SpoutDX12`, `SpoutDirectX`, `SpoutCopy` are vendored
for the receiver test and the future D3D12 bridge.

Facts verified in the Spout2 sources:

* A sender is a named shared-memory block (`SharedTextureInfo`: 32-bit share handle, width, height, DXGI format,
  description = exe path) plus an entry in the `SpoutSenderNames` set. `spoutSenderNames::CreateSender / UpdateSender /
  ReleaseSenderName` manage it. `spoutDX::SendTexture` is **not** used: it would copy into a Spout-owned texture.
* Receivers (`spoutDX::ReceiveTexture`, used by the OBS Spout2 plugin) read the info, `OpenSharedResource` the legacy
  handle on their own D3D11 device, then `CopyResource` into their texture while holding the named access mutex
  `<sender>_SpoutAccessMutex` (`spoutFrameCount::CheckAccess/AllowAccess`, 67 ms timeout).
* The frame counter semaphore (`<sender>_Count_Semaphore`) is only active when SpoutSettings enabled "Framecount"
  in the registry; without it receivers treat every read as a new frame.
* Handles are stored as 32 bits and sign-extended by receivers (`LongToHandle`), which works because legacy DXGI
  shared handles only carry 32 significant bits (validated with handles ≥ 0x80000000).

CyGameCaptureRS therefore: creates the shared texture through ReShade, registers name + handle + DXGI format
(ReShade's `format` values equal `DXGI_FORMAT`), takes the access mutex around each GPU copy, and optionally
flushes (`FlushAfterCopy`, off by default — measured at ~190 µs CPU per frame for no additional GPU ordering guarantee,
since Present follows immediately).

Formats sent without conversion: `R8G8B8A8_UNORM`, `B8G8R8A8_UNORM`, `R10G10B10A2_UNORM`, `R16G16B16A16_FLOAT`,
`R16G16B16A16_UNORM`, `R32G32B32A32_FLOAT` (typed non-sRGB variant of the source). Others (e.g. `R11G11B10_FLOAT`,
`B8G8R8X8`) report *Unsupported format* until the GPU conversion pass exists.

## 10. D3D11 backend

No native code: `reshade::api` covers resource creation with the shared flag, copies, resolves and views. Validated with
the D3D11 test application (scene → HDR RT → bloom → tonemap RT → back buffer + HUD): the tonemapped RT (`#005`) is
streamed without the HUD while the window shows the HUD, keyboard navigation, view mode and two simultaneous streams work,
and `CyGameCaptureSpoutReceiverTest` receives changing frames from both senders.

Measured on the test app (RTX 3090, 1280×720 RGBA8, 144 fps): capture cost per stream ≈ 12–15 µs CPU per frame
(of which ~3 µs mutex wait); with `FlushAfterCopy=1` ≈ 195 µs. GPU cost is a single 1280×720 copy per stream.

## 11. D3D12 backend — D3D11On12 bridge (implemented)

Tracking, inspector and preview run on D3D12 through `reshade::api` alone. Spout output needs one extra hop, because
a D3D12 resource can only produce an **NT** shared handle while Spout receivers open **legacy** DXGI handles. The
bridge, `Source/Backends/D3D12/D3D11On12Bridge.{hpp,cpp}`, is a process-wide singleton shared by every stream:

1. `D3D11On12CreateDevice` on the game's own `ID3D12Device` and **presenting** `ID3D12CommandQueue`. The queue is
   captured in `DeviceContext::OnPresent` and pushed to every `CaptureManager` (`SetPresentQueue`) before any capture
   can start, so the bridge is never built on a queue that is not known yet.
2. The persistent shared texture is created on that D3D11 device with `D3D11_RESOURCE_MISC_SHARED`, and
   `IDXGIResource::GetSharedHandle` gives the legacy handle Spout publishes.
3. Each stream also owns a plain D3D12 intermediate texture. The game's queue copies the selected resource into it
   (`copy_texture_region`, exactly as in D3D11), and the intermediate is wrapped once with `CreateWrappedResource`.
4. After the copy has been submitted, `AcquireWrappedResources` → `CopyResource` → `ReleaseWrappedResources` →
   `Flush` moves the picture into the shared texture. Two GPU copies per stream per frame instead of one; still no
   CPU readback anywhere.

**Ordering.** No fence is needed. ReShade's D3D12 `flush_immediate_command_list()` calls `ExecuteCommandLists`
synchronously on the calling thread, and the 11on12 context submits to the *same* queue, so the bridge copy is
always queued behind the copy that fills the intermediate. (This is what makes the ReShade case simpler than the
Unreal one, where the D3D12 RHI submits from another thread and a real fence is required — see
`CyGameCaptureUE/Docs/`.) `CaptureManager::NeedsQueueFlush()` asks for the flush on the frames that need it.

**Lifetime.** The wrapped resource and the shared texture are referenced by GPU work that may still be in flight, so
`DestroyOutput()` calls `command_queue::wait_idle()` before releasing them; releasing them straight away caused a
device removal. The bridge itself holds references to the game's device and queue, so it is released from the
`destroy_device` event (`ShutdownForDevice`), not at add-on unload, and `destroy_command_queue` clears every stored
queue pointer so `wait_idle` is never called on a dead queue.

**DXGI debug layer.** When Graphics Tools is installed and something in the process created a debug DXGI factory,
`dxgi.dll` raises a breakpoint on the error message that `D3D11On12CreateDevice` produces inside a hooked process.
With no debugger attached that breakpoint kills the game. The bridge therefore suppresses DXGI break-on-severity and
neutralises that one breakpoint **for the duration of its own call only** (`DxgiDebugBreakScope`), logging whatever
the debug layer said. Nothing else in the process is affected.

Alternative kept for later: CyGameCaptureOBS opening NT handles directly (`OpenSharedResource1`) with a shared
fence, removing the 11on12 hop for our own receiver while keeping the legacy handle for third-party Spout apps.

## 11b. GPU passes: derived streams (implemented)

Until the depth and HUD streams, the add-on only ever issued copies. Producing a stream that does not
exist as a buffer needs real shader work, so `Source/GpuPass/` adds a small, deliberately narrow pass
system: one fullscreen pixel shader, N input textures, one render target, no vertex buffer.

* `FullscreenPass` compiles HLSL to **DXBC** at run time with `D3DCompile` (`vs_5_0` / `ps_5_0`, which
  both Direct3D back ends of ReShade accept) and builds the pipeline through `reshade::api` only:
  `create_pipeline_layout` (param 0 = the input SRVs, param 1 = one linear clamp sampler, param 2 = the
  push constants), `create_sampler`, `create_pipeline` with the vertex/pixel shader, blend, rasterizer
  (cull none), depth-stencil (off), topology, render target format, sample mask/count and viewport
  count sub-objects. Drawing is `begin_render_pass` → `bind_pipeline` → viewport/scissor →
  `push_descriptors` ×2 → `push_constants` → `draw(3, 1, 0, 0)` → `end_render_pass`.
  The geometry is the usual fullscreen triangle generated from `SV_VertexID`.
* **Inputs that cannot be sampled.** A swap-chain back buffer is normally created for rendering only,
  with no `shader_resource` usage, and no view can be made on it — which is exactly the buffer the HUD
  pass needs as its "finished image". `CaptureManager::PassInput` handles that: when the resource lacks
  `shader_resource`, the frame is copied into a texture the stream owns and the pass samples that
  instead. One extra copy, and only for the buffers that need it. The same path covers depth buffers
  created without the flag, which is the usual case in D3D12.

`StreamTransform` says what a stream does between the selected resource and the shared texture:

| Transform | Output format | What it does |
|-----------|---------------|--------------|
| `Copy` | the source format, folded to a typed non-sRGB variant | one GPU copy, bit for bit (the original path) |
| `DepthLinear` | `R16G16B16A16_UNORM` | linearises a depth buffer (`GpuPass/DepthPass`) |
| `HudDifference` | `R8G8B8A8_UNORM` | finished image minus the scene before the interface, mask in alpha (`GpuPass/HudPass`) |

**Depth.** A depth buffer is neither shareable nor linear, and the convention is game-specific: reversed
Z, logarithmic distribution, upside down, and near/far planes that simply cannot be read from outside.
The pass therefore uses exactly ReShade's own linearisation (`RESHADE_DEPTH_*`), so settings already
worked out for a game with a ReShade depth shader carry over unchanged, and exposes them in the Capture
tab and in `ReShade.ini`. The result goes to 16 bits per channel — the precision a depth-based
reprojection needs — with R, G and B carrying the same value so it still reads as a grey-scale image.

**HUD.** In most games the interface is not a buffer: it is composited onto the image, so what exists is
"the scene before" and "the finished image". Since `final = hud * a + clean * (1 - a)` is one equation
with two unknowns per pixel, `hud` and `a` cannot both be recovered. The pass produces what an editor
actually wants instead: the finished image as the colour and a mask in alpha where the two buffers
disagree, built from the largest per-channel difference (many interface elements are coloured overlays
that barely move the luminance). Exact wherever the interface is opaque, an approximation in soft edges
and translucent panels, which is why the threshold and softness are exposed. When a game *does* render
its interface into its own render target with an alpha channel, select that buffer directly: a plain
copy is exact and cheaper.

Validated on both back ends: the HUD mask and the linearised depth come out identical on D3D11 and
D3D12 from the test applications.

### Dynamic resolution

A resource description says how big a texture is, not how much of it holds the current frame. Engines
that scale their resolution at run time almost never reallocate their render targets: they keep them at
the maximum size and render into a sub-rectangle, then upscale on the way to the back buffer. Nothing in
the resource changes — only the viewport of the draws does.

Capturing such a target whole would hand a receiver a picture with a stale margin that grows and shrinks
several times a second. Capturing only the active rectangle would make the Spout sender change size just
as often, which every receiver would have to chase.

* The tracker records the viewport of each draw (`bind_viewports`) and clamps it to the resource, giving
  `TrackedResource::active_rect`. Only draws carry it: a clear or a copy says nothing about coverage.
* `CaptureManager` turns it into a `SourceRect` (a texture-coordinate scale and offset) and **scales the
  active area into an output of stable size**, exactly as the game does for its own back buffer. The
  stream keeps one size for as long as the game keeps one resolution.
* A plain copy therefore becomes a blit through `GpuPass/BlitPass` as soon as the source is only partly
  rendered into; it stays a copy the rest of the time.
* The depth and HUD passes take the rectangle in their constants, so they sample the right region. The
  HUD pass takes **one rectangle per input**: with dynamic resolution the scene is routinely smaller
  than the finished image, so the two buffers no longer have to be the same size — only the same aspect
  ratio.
* The overlay shows the rendered area next to the stream resolution, and the log says so once on
  entering and once on leaving, never per frame.

One consequence worth knowing: an isolated HUD compares the finished image against the scene it was
built from, and with dynamic resolution the finished image is an *upscale* of that scene. Resampling
never reproduces the original exactly at high-contrast edges, so a little of the scene leaks into the
mask. Raising `HudThreshold` absorbs it — measured on the test application, 0.02 leaves faint traces of
the scene's thin bright bars and 0.06 removes them while leaving the interface untouched (mask mean
18.2/255 in both cases, identical to the same scene without dynamic resolution).

### Selecting "the back buffer"

A swap chain owns several back buffers and rotates through them, so the one the inspector lists as
`#001` only holds the finished frame every second or third present. Capturing that one resource mixes
current and stale frames. It is invisible on a plain colour copy -- one frame of lag on a moving picture
looks like a moving picture -- and obvious on a HUD stream, where the finished image is compared against
the scene of the *current* frame: everything that moved between the two frames lands in the mask.

It showed up exactly that way on the D3D12 test application: the rotating bars of the scene appeared in
the HUD mask as pairs of ghost rays, one per frame position, and the mask's mean alpha read 19.4/255
instead of 18.2. D3D11 did not show it, because a D3D11 `DXGI_SWAP_EFFECT_DISCARD` swap chain exposes a
single back-buffer texture while a D3D12 flip chain hands out one resource per buffer.

So `DeviceContext::OnPresent` publishes `swapchain->get_current_back_buffer()` to every stream, and
`CaptureManager::ResolveSource` substitutes it whenever the selected resource is a back buffer -- for the
primary source and for the HUD stream's second one alike. Every back buffer of a swap chain shares one
description, so nothing else changes. After the fix the D3D12 mask is identical to the D3D11 one
(alpha mean 18.2 in both) and its recording shrank from 5.3 MB to 1.3 MB, the difference being the ghost
rays FFV1 had been dutifully encoding.

## 11c. Moments of the frame (`Capture/FrameSnapshots`)

Everything else reads buffers in the `present` event, at the end of the frame. `FrameSnapshots` reads a buffer *after
its K-th write* instead, which is the only way to get a clean picture out of an engine that composites its interface
into the buffer holding the tonemapped image (Unreal Engine 4: measured on Stray, the back buffer is cleared, receives
one tonemapper draw, then the interface draws, then present).

* **Write notification.** `CommandListState` knows its command list and tracker. `CommandListState::Add` is called by
  every draw, clear, copy and resolve hook, and ReShade calls those hooks *before* forwarding the call. `Add` reports
  each write to `ResourceTracker::NotifyWrite` first thing, before its own coalescing, so every single write is seen.
  The notification is gated by an atomic flag owned by `FrameSnapshots` and set only while a snapshot is in use: the
  draw path pays one relaxed load otherwise.
* **The copy.** `FrameSnapshots::OnWrite` counts the writes of each requested buffer; when write K+1 is about to happen
  it records `copy_resource(buffer -> owned texture)` on the same command list, so it executes first. The owned texture
  has the buffer's size, mips, layers and typeless format family, one sample, `copy_dest | copy_source | shader_resource`.
  A frame with K writes or fewer is copied at its end in `EndFrame`, so the slider's last position means "end of frame".
* **Readers.** `Proxy(ticket)` returns a `TrackedResource` with the buffer's id, typed format and active area but the
  owned texture's handle and usage. The capture, the preview and the view mode take it in place of the buffer, so every
  existing path (format conversion, dynamic resolution, depth and HUD passes) works on it unchanged. The HUD pass can
  therefore compare one buffer with itself at two moments.
* **Sharing and lifetime.** Requests are reference counted per (buffer, moment) and identified by tickets, so a late
  release after the buffer was destroyed (and its handle possibly reused) cannot touch a newer request. A destroyed
  buffer drops its snapshots immediately; their users see the source lost through the tracker as usual.
* **Scope.** D3D11 immediate context only: there, recording order is execution order. Writes on deferred contexts are
  ignored rather than miscounted. D3D12 would need per-command-list counting ordered at submission (not done).
* **Settings.** Inspector: the *Moment of the frame* slider (1 .. writes of the last frame), with the buffer's write
  order from the timeline under it. Config: `AutoCapture=#5@2` (`@K` on an id entry); a `:hud` entry inherits the first
  stream's moment as the scene's moment.

## 11d. AI assistant (`Assistant/AiAssistant` + `CyGameCaptureAI.exe`)

A discussion window in the overlay, whose automatic first message is the button "find the setting that removes the
game's interface". The AI proposes, the user applies.

* **Conversation.** `AiAssistant` keeps the messages (user, assistant, errors). Every message starts a helper run with
  the conversation so far (last 12 messages, 4000 bytes each): the clients keep no session, so the same approach works
  with all three. A message may carry fresh pictures of the frame ("Attach the current frame"); without them, the
  request still carries the current buffer list and write order. A reply may end with a proposal; the overlay turns it
  into Show / Apply / Apply-with-interface buttons under that reply, and the inspector's section keeps the latest one
  at hand. The discussion window opens by itself when a message or a reply arrives.

* **In the game (`AiAssistant`, D3D11).** A message only sets a flag; the work is done at present time. Plan: the current
  back buffer after each of its writes of the last frame (up to 7 moments, the first three always, the rest spread),
  plus the back buffer at the end of the frame, plus up to 8 steady full-size colour buffers at the end of the frame.
  The moments are `FrameSnapshots` tickets held for one frame. Render: each picture goes through the scaling blit
  (active area only) into one 640-wide tile render target, copied into a shared `r8g8b8a8` atlas
  (`resource_flags::shared`); the immediate context is flushed, and the helper is started two frames later with
  `CreateProcess` (no window, below normal priority). The request (`request.json`) carries the atlas share handle and
  layout, the tiles' labels, the steady buffers of a quarter of the screen or more, and the frame's write order.
  Progress and answer come back as `status.txt` / `result.txt` key=value files, polled from the overlay every 0.4 s
  while a search runs; the run id includes the game's process id, so files from an earlier launch are never taken for
  the current one. The atlas is released as soon as the helper reports it has read the pictures.
* **Out of the game (`CyGameCaptureAI.exe`).** Opens the atlas on its own D3D11 device (like any Spout receiver),
  copies it to a staging texture, writes one opaque PNG per tile with WIC. Finds the client like CyAICodex's
  `local_clients.py` (PATH, `%LOCALAPPDATA%\OpenAI\Codex\bin\*\codex.exe`, `~\.local\bin`, npm packages including
  their bundled `.exe`, node + script as the last resort, never a shell) and runs it like CyAICodex's `assist.py` /
  `connectors.py`: Claude Code `-p` with stream-json in and out (`--verbose`), no tools, `--strict-mcp-config` with
  no server, no setting sources, no session persistence, the pictures as base64 image blocks; Codex `exec`
  `--ignore-user-config --ephemeral --sandbox read-only` with `--image`; OpenCode `run --pure --format json` with
  `--file` and every permission denied through `OPENCODE_CONFIG_CONTENT`. The client runs in a job object (a timeout,
  or the game closing, ends it with everything it started) and its stderr is discarded. The answer must end with a
  JSON object `{found, buffer, moment, hud_stream}` when it proposes a setting (always for the automatic first
  message); the helper checks the buffer belongs to the frame and that the moment exists, removes the object and any
  emphasis markers from the text (`reply.txt`, shown as is), and writes the proposal to `result.txt`.
* **Why not MCP.** A single round trip carrying all the evidence is enough for this question and works identically
  with every client; an MCP server would add a bridge process and a protocol for no gain here.
* **Measured.** Test application: `#005` at the end of the frame (the clean scene buffer). Stray, title screen:
  `#005` right after write 2 (the tonemapper), the setting found by hand. Black loading screen: `found: false`, with
  the advice to show a scene with the interface visible.

## 12. Resource lifetime

Handled cases: resource destroyed (capture → *Source Lost*, preview cleared), swap chain resize (back buffers
re-registered, dependent captures lost), source size / format change (output recreated, sender updated), device
destruction (all GPU objects released in `DeviceContext` destructor before the device goes away), add-on unload
(`AddonUninit` unregisters everything).

Not yet: automatic re-selection after a loss (needs the profile / signature system of milestone 2).

**Destruction reported from inside our own calls.** D3D11 destroys a released resource late, during whichever runtime
call comes next, and ReShade reports that destruction right there, synchronously, on that thread. Any GPU call the
add-on makes while holding one of its own locks can therefore bring the destruction of an unrelated game texture back
into a `destroy_resource` listener of the same object, on the same thread: re-taking a `std::mutex` throws
`std::system_error`, which takes the game down (0xe06d7363; seen twice in Stray). Every object that makes GPU calls
under a lock and listens to destructions guards against it: `CaptureManager` ignores a destruction reported by the
thread holding its lock (the next frame's tracker lookup reports the loss), and `FrameSnapshots` notes it and handles
it when its lock is released (`FrameSnapshots::Guard`).

## 13. Synchronisation

* Copies are recorded on the presenting queue's immediate command list at `present` time: after the game finished
  recording its frame and before ReShade's own work, so the source content is final for that frame.
* D3D11: implicit ordering on the immediate context; barriers are no-ops.
* Spout side: named access mutex around the copy submission (CPU side, like Spout's sender). There is no cross-process
  GPU fence in the Spout protocol; the receiver copies whatever the shared texture contains when it runs.
* No CPU stalls, no queue flush (unless `FlushAfterCopy` is enabled), no readbacks.
* D3D12 (later): copies must not be recorded inside a render pass; multi-queue resources need state tracking.

## 14. Format conversion

A buffer in a format Spout receivers open is copied bitwise (typeless / sRGB variants folded to the typed non-sRGB
format). Any other format is converted on the GPU by the scaling blit, into the nearest format that does not lose range
(`ToPublishableFormat`): `r16g16b16a16_float` for anything floating point or wider than 8 bits per channel
(Unreal's `R11G11B10_FLOAT` scene colour, for example), `r8g8b8a8_unorm` otherwise. Multisampled sources in such a
format are still refused, since they would need a resolve before the conversion. Still planned: HDR → SDR tonemap,
gamma / colour space options, and a depth visualisation for the preview.

## 15. Profile system (milestone 2)

Per executable ini (separate from `ReShade.ini`) storing renderer, sender name and a **capture resource signature**:
size relative to the back buffer, format, sample count, usage, creation order, first / last write event position,
write pattern (draw calls, cleared), relationship to the back buffer. On start-up: search matching tracked resources,
validate, resume capture; otherwise *Saved Capture Source Not Found* and manual selection — never a silent guess.
Double / triple buffered targets are matched as a class (identical signatures alternating frames).
`AutoCapture` (size + format) is the current minimal precursor used by the tests.

## 16. CyGameCaptureOBS (implemented)

Native OBS source plugin, "Add Source → CyGameCapture" (`CyGameCaptureOBS/`, see
[its README](../CyGameCaptureOBS/README.md)). It links libobs' public C API only and needs no OBS build tree: the
headers of the target release are vendored and the import library is generated from the installed `obs.dll` by
`Tools/GenerateObsImportLib.cmd`.

* Discovery: Spout's machine-wide name table, with `IsCyGameCaptureSender` putting `CyGameCaptureRS::*` and
  `CyGameCaptureUE::*` first and `SenderOriginName` telling ReShade from Unreal.
* Transport: `gs_texture_open_shared()` on the legacy handle Spout published, then `obs_source_draw()`. The picture
  is never copied by the plugin.
* The source must **not** declare `OBS_SOURCE_CUSTOM_DRAW`: with that flag OBS calls `video_render` outside any
  effect pass and `obs_source_draw()` finds no active effect and draws nothing (the source renders black). Without
  it, OBS wraps the callback in its default "Draw" technique, which is what is needed here.
* Lifetime: the published description is re-read every tick; any change of handle, size or format reopens the
  texture, and a sender that disappears becomes *Capture source lost* rather than a frozen last frame. A different
  sender is never picked silently unless "Reconnect automatically" is on.

Recording: the plugin can record several buffers at once, each into its own file, using one libobs video mix per
buffer (`obs_view_add2`, native resolution and its own colour format), its own encoder and its own `ffmpeg_muxer`
output. All enabled buffers start inside one call so the files begin on the same tick, and the frame-sync channel
below is read every tick to report the drift between them. The engine (`Source/BufferRecorder.*`) uses libobs only,
never the OBS front-end API, so a headless CyGameCaptureRec linking libobs can reuse it unchanged.

Lossless recording uses OBS' raw `ffmpeg_output` with FFV1 instead of an encoder plus a muxer, pointed at
the stream's own mix with `obs_output_set_media`. Two modes: a `BGRA` mix with `gpu_conversion` off,
which is bit exact, and a `P416` mix, which is the only way to make libobs render into `RGBA16F` rather
than 8-bit BGRA. Measured on a 16-bit depth stream spanning 0.0257-0.9994: NVENC HEVC returns 214
distinct levels in limited range, lossless BGRA 249 linear levels, lossless 16-bit 4945 levels (~12.3
bits) with an invertible sRGB curve. libobs has no 16-bit *unorm* mix, so the full 16 bits cannot reach
a file through OBS at all - a consumer that needs them reads the Spout stream.

Alpha: the HUD stream keeps its mask in the alpha channel, and only the `BGRA` lossless mode has one to
keep it in - the hardware encoder and the 16-bit mix are both YUV. OBS clears a mix to transparent and
composites onto it, so the recorded RGB comes out premultiplied by the mask, which is what a compositing
tool wants. Verified by extracting the alpha back out of a recording with `CyGameCaptureVideoProbe
--dump 3`: identical to the live stream, mean 18.2/255 in both. A take warns in the OBS log when the
chosen encoding would drop the mask.

Data streams (depth) are drawn with `gs_set_linear_srgb(false)`: this source declares `OBS_SOURCE_SRGB`,
which is right for colour (decode on read, encode on write, net identity) but wrong for a 16-bit unorm
data texture, where no sRGB view exists to decode with and only the encode happens, gamma-curving every
value. The stream kind comes from the frame-sync channel, with the DXGI format as a fallback.

Constraints found in libobs 29: `ffmpeg_muxer` is an audio+video output and `can_begin_data_capture` refuses to
start one without an audio encoder, so every file carries an audio track; and a source must not declare
`OBS_SOURCE_CUSTOM_DRAW`, or `obs_source_draw()` runs outside any effect pass and draws nothing.

Later: the IPC metadata channel (`StreamInfo`: game name, exe, resolution, FPS, colour format, available streams,
version) so the OBS UI can show more than Spout carries, and direct NT handle / shared fence transport for D3D12.

## 16b. Frame-sync channel (CyGameCaptureCore/FrameSync.hpp)

Spout carries a texture and nothing else, so two senders updated in the same game frame look, to a
receiver, like two unrelated streams. That is fine for a preview and not acceptable for recording, where
the clean scene and the HUD (or the colour and the depth) have to line up frame for frame.

Producers therefore publish, next to each Spout sender, a small shared-memory record holding the index of
the frame last copied into that shared texture, the group it belongs to (process + device), a QPC time
stamp and the geometry. `DeviceContext::OnPresent` increments one index per presented frame and every
stream of that present publishes it, so all the streams of a frame carry the same number.

* Named section `Local\CyGameCapture_FrameSync_v1`, 256 slots, claimed by Spout sender name so no other
  agreement between producer and recorder is needed. Slots of dead processes are reclaimed.
* Each slot is a seqlock (`sequence` odd while writing, even when readable), so a reader detects a torn
  read and skips that sample. No kernel object is touched on the producer side: this runs in the present
  callback of a game.
* No pixels ever go through it — a few dozen bytes per stream per frame.

CyGameCaptureOBS reads it every tick while recording and reports the worst gap between the streams of a
take ("drift"). A foreign Spout sender publishes nothing and simply cannot be checked.

## 16b-2. Interface language (`UI/Localization`)

English is the interface's language in the code: every text shown by the overlay is written in English where it is
used and wrapped in `TR()`. `i18n::Tr` looks the English text up in the table of the current language
(`UI/Translations_fr.inl`, `{ "English", "French" }` pairs, turned into a hash map on first use) and returns the English
text itself when there is no entry, so an untranslated text never breaks anything. Format strings are translated as a
whole and keep their conversions in the same order. Lists (combo items) are built every frame from `TR()` so a change
of language shows at once; window titles are `"<title>###<id>"` so ImGui keeps their place and size across languages.
Texts produced elsewhere (capture states, the assistant's progress, snapshot errors) are translated at display time
when the table knows them. The language is `[CYGAMECAPTURE] Language=en|fr`, switched from a selector next to the
logo. The AI assistant's replies follow it unless `AILanguage` says otherwise, and `CyGameCaptureAI.exe` writes its
progress and error messages in the language of the request; the prompt itself stays in English.

## 16c. Branding

`Resources/Branding` holds the logo (`CyGameCapture_Logo.png`, the master), the Windows icon and two smaller PNGs,
all derived from the master by `Tools/GenerateBranding.py` and committed. The `CyGameCaptureBranding` CMake target is
header-only: linking it adds the folder to the include path, which is how each binary's resource script finds
`CyGameCaptureIcon.rc2` and the images. The icon is embedded in every binary (`IDI_CYGAMECAPTURE`, the first icon
resource, so Explorer uses it); the test applications also set it on their window class.

The add-on additionally embeds the 256 px logo as `RCDATA` and draws it at the top of the inspector window and of its
settings panel (`UI/Logo`). The PNG is decoded once per process with WIC, which is part of Windows, so no image
decoder is vendored; COM is initialised for the call only if the thread was not already in an apartment. Each device
then gets an ordinary `shader_resource` texture made from those pixels, drawn with `ImGui::Image` like the preview.
This is static interface artwork uploaded once: nothing ever reads a GPU resource back.

## 17. Known limitations (0.1.0)

* D3D12 Spout output goes through the D3D11On12 bridge: two GPU copies per stream per frame instead of one.
* Reading a buffer at a moment of the frame is D3D11 only, and refuses multisampled buffers; so is the AI assistant.
* A multisampled buffer in a format outside the Spout list is refused (converting it would need a resolve first).
* The HUD stream needs both buffers at the same size, and multisampled depth is refused (it would need a resolve first).
* A pass is recorded in the `present` callback like the copies, so it shares their timing: a transient
  target reused later in the same frame is seen after its reuse.
* A HUD stream also picks up anything else that is in the finished image but not in the clean scene -
  ReShade's own FPS counter, for instance, when it is enabled.
* View mode needs back-buffer size and copy-compatible format.
* Read tracking is limited to copies / resolves; write tracking does not see compute writes on D3D11 (no barriers).
* Preview and capture use mip 0 / slice 0 of array or mipmapped resources.
* Profiles / automatic re-selection not implemented; the `AutoCapture` config key is a test hook.
* ReShade's ImGui docking makes the registered tab hard to find; the standalone window is the primary UI.
* Capture timing is "end of frame": a transient target reused later in the same frame would be captured after its reuse
  (an "after last write" mode using the tracked write events is a planned option).
