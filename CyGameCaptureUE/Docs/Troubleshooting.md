# Troubleshooting

Start with the log (`LogCyGameCapture`) and the console commands:

```
CyGameCapture.ListSenders      streams published by this process
CyGameCapture.ListReceivers    streams consumed by this process
CyGameCapture.Spout.List       every Spout sender on the machine
CyGameCapture.Debug 1          on-screen overlay
```

## "Spout: unavailable"

The subsystem logs the reason at startup:

| Reason | Fix |
|--------|-----|
| `Spout texture sharing needs D3D11 or D3D12 (current RHI: Vulkan)` | start with `-dx11` / `-dx12`, or change Project Settings → Platforms → Windows → Default RHI |
| `Spout disabled in Project Settings` | Project Settings → Plugins → CyGameCaptureUE → **Enable Spout** |
| `D3D12 backend disabled in project settings` | enable **Enable D3D12** (same page) |
| `Spout2 is only available on Windows (Win64) builds` | expected on other platforms; the components do nothing |
| `RHI not initialized yet` | transient, the check is redone on the next call |

## The sender does not start

* `On Error` / log says **"A Spout sender named ... already exists"** → another application (or a leftover PIE
  session) owns the name. Use `Auto Rename`, or `Replace If Owned By This Process` for PIE leftovers.
* **"Unsupported source pixel format"** → the render target uses a format the Spout path does not carry
  (`RTF_R8`, `RTF_RG16f`, `PF_FloatR11G11B10`…). Use `RTF_RGBA8`, `RTF_RGB10A2`, `RTF_RGBA16f` or `RTF_RGBA32f`.
* **"Scene Capture mode needs an owner actor"** → the component was created outside an actor; assign a scene
  capture explicitly or attach the component to an actor.
* Nothing at all in the log → the component's `Auto Start` is off, or `bAllowEditorCapture = false` while in the
  editor.

## The sender is "Active" but nothing appears in the consumer

1. `CyGameCapture.Spout.List` — is the name there, with a plausible size / format?
2. Is the consumer on the **same GPU**? Legacy shared handles do not cross adapters. On laptops force both
   applications to the same GPU (Windows Graphics settings / NVIDIA control panel).
3. Is the source actually being drawn? A `UTextureRenderTarget2D` that nothing renders into stays black; a
   scene capture with `bCaptureEveryFrame = false` only updates on demand.
4. `Frames` in `CyGameCapture.ListSenders` must increase. If it stays at 0 with a high `dropped` count, the
   source size or format changes every frame, or the receiver holds the access mutex (see below).

## Many dropped frames

* **Sender, D3D12**: all three intermediate slots busy — the consumer or the bridge is slower than the game.
  Limit the sender frame rate.
* **Sender or receiver, any RHI**: the Spout access mutex timed out (67 ms) because another process holds it.
  A stalled consumer (a debugger paused on a receiver) does that.
* **Receiver**: the target render target does not match the sender (size or format) and
  `Resize Target Automatically` is off.

## Receiver stays "Waiting"

* The name must match exactly (`CyGameCapture.Spout.List` shows the real names, `::` separators included).
* The sender may publish a zero size while starting; the receiver connects as soon as the info is valid.
* `OpenSharedResource failed` in the log → different GPU adapter, or the sender published an NT handle
  (some non-Spout applications do): only legacy Spout handles are supported.

## PIE leaves senders behind

It should not: streams stop on `EndPlay`, world cleanup and engine exit. If a crash left a name registered,
Spout cleans stale entries when the next sender registers (`CleanSenders`), or restart the editor. The
`Replace If Owned By This Process` policy also reclaims a leftover of the same executable.

## Editor crashes / RHI assertion when starting a stream

Check that the plugin binaries match the engine version in use (a plugin built for 5.5 cannot be loaded by 5.6).
Rebuild with `RunUAT BuildPlugin` for that engine, or let the editor compile the project plugin.

## Nothing works after upgrading the engine

Delete `Binaries/` and `Intermediate/` of both the project and the plugin, then rebuild. The plugin has no
prebuilt libraries, so a clean rebuild always regenerates everything (Spout2 included).
