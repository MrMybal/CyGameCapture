# Game viewport capture

`Source Mode = Game Viewport` sends **the final game image, exactly as the player sees it**, with no second
scene render.

## How it works

The plugin binds `FSlateRenderer::OnBackBufferReadyToPresent`, a public Slate delegate fired on the render
thread for every window just before `Present`, with the window's swap-chain texture. The sender matches the
window against `GEngine->GameViewport->GetWindow()` and copies that texture into its shared texture.

```
Game render ──► Slate composites HUD / UMG ──► back buffer ──► [OnBackBufferReadyToPresent] ──► GPU copy ──► Spout
                                                    │
                                                    └──► Present (unchanged, player sees the normal image)
```

Cost: **one GPU copy** (D3D11) or two (D3D12 bridge). No extra scene render, no `SceneCapture2D`, no CPU readback.

## What the image contains

Everything that Slate composited into the window back buffer:

* the 3D scene with all post-processing,
* the HUD and UMG widgets,
* any Slate overlay drawn in that window (including, in PIE-in-editor, the editor UI — see below),
* **not** the mouse cursor (drawn by the OS) and not the ReShade/Steam-style external overlays.

A "clean, pre-UI" image is not available from a plugin: it would require rendering changes inside the engine
renderer. If you need the scene without HUD, use a `SceneCapture` sender (which re-renders the scene) or
CyGameCaptureRS (which picks a pre-HUD render target from outside the engine).

## Per context

| Context | What the Viewport sender captures |
|---------|-----------------------------------|
| **Standalone game / packaged build** | the game window: exactly the player's image ✔ |
| **PIE → New Editor Window** | the PIE window: the game image ✔ |
| **PIE → Play in Viewport (docked)** | the **whole editor window** (the back buffer belongs to the editor window) ⚠ |
| **Editor, not playing** | nothing (the component starts on BeginPlay) |

For recording, use Standalone or "New Editor Window".

## Resolution and format

The stream follows the window back buffer: resizing the window re-creates the shared texture and fires
`On Resolution Changed`. Typical formats are `R10G10B10A2_UNORM` (UE5 default swap chain) or `B8G8R8A8_UNORM`;
both are sent unchanged. Receivers that only handle 8-bit will show a `R10G10B10A2` stream shifted — prefer
`r.HDR.EnableHDROutput=0` plus a BGRA8 swap chain, or a SceneCapture sender, if your consumer is strict.

## Notes

* Several Viewport senders can coexist (for example one at 120 fps and one limited to 30 fps); they all share
  the same hook and each performs its own copy.
* The hook is bound when the first viewport sender starts and unbound when the last one stops.
* In multi-window setups only the game viewport window is captured; other Slate windows are ignored.
