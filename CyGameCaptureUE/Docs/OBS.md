# OBS

OBS is **not** a dependency: CyGameCaptureUE speaks plain Spout2, so it works with Resolume, TouchDesigner,
another Unreal instance, CyGameCaptureRS or any custom Spout application. This page just documents the OBS case
because it is the most common one.

## Requirements

* OBS Studio with the **Spout2 plugin** (https://github.com/Off-World-Live/obs-spout2-plugin) — it provides both
  the *Spout2 Capture* source and the *Spout2 Output* (OBS → Spout).
* Unreal running with D3D11 or D3D12 on the **same GPU** as OBS.

## Unreal → OBS

1. In Unreal, start a sender (any source mode). The log prints the final name:
   `Spout sender registered: 'CyGameCaptureUE::Beyond::Viewport' 1280x720 R10G10B10A2_UNORM`.
2. In OBS: **+ → Spout2 Capture** → *Spout Sender* → pick the name → OK.
3. The image appears immediately. Resizing the Unreal window / changing the render target size updates the OBS
   source automatically (the plugin re-registers the new size and OBS re-opens the texture).

Nothing else to configure: no colour format, no composite mode.

## OBS → Unreal

1. In OBS: **Tools → Spout2 Output settings** (or the output entry added by the plugin) → enable it. OBS
   publishes a sender, typically named `OBS_Program` (and `OBS_Preview` in studio mode).
2. In Unreal: add a **CyGameCapture Spout Receiver**, set *Sender Name* to that name, keep
   *Output Mode = Internal Render Target*.
3. `Get Receiver Texture` gives a `UTextureRenderTarget2D` you can put on a material (a TV screen in the level,
   a UMG image, a Composure layer, a Niagara texture sampler…).

## Naming for OBS scenes

OBS stores the sender name in the scene. If a scene must survive project renames, use a fixed custom name:

* Sender component → **Use Custom Sender Name** = true, **Sender Name** = `Beyond_View`.
* Keep the default `Auto Rename` policy, or use `Replace If Owned By This Process` if you want the same name
  reused across PIE sessions (the plugin then takes over the leftover sender of the previous session only).

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| The sender is not in OBS's list | the OBS Spout2 plugin is not installed, or OBS was started before the sender: reopen the source properties (the list is refreshed on open) |
| Black image in OBS | check the RHI (`CyGameCapture.ListSenders` shows the format; Vulkan is not supported), and that Unreal and OBS run on the same GPU |
| Colours look washed out / too bright | the sender uses a float (HDR) render target; use an 8-bit target (`RTF_RGBA8`) for OBS |
| Image freezes when resizing | normal for one or two frames: the shared texture is re-created and OBS re-opens it |
| Stutter at high frame rate | limit the sender (`Frame Rate Mode = Custom Frame Rate`, 60) so it matches the OBS canvas |
