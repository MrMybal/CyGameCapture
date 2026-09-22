# Render Target source

`Source Mode = Render Target` sends any `UTextureRenderTarget2D`, whatever fills it: a Scene Capture, a Canvas
draw, a Draw Material to Render Target, Composure, Niagara, a custom RHI pass, …

```
Anything that renders ──► UTextureRenderTarget2D ──► GPU copy ──► shared texture ──► Spout
```

## Setup

1. Create a render target asset (Content Browser → Textures → Render Target) or one at runtime
   (`Create Render Target 2D`).
2. Assign it to the sender component's **Render Target** property, or call `Set Render Target` at runtime.
3. The stream starts publishing as soon as the target has a valid resource; it waits (state *Waiting*) while the
   property is empty.

## Supported formats

| `ETextureRenderTargetFormat` | Sent as | Notes |
|------------------------------|---------|-------|
| `RTF_RGBA8` | `B8G8R8A8_UNORM` | best choice for OBS and most tools (sRGB encoded) |
| `RTF_RGBA8_SRGB` | `B8G8R8A8_UNORM` | same bits |
| `RTF_RGB10A2` | `R10G10B10A2_UNORM` | 10-bit SDR |
| `RTF_RGBA16f` | `R16G16B16A16_FLOAT` | linear HDR, receivers must expect float |
| `RTF_RGBA32f` | `R32G32B32A32_FLOAT` | heavy, rarely needed |
| `RTF_R8`, `RTF_RG8`, `RTF_R16f`, `RTF_RG16f`, `RTF_R32f`, `RTF_RG32f` | — | rejected: `On Error` "Unsupported source pixel format" |

A `PF_R8G8B8A8` target created with `InitCustomFormat` is sent as `R8G8B8A8_UNORM`.

## Size changes

Calling `Resize Render Target` (or re-creating the asset) makes the sender re-create its shared texture, update
the Spout registration and fire `On Resolution Changed`. Consumers re-open the texture automatically. Doing this
every frame would thrash allocations, so resize only when needed.

## Typical recipes

**Draw a material into the stream**

```
Event Tick ─► Draw Material to Render Target (Target = RT_Stream, Material = MI_Overlay)
```
The sender copies `RT_Stream` every frame; nothing else to do.

**Send a UMG widget**

Use a `Widget Component` in *Render Target* mode (or `UWidgetRenderer` in C++) and send its render target: the
result is the widget alone with alpha, useful for compositing in OBS or Resolume.

**Send a Composure output**

Composure writes into render targets; point the sender at the final output target.

**Debug views**

A small `RTF_RGBA8` target (for example 512×512) limited with `Frame Rate Mode = Custom Frame Rate, 10` gives a
cheap always-on debug stream.

## Alpha

The alpha channel of the render target is transmitted as-is (BGRA8 / RGBA16F). Whether the consumer uses it
depends on the consumer: the OBS Spout2 source has a "premultiplied alpha" option, Resolume uses alpha directly.
Scene captures need `Capture Source = Final Color (with Alpha)` (or a scene capture with `bEnableClearOnCapture`
and a transparent clear colour) for the alpha to be meaningful.
