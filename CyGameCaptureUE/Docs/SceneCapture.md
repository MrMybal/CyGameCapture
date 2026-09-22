# Scene Capture source

`Source Mode = Scene Capture` streams a `USceneCaptureComponent2D`: a second camera rendering the scene into a
render target, which is then copied to Spout.

```
USceneCaptureComponent2D ──► its TextureTarget ──► GPU copy ──► shared texture ──► Spout
```

Unlike *Game Viewport*, this **does** render the scene a second time (that is what a scene capture is), but it
gives a completely independent camera: different position, FOV, post-process, show flags, resolution.

## Two ways to use it

### 1. Your own scene capture (recommended)

Assign an existing `USceneCaptureComponent2D` to the **Scene Capture** property. The plugin uses its
`TextureTarget` as the source and changes nothing else: you keep full control of FOV, projection,
post-process settings, show flags, `bCaptureEveryFrame`, capture source, etc.

If the component has no `TextureTarget`, the plugin creates one using **Scene Capture Size** and
**Scene Capture Format** and assigns it.

### 2. Managed scene capture

Leave **Scene Capture** empty. The component creates a `USceneCaptureComponent2D` attached to the owner actor's
root, with:

* `bCaptureEveryFrame = true`, `bCaptureOnMovement = false`
* `CaptureSource = SCS_FinalColorLDR`
* a transient render target of **Scene Capture Size** / **Scene Capture Format**

Move the actor to move the camera. Retrieve it at runtime with `Get Active Scene Capture` to tweak anything
(FOV, post-process, show flags…). It is destroyed with the component.

## Choosing the capture source

| `CaptureSource` | Result |
|-----------------|--------|
| `SCS_FinalColorLDR` | tone-mapped, sRGB, ready for OBS ✔ (default of the managed capture) |
| `SCS_FinalToneCurveHDR` | HDR after the tone curve, use with an `RTF_RGBA16f` target |
| `SCS_SceneColorHDR` | linear HDR scene colour, needs a float target |
| `SCS_FinalColorLDR` + `bEnableClearOnCapture` + transparent clear | LDR with usable alpha, for compositing |

`SCS_SceneColorSceneDepth`, `SCS_Normal`, etc. also work as long as the render target format is one of the
supported ones (see `RenderTarget.md`).

## Cost

A scene capture renders the scene again: that is by far the dominant cost, not the Spout copy. To keep it
reasonable:

* use a modest resolution (960×540 is plenty for a secondary camera in a stream),
* set `bCaptureEveryFrame = false` and call `CaptureScene()` yourself if the camera does not need every frame,
* or set the sender's **Frame Rate Mode = Custom Frame Rate** *and* drive the capture at the same rate,
* disable expensive show flags in `ShowFlags` (`SetMotionBlur(false)`, screen-space reflections, etc.),
* use `HiddenActors` / `ShowOnlyActors` to limit what is drawn.

Limiting only the sender's frame rate does **not** save the scene capture cost: the capture still renders
every frame unless you also throttle it.

## Timing

The sender component ticks in `TG_PostUpdateWork`, after scene captures have been updated, so the frame it
sends is the one rendered during this frame.

## Multiple cameras

Add one sender component per camera (each with its own scene capture and render target). They are fully
independent: `CyGameCaptureUE::Beyond::CameraA`, `::CameraB`, … See `MultiStream.md`.
