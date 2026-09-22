# CyGameCaptureOBS

Native OBS Studio plugin that receives the GPU buffers published over Spout2 by **CyGameCaptureRS**
(the ReShade add-on) and **CyGameCaptureUE** (the Unreal Engine plugin) — and any other Spout2 sender
when you ask for it.

```
CyGameCaptureRS / CyGameCaptureUE ──► Spout2 shared texture ──► CyGameCaptureOBS ──► OBS scene
                                        (GPU, no copy)
```

In OBS: **Sources → + → CyGameCapture**.

## Why a dedicated plugin instead of the generic Spout2 plugin

The generic Spout2 OBS plugin works and stays a perfectly valid option. This one adds what a generic
receiver cannot know about:

* it recognises `CyGameCaptureRS::*` and `CyGameCaptureUE::*` senders and shows them first, with a
  readable name (`Beyond` instead of `CyGameCaptureRS::Beyond`) and where they come from;
* it reconnects on its own when a game is restarted or a stream is resized, and says *"Capture source
  lost"* rather than quietly showing the last frame forever;
* it never silently switches to a different sender — switching only happens when you turned
  "Reconnect automatically" on;
* it shows how full Spout's machine-wide sender table is, which is the only real limit on the number
  of simultaneous streams.

## The video path

No CPU readback anywhere:

1. Spout publishes, in shared memory, a legacy DXGI **shared handle** of a texture the sender owns.
2. The plugin reads that description (name, size, DXGI format, handle) — CPU, a few bytes per frame.
3. OBS opens **that very texture** on its own D3D11 device with `gs_texture_open_shared()`.
4. `obs_source_draw()` draws it into the scene.

The picture never leaves the GPU. Nothing is decoded, converted or copied by this plugin.

## Building

`build.cmd Release` at the root of the repository builds it together with the rest, into
`bin\Release\obs-plugins\64bit\CyGameCaptureOBS.dll` plus `bin\Release\data\obs-plugins\CyGameCaptureOBS\`.

It needs an import library for `obs.dll`, which OBS does not ship. Generate it once from the installed
OBS Studio:

```bat
Tools\GenerateObsImportLib.cmd
```

(Optionally pass the path of another `obs.dll`.) Without it, CMake skips the OBS plugin with a warning
and still builds everything else.

## Installing

Copy the two outputs next to each other under OBS' user plugin folder — no administrator rights needed:

```
%ProgramData%\obs-studio\plugins\CyGameCaptureOBS\bin\64bit\CyGameCaptureOBS.dll
%ProgramData%\obs-studio\plugins\CyGameCaptureOBS\data\locale\en-US.ini
```

Note that on Windows OBS looks in `%ProgramData%`, **not** `%APPDATA%`, for user plugins. Alternatively
`OBS_PLUGINS_PATH` and `OBS_PLUGINS_DATA_PATH` can point OBS at any folder, which is convenient while
developing.

Then restart OBS and add the source. `bin\Release\obs-plugins\64bit` and `bin\Release\data\obs-plugins`
already have the right layout to be copied over directly.

## Source properties

| Setting | What it does |
|---------|--------------|
| **Capture source** | The Spout sender to receive. `(Automatic)` takes the first CyGameCapture sender that appears. A sender you picked earlier stays in the list as *(not running)* so it can be reconnected later. |
| **Refresh the list** | Re-reads Spout's sender table. |
| **List every Spout sender** | Also shows senders from other applications (Resolume, TouchDesigner, another OBS...). |
| **Reconnect automatically** | Reconnect on its own when the chosen sender comes back. |
| **Hold the last frame** | Keep the last picture when the sender holds the access mutex for a frame, instead of showing nothing. |
| **Status** | What is connected, its size and frame rate, and how many of Spout's machine-wide sender slots are in use. |

## Recording several buffers at once

A game frame holds more than one interesting picture. Record them **at the same time, each into its own
file**, and they can be laid on top of each other in an editor afterwards: the scene without the HUD next
to the final image, or the colour next to the depth for a 3D conversion.

One CyGameCapture source per buffer, tick **Record this buffer** on each, then **Start recording all
selected buffers**. Every enabled buffer is started inside the same call, so the files begin on the same
frame.

| Setting | What it does |
|---------|--------------|
| **Record this buffer** | Include this buffer in the take. |
| **Take folder** | Where the files go. All buffers of one take share a time stamp, and each file is named after its source and its sender, e.g. `2026-09-17 15-21-41 Clean - Beyond.mkv` and `2026-09-17 15-21-41 HUD - Beyond-2.mkv`. |
| **Encoder** | Hardware first (NVENC HEVC, NVENC H.264, AMF, QuickSync), x264 last. Several buffers encode at once, so a hardware encoder matters. |
| **Container** | Matroska survives a crash or a power cut; an unfinished MP4 does not. |
| **Audio track** | Which OBS audio track goes into the file — see the note below. |
| **Start the take by itself** | Begin as soon as every selected buffer has a picture, without clicking. Fires once per OBS run; for recording a whole session unattended. |
| **Status** | Number of buffers recording, the drift between them, and the current file with its frame count. |

### How the files stay in step

Each buffer gets its own libobs video mix (`obs_view_add2`) at its **native resolution**, its own encoder
and its own muxer. Every mix is rendered in the same OBS graphics tick, so the Nth frame of every file is
the same tick — the files start together and stay together.

What that alone would not prove is that the *game* had published the same frame into each shared texture
at that moment: Spout carries a texture and nothing else. So the producers publish a per-stream frame
index in shared memory next to their senders (`CyGameCaptureCore/FrameSync.hpp`), and the status line
shows the **drift**: the gap, in game frames, between the streams of the take. `0` means they are frame
for frame. A foreign Spout sender publishes no index and simply cannot be checked.

### Two things to know

- **Every file carries an audio track.** OBS' `ffmpeg_muxer` declares itself an audio+video output and
  libobs refuses to start such an output without an audio encoder, so a video-only file is not on offer.
  It costs about 1 MB per minute, negligible even on a data buffer.
- **The take stops buffer by buffer**, so one file can end one frame later than another. The *starts* are
  aligned, which is what matters; trim the tail if you need the exact same length.

### Encoding, and what survives

| Mode | Written as | What it preserves |
|------|-----------|-------------------|
| **Hardware encoder** | NVENC / AMF / QuickSync, 8-bit 4:2:0 | The picture. Lossy, chroma subsampled, and OBS' usual limited range. Right for anything you will watch. |
| **Lossless, 8-bit RGB** | FFV1, `bgra` | Bit exact, and the only mode with an **alpha channel** — the one an isolated HUD keeps its mask in. The mix is staged with no conversion at all and FFV1 takes BGRA directly, so a colour buffer or a HUD comes back identical to what was drawn. |
| **Lossless, 16-bit 4:4:4** | FFV1, `yuv444p16le` | The most a 16-bit stream can keep here: around twelve of its sixteen bits. For depth. No alpha channel. |

The lossless modes bypass the hardware encoder entirely and use OBS' raw "Custom Output (FFmpeg)" output,
pointed at this buffer's own mix, so nothing re-encodes or re-samples in between.

### Depth and HUD streams

CyGameCaptureRS can publish two streams that are not buffers the game holds: a **linearised depth**
stream (16 bits per channel) and the **HUD on its own** (the finished image minus the scene before it,
with a mask in the alpha channel). They arrive here as ordinary CyGameCapture senders and record like
any other buffer.

#### How much of a depth buffer survives a recording

This was measured rather than guessed: the same depth stream, 16-bit with values spanning
0.0257 to 0.9994, recorded three ways at once and decoded back with `CyGameCaptureVideoProbe`
(in `Tests/`, see the root README). Re-run it on your own takes the same way:

```bat
bin\Release\CyGameCaptureVideoProbe.exe "take.mkv" 2
```

| Mode | Stored as | Distinct levels | Range recovered | Transfer applied |
|------|-----------|-----------------|-----------------|------------------|
| Hardware (NVENC HEVC) | `yuv420p`, 8-bit | **214** | 0.086 – 0.922 | gamma **and** limited range **and** 4:2:0 |
| Lossless, 8-bit RGB | `bgra`, 8-bit | **249** | 0.0275 – 1.0 | none, values are linear |
| Lossless, 16-bit 4:4:4 | `yuv444p16le` | **4945** | 0.1743 – 0.9996 | sRGB curve |

Reading that table:

* **The hardware encoder is not usable for depth.** 214 levels out of 65536, squeezed into limited range
  and chroma subsampled. Fine as a visual reference, useless for a reprojection.
* **Lossless 8-bit RGB is linear and exact** — 249 of the 250 levels an 8-bit quantisation of that range
  can hold. Use it when you want depth values you can read straight off, and eight bits is enough.
* **Lossless 16-bit keeps the most**, about 4945 levels ≈ 12.3 bits. It is not the full 16: OBS renders
  a 16-bit mix into an `RGBA16F` texture (see `obs_init_textures` in `libobs/obs.c`), and a half float
  carries roughly eleven bits of mantissa — denser near zero, which is where a linearised depth puts its
  near geometry. Its values also come out **sRGB encoded**, which is invertible: apply the standard sRGB
  → linear transfer to get the depth back. Verified on the recording: a source value of 0.0257 arrives
  as 0.1743, and sRGB(0.0257) = 0.1747.

#### An isolated HUD: only one mode keeps the mask

The HUD stream carries the interface in RGB and its **mask in the alpha channel**. Same measurement, the
same HUD stream recorded three ways at once:

| Mode | Stored as | Alpha channel | Mask |
|------|-----------|---------------|------|
| Hardware (NVENC HEVC) | `yuv420p` | **none** | lost |
| Lossless, 16-bit 4:4:4 | `yuv444p16le` | **none** | lost |
| Lossless, 8-bit RGB | `bgra` | **yes** | kept, exactly |

YUV formats have no alpha to put a mask in, so two of the three modes drop it silently. Only the 8-bit
lossless mode is RGB with alpha. Verified by pulling the alpha back out of the recorded file
(`CyGameCaptureVideoProbe --dump 3 <folder>`): it is the HUD cut-out, pixel for pixel, and its mean
matches the live Spout stream to the decimal (18.2 of 255 in both). The plugin warns in the OBS log
before a take when the chosen encoding would throw the mask away.

**Record an isolated HUD in "Lossless, 8-bit RGB".**

Measured on Direct3D 12 as well as Direct3D 11, with the clean scene, the linearised depth and the
isolated HUD published and recorded at once: three files of 395 frames each, the colour bit exact with
its alpha a constant 255, the depth at 4763 distinct 16-bit levels with perfectly neutral chroma
(U = V = 32768, so it is a true grey scale), and the HUD mask at mean 18.2/255 — the same number the live
Spout stream reports, on both back ends.

One useful detail about how it comes out: OBS clears its mix to transparent and composites the source
onto it, so the recorded RGB is already **premultiplied** by the mask — the interface cut out on black.
That is what a compositing tool wants; set the footage to premultiplied alpha rather than straight.

**There is no way to record a full 16-bit stream through OBS.** Every video mix renders into 8-bit BGRA
or into RGBA16F; libobs has no 16-bit unorm mix, so the precision is capped before any encoder is
involved. When the last bit matters, read the Spout stream directly — it carries the exact 16 bits.

A related detail the plugin handles for you: a depth stream is marked as data in the frame-sync channel,
and data streams are drawn with OBS' sRGB handling turned off (`gs_set_linear_srgb(false)`). Without
that, OBS encodes the values on the way into the mix and even the 8-bit lossless file comes out
gamma-curved instead of linear.

## Limits

There is no artificial limit in this plugin: add as many CyGameCapture sources as you like, they are
independent. Receivers take no slot in Spout's name table; only *senders* do, and that table holds 64
entries by default for the whole machine (see `CyGameCaptureUE/Docs/MultiStream.md`).

## Tested against

OBS Studio 29.1.3 (libobs API 29.1.3) on Windows 10, receiving a Direct3D 12 sender published by
CyGameCaptureRS through its D3D11On12 bridge, and Direct3D 11 senders from both CyGameCaptureRS and
CyGameCaptureUE. Two buffers of the same Direct3D 12 frame recorded simultaneously with NVENC HEVC gave
two files of 1053 and 1054 frames from 1067 drawn frames each, with a frame-index drift of 0.

## Licence

CyGameCaptureOBS is under the GNU AGPL v3 or later, like the rest of CyGameCapture except the Unreal plugin
(see [`LICENSE`](../LICENSE) and the project's README). It links libobs, which is GPL-2.0-or-later: its "or later"
clause makes GPL-3.0 available, and section 13 of GPL-3.0 explicitly allows combining it with AGPL-3.0 code. See
[`ThirdParty/THIRD_PARTY_NOTICES.md`](ThirdParty/THIRD_PARTY_NOTICES.md). The ReShade add-on and the
Unreal plugin link nothing from OBS.
