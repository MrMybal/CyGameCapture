# Multi-stream

Every sender and every receiver is an independent object. Starting, stopping, resizing or losing one never
affects the others.

## How many streams can run at once

There is no artificial cap in the plugin. Two real ceilings apply:

- **Senders**: Spout publishes sender names in a shared-memory table that is *machine wide* — every Spout
  application on the computer shares it. It holds **64 entries by default**, and `CreateSender` fails silently
  once it is full, so `CySpout::CanRegisterNewSender` checks it first and reports how many entries are in use.
  The size can be raised in the registry under
  `HKCU\Software\Leading Edge\Spout\MaxSenders` (Spout's own SpoutSettings tool writes this key).
- **Receivers**: they take no entry in that table, so the only limits are GPU memory and bandwidth.

Each active stream costs one GPU copy per frame (two on D3D12, through the D3D11On12 bridge), which is what
actually determines how many streams a given machine can sustain.

`StreamSanityLimit` in the project settings (256 by default) is only a guard against a runaway Blueprint loop
creating streams forever; raise it freely if a setup genuinely needs more.

## Typical setup

```
UNREAL
├── Sender "Viewport"   Game Viewport ─────────────► Spout "CyGameCaptureUE::Beyond::Viewport"
├── Sender "CameraA"    SceneCapture A ────────────► Spout "CyGameCaptureUE::Beyond::CameraA"
├── Sender "CameraB"    SceneCapture B ────────────► Spout "CyGameCaptureUE::Beyond::CameraB"
├── Sender "Debug"      RenderTarget ──────────────► Spout "CyGameCaptureUE::Beyond::Debug"
│
├── Receiver "Program"  ◄── Spout "OBS_Program"    ──► RenderTarget ──► TV screen material
├── Receiver "CamExt"   ◄── Spout "OBS_Camera"     ──► RenderTarget
├── Receiver "VJ"       ◄── Spout "Resolume"       ──► RenderTarget
└── Receiver "Other"    ◄── Spout "CyGameCaptureRS::Beyond"
```

Add one component per stream (any actor, any number of actors) or create them from C++ with
`UCyGameCaptureSubsystem::CreateSender` / `CreateReceiver`.

## Cost

Each **active** sender costs, per sent frame:

| RHI | GPU | CPU (render / RHI thread) |
|-----|-----|---------------------------|
| D3D11 | 1 copy (source → shared texture) | ~2–8 µs to issue the copy |
| D3D12 | 2 copies (source → intermediate, intermediate → shared) | ~2–8 µs + a worker thread doing the fence wait |

Each **connected** receiver costs one copy per frame (D3D11) or two (D3D12), same order of CPU cost. A receiver
whose sender does not exist costs nothing but a shared-memory lookup every 0.25 s.

Measured on the host harness (RTX 3090, UE 5.5): 4 senders (1280×720 BGRA8, 640×360 RGBA16F, 960×540 BGRA8,
1280×720 RGB10A2) + 1 loop-back receiver ran at **175 fps in D3D11** and **117 fps in D3D12** with 0–9 dropped
frames over ~2800 frames.

## Frame-rate budgeting

Use **Frame Rate Mode = Custom Frame Rate** on the streams that do not need the full game frame rate:

```
Game            = 120 fps
Viewport sender = 60 fps      (OBS records at 60)
CameraA sender  = 60 fps
Debug sender    = 10 fps
```

A skipped frame does **no** GPU work at all: the limiter runs before anything is issued.

## Names and collisions

Default: `<Prefix>::<ProjectName>::<StreamName>` (prefix from the project settings). Two components with the
same `Stream Name` collide; the **Name Collision Policy** decides:

* `Auto Rename` (default) → the second becomes `...::2`.
* `Fail` → the second refuses to start and fires `On Error`.
* `Replace If Owned By This Process` → takes over a name left behind by a previous PIE session of the same
  executable (Spout stores the owner executable path), but never a sender of another application.

Receivers are matched by exact name, so pick the policy that fits your workflow: `Auto Rename` is safest for
tools, a fixed custom name (`Beyond_View`) is best when OBS scenes are already configured.

## Several Unreal instances

Two Unreal instances of the same project produce the same default names, so the second one auto-renames to
`...::2`. To make them deterministic, set an explicit **Sender Name** per instance (for example from a command
line argument read in Blueprint/C++ at startup).

## Inspecting everything at runtime

* `CyGameCapture.Debug 1` — on-screen list of every sender and receiver with resolution, format, fps and drops.
* `CyGameCapture.ListSenders` / `CyGameCapture.ListReceivers` — console output with the same data.
* `CyGameCapture.Spout.List` — every Spout sender on the machine, including other applications.
* Blueprint: `Get All Stream Stats` returns two arrays of `FCyGameCaptureStreamStats`.
