<p align="center"><img src="Resources/Branding/CyGameCapture_Logo_256.png" width="160" alt="CyGameCapture logo"></p>

# CyGameCapture

**GPU capture toolkit: ReShade add-on, Unreal Engine plugin, Spout2 everywhere.**

Maintained by **Cyberalien**.

CyGameCapture lets you pick any render target of a game's frame (for example the clean, tonemapped scene
*before* the HUD is composited) and stream it to OBS through Spout2, entirely on the GPU.
The player keeps the normal image (gameplay + HUD + menus), OBS receives the buffer you selected.

```
Game -> ReShade -> CyGameCaptureRS -> selected render target -> GPU copy -> shared texture -> Spout2 -> OBS
```

| Component | Role | Status |
|-----------|------|--------|
| `CyGameCaptureRS`   | Native ReShade add-on (`CyGameCaptureRS.addon64`): resource tracker, Buffer Inspector overlay, GPU preview, Spout sender(s) | **D3D11 and D3D12 working** |
| `CyGameCaptureUE`   | Unreal Engine plugin (4.27 -> 5.8): multi-stream Spout2 **sender and receiver** (render target, scene capture, game viewport), Blueprint + C++ | **D3D11 and D3D12 working**, see [its README](CyGameCaptureUE/README.md) |
| `CyGameCaptureAI`   | Helper of the add-on's AI assistant (`CyGameCaptureAI.exe`): turns the frame's pictures into PNG files outside the game and runs the chosen AI client | **working** (D3D11 games) |
| `CyGameCaptureCore` | Small library shared by the add-on, the AI helper and the OBS plugin (sender naming, versions, stream kinds, frame sync) | done |
| `CyGameCaptureOBS`  | Native OBS source plugin ("Add Source -> CyGameCapture"): discovery, auto-reconnect, zero-copy shared texture, and **recording several buffers at once into separate synchronised files** | **working**, see [its README](CyGameCaptureOBS/README.md) |
| `Tests/`            | D3D11 and D3D12 test applications (scene + fake HUD), a headless Spout receiver, and a probe that reports what really survived in a recording | done |
| `CyGameCaptureUEHost/` | Unreal host project used to build, run and benchmark the UE plugin | done |

## Requirements

* Windows 10/11, 64-bit game using **Direct3D 11**, **Direct3D 12** or D3D10. On D3D12 the Spout output goes through a
  D3D11On12 bridge (see the architecture document); everything else is identical.
* **ReShade 6.8.0 with add-on support** (the installer labelled *"Add-on support"* on https://reshade.me). Plain ReShade builds refuse external add-ons. The add-on targets ReShade API version 20 (6.8.0); older ReShade versions (6.7.x = API 18) will not load it.
* OBS Studio: either **CyGameCaptureOBS** (built here, `Add Source -> CyGameCapture`) or the generic
  **Spout2 plugin** (https://github.com/Off-World-Live/obs-spout2-plugin). Any other Spout2 application works too
  (Resolume, TouchDesigner, another Unreal instance...): nothing in the pipeline is OBS specific.
* Visual Studio 2022 (MSVC v143, CMake and Ninja components) to build.

## Build

```bat
build.cmd Release
```

Outputs in `bin\Release\`:

* `CyGameCaptureRS.addon64` (+ `.pdb`)
* `obs-plugins\64bit\CyGameCaptureOBS.dll` and `data\obs-plugins\CyGameCaptureOBS\` — copy both into
  `%ProgramData%\obs-studio\plugins\CyGameCaptureOBS\{bin\64bit,data}`. Run `Tools\GenerateObsImportLib.cmd` once
  first; without it CMake skips the OBS plugin with a warning and builds everything else.
* `CyGameCaptureSpoutReceiverTest.exe`
* `CyGameCaptureVideoProbe.exe` — optional, built only once `Tools\GenerateFFmpegImportLibs.cmd` has been
  run; otherwise CMake skips it with a message and everything else builds as usual.
* `TestApp\CyGameCaptureTestApp.exe` (D3D11) and `TestAppD3D12\CyGameCaptureTestAppD3D12.exe` (D3D12), each with the
  add-on and a `ReShade.ini` next to it (drop the ReShade add-on build `dxgi.dll` there to run them).
  Both accept `-seconds N` to exit on their own.

## A ready-to-run folder

To try everything without building anything — or to hand the whole thing to someone else:

```bat
Tools\MakeTestPackage.cmd
```

It assembles `dist\CyGameCapture_Test\` from the Release build: the ReShade add-on, the OBS plugin with
a one-click installer, the two test applications with ReShade already beside them, the verification
tools, two ready-made OBS scene collections, the Unreal plugin for each engine version, the
documentation and the third-party licences. A French `LISEZMOI.txt` walks through it in five minutes.

Pass `noue` to leave out the Unreal plugin packages, which are more than 200 MB of the ~290 MB total.
Re-run it after any build to refresh the folder.

## Release archives

```bat
build.cmd Release
Tools\MakeRelease.cmd
```

It writes `dist\CyGameCapture-<version>\`: one archive for the ReShade add-on (with the AI helper), one for the OBS
plugin (with its installer), one per engine version for the Unreal plugin (extracted into `<Project>\Plugins`, it gives
`Plugins\CyGameCaptureUE`), a complete archive holding all of them with the documentation, `SHA256SUMS.txt` and the
release notes taken from [CHANGELOG.md](CHANGELOG.md). The Unreal plugin has to be packaged first for each engine version
into `build\Plugin_UE<version>` (see [its README](CyGameCaptureUE/README.md#verified)). The script stops on a binary older
than its sources, a version that does not match `Version.hpp`, or a symbol file or log in the package.

## Install in a game

1. Install ReShade 6.8.0 (add-on support build) for the game as usual.
2. Copy `CyGameCaptureRS.addon64` next to the game executable (or into the folder set by `[ADDON] AddonPath=` in `ReShade.ini`).
3. Start the game; `ReShade.log` shows `Registered add-on "CyGameCaptureRS"` and `[CyGameCaptureRS] Renderer: D3D11 (Spout output available)`.

## Use

1. Press **Home** to open the ReShade overlay. The **CyGameCaptureRS - Buffer Inspector** window opens with it
   (it is also available as a tab of the main ReShade window; the window can be hidden from the add-on settings).
2. **Buffer Inspector** tab: the **Contact Sheet** draws every listed buffer small, side by side, so the scene, the HUD
   or the depth can be spotted at a glance; click one to select it. Below it, the table gives each buffer's resolution
   (with the area actually rendered when the game scales its resolution), format, writes, and **Steady**: the share of
   frames it was written during. A buffer worth capturing is written nearly every frame; the ones an engine takes from a
   pool for a frame or two are what used to make the list flicker, and **Steady buffers only** (on by default) hides them.
   The other filters (written recently, colour targets, depth, resolution relative to the back buffer, format) and
   **Sort by** narrow it further. Browse with **Previous / Next**, the **Left / Right** keys or by clicking; the live GPU
   preview of the selection appears below, and **Freeze Preview** keeps the current image for inspection.
3. Click **Use As Capture Source**. A Spout sender named `CyGameCaptureRS::<Game>` appears in OBS (Spout2 Capture source).
   **Add As Extra Stream** sends another buffer at the same time as `CyGameCaptureRS::<Game>::2`, `::3`...
   There is no fixed number of streams (see [How many streams](#how-many-streams-at-once) below); each one costs one GPU copy per frame.
   **Moment of the frame** (D3D11): reads the selected buffer right after one of its writes instead of at the end of the
   frame. It is the answer for games that have no clean buffer at all, see
   [When the interface is drawn into the final image](#when-the-interface-is-drawn-into-the-final-image). The preview,
   the capture buttons and "show selected buffer in game" all use the moment chosen here.
4. **Show selected buffer in game** (checkbox, or the **F10** hotkey) replaces what the player sees with the selected buffer:
   play without HUD / UI. Works when the buffer has the back buffer size and a copy-compatible format (a scaling / conversion pass is planned).
5. **Capture** tab: status, sender name, resolution, format, FPS, per-stream CPU cost. **Timeline** tab: the write events of the last frame
   (clear / draw / copy / resolve / present per resource). **Debug** tab: tracker statistics and the verbose logging toggle.
6. Tabs can also be switched with **Ctrl+1..4**.

## Configuration (`ReShade.ini`, section `[CYGAMECAPTURE]`)

| Key | Default | Meaning |
|-----|---------|---------|
| `Language`       | `en` | Language of the overlay: `en` (English) or `fr` (French). Also switched from the selector next to the logo |
| `SenderName`     | `CyGameCaptureRS::<exe name>` | Override the Spout sender base name |
| `ViewToggleKey`  | `121` (F10) | Virtual key code toggling "show selected buffer in game" (0 disables) |
| `StandaloneWindow` | `1` | Show the standalone inspector window when the overlay opens |
| `FlushAfterCopy` | `0` | Flush the immediate context after the copies, like Spout's own sender (costs ~200 us CPU per frame, no extra safety) |
| `Verbose`        | `0` | Log every tracked resource creation/destruction |
| `AutoCapture`    | *(empty)* | Test / bootstrap hook: `1280x720:R8G8B8A8_UNORM[;1280x720:D32_FLOAT:depth][;1280x720:R8G8B8A8_UNORM:hud]` selects matching buffers automatically once the frame is stable. The `:depth` and `:hud` suffixes pick the derived streams; a `:hud` entry subtracts whatever the first entry selected. An entry can also name one buffer exactly, `#42` (ids are stable within a run, see `DumpBuffers`), and read it at a moment of the frame, `#42@2` (right after its 2nd write); `#5@2;#5:hud` gives the game without its interface on stream 1 and the interface alone on stream 2 |
| `DumpBuffers`    | `0` | At that frame, write every buffer written during the frame to `ReShade.log` once (id, size, active area, format, writes, steadiness), to choose a buffer from a log file alone |
| `AIClient`       | `auto` | AI assistant client: `auto` (first installed of Claude Code, Codex, OpenCode), `claude`, `codex`, `opencode` |
| `AIModel`        | *(empty)* | Model passed to the client (`--model`); empty for the client's default |
| `AIClientPath`   | *(empty)* | Full path of the client's `.exe` when it is not found automatically |
| `AILanguage`     | *(the overlay's language)* | Language of the AI assistant's replies, when it should differ from the overlay's: `fr` or `en` |
| `AISearchOnStart` | `0` | Test hook: presses the AI assistant's button once at that frame (60 at the earliest) |
| `OpenOverlayOnStart` | `0` | Open the ReShade overlay by itself once the game has settled. For automated checks: sending the overlay key lands in whichever window has the focus |
| `DepthFarPlane`  | `1000` | Distance the depth linearisation maps to white. Game specific, same meaning as `RESHADE_DEPTH_LINEARIZATION_FAR_PLANE` |
| `DepthReversed`  | `0` | The depth buffer is 1 at the near plane (common in modern engines) |
| `DepthLogarithmic` | `0` | The depth buffer is distributed logarithmically |
| `DepthUpsideDown` | `0` | The depth buffer is stored flipped compared to the colour buffer |
| `HudThreshold`   | `0.02` | Below this much difference between the two buffers, a pixel counts as pure scene |
| `HudSoftness`    | `0.08` | Difference at which the HUD mask is fully opaque |
| `HudKeepFinalColour` | `1` | Keep the finished colours in the HUD stream instead of the raw difference |

## Check what a recording really contains

Recording a buffer does not mean keeping it: an encoder can quietly halve its precision, squeeze it into
limited range or subsample its chroma. `CyGameCaptureVideoProbe` decodes a file and says what survived —
the pixel format the codec actually used, its bit depth, and how many *distinct* values the picture
holds:

```bat
bin\Release\CyGameCaptureVideoProbe.exe "take.mkv" 2
```

`--dump <component> <folder>` writes one component of each frame as a grey-scale `.bmp`, which is how you
look at a channel that is invisible otherwise — `--dump 3` saves the alpha, where an isolated HUD keeps
its mask.

```
codec  : ffv1
stored : yuv444p16le 1280x720
  frame  0  yuv444p16le  1280x720  planar  3 component(s), NO ALPHA CHANNEL
      Y  depth=16  1280x720   distinct levels=  4945  min=11422 max=65507 mean= 52375.2  (0.1743..0.9996)
      U  depth=16  1280x720   distinct levels=     5  ...
```

A depth stream that was 16-bit at the source and comes back with 214 distinct levels lost everything
below eight bits somewhere, whatever the codec calls itself. That is how the recording modes of
CyGameCaptureOBS were measured.

It decodes with the FFmpeg libraries OBS already installs, so there is nothing extra to install: run
`Tools\GenerateFFmpegImportLibs.cmd` once to build the import libraries from them. Set `CYGC_OBS_BIN` to
use an `obs-studio\bin\64bit` folder other than the installed one.

## Validate without OBS

Start the test app with ReShade, then:

```bat
bin\Release\CyGameCaptureSpoutReceiverTest.exe --list
bin\Release\CyGameCaptureSpoutReceiverTest.exe --frames 3 --out %TEMP%
```

The tool behaves like the OBS Spout2 plugin (SpoutDX receiver), lists every `CyGameCaptureRS::*` sender and dumps received frames as `.bmp`.

## The AI assistant: "find the setting that removes the game's interface"

At the top of the **Buffer Inspector** tab, the **AI Assistant** section does the search above for you, with an AI
command line client already installed and signed in on this PC (Claude Code, Codex or OpenCode). Show a scene where
the game's interface is visible, pick the client, click the button: it is the automatic first message of a discussion,
which opens in its own window (**CyGameCapture - AI Assistant**). A minute or two later the reply appears, and every
proposal in it comes with **Show** / **Apply** / **Apply, and send the interface alone too**. Nothing changes before
that click.

Then answer in the discussion: "the subtitles are still there", "and the interface alone?"... **Attach the current
frame** (on by default) sends fresh pictures with the message, for instance after moving to a scene where the problem
shows; **Send** or Ctrl+Enter sends it; **New discussion** starts over. The clients keep no session: the conversation
(its last 12 messages) goes with each message, which works the same with all three.

What happens behind the button:

1. The add-on renders small pictures of the frame on the GPU into one shared texture: the back buffer after each of
   its writes, and the full-size buffers at the end of the frame. That is all the game process does.
2. `CyGameCaptureAI.exe`, next to the add-on, reads the pictures back in its own process, writes them as PNG files,
   and runs the chosen client non-interactively with the method (the reasoning of the section below) and the frame's
   buffer list and write order. The client runs with no tools, no user configuration and no saved session; the command
   lines are the ones CyAICodex (a sibling project) already uses with these clients.
3. A reply that proposes a setting ends with a JSON object (buffer, moment of the frame, whether the interface alone
   can be had); the helper checks it against the buffers of the frame, removes it from the text, and hands both back to
   the overlay, where it becomes the buttons.

Measured: on the D3D11 test application it proposes the clean scene buffer `#005`; on Stray's title screen it proposes
the back buffer right after its 2nd write, the setting found by hand below. With a black loading screen it answers that
it cannot tell and asks for a scene with the interface visible.

What leaves the PC goes to the provider of the client you chose, with your account: the pictures and the buffer list.
CyGameCapture stores no key. Everything that was sent and received stays readable in
`%LOCALAPPDATA%\CyGameCapture\AI\<game>` (`prompt.txt`, `answer.txt`, `picture_NN.png`). D3D11 only for now.

## When the interface is drawn into the final image

Some games never keep their picture without the interface in a buffer of its own. Unreal Engine 4 is the common case:
the tonemapper writes the finished image straight into the back buffer, and the interface is drawn into that same
buffer right after. `DumpBuffers` shows it plainly in Stray:

```
event 60  #005 back buffer  Clear  x1
event 61  #005 back buffer  Draw   x3    tonemapper, then the interface, in the same buffer
event 62  #005 back buffer  Present
```

At the end of the frame, no buffer without the interface exists anywhere, so none can be picked. What does exist is a
*moment*: after the 2nd write of `#005` (the clear, then the tonemapper) and before the 3rd (the first interface draw).
The **Moment of the frame** slider reads the buffer right there: the add-on sees every write before it reaches the GPU
(ReShade reports draws, clears and copies ahead of forwarding them), and when the chosen one is about to happen it
slips a copy of the buffer in first. That copy is what the preview shows and what the stream sends. The player still
sees the normal image.

How to find the moment: select the back buffer, look at the line *Writes of this buffer in the last frame* under the
slider (`1 clear, 2-4 draw` in Stray's menus), and move the slider back until the interface disappears from the
preview. In Stray, *after write 2* gives the scene without the menu, its blur and its darkening. With that moment on
stream 1, **Isolate HUD Against Stream 1** on the same back buffer at the end of the frame sends the interface alone.

Limits: D3D11 only for now (D3D12 records its command lists in parallel, so the order of the writes is not known while
they are recorded); multisampled buffers are refused; one extra full-size GPU copy per frame per moment in use.
Anything the game draws *before* its interface in the same buffer stays, of course: Stray's title logo is part of the
3D scene and remains.

## Derived streams: depth and HUD

Two of the streams are not buffers the game holds; the add-on computes them with a GPU pass and sends
them like any other stream.

* **Depth.** Select a depth buffer and click **Use As Depth Stream**. It is linearised and published as
  16 bits per channel (`R16G16B16A16_UNORM`), which is what a depth-based 3D conversion needs. The far
  plane, reversed-Z, logarithmic and upside-down settings are game specific and cannot be detected from
  outside, so they sit in the Capture tab (and in `ReShade.ini`); they have exactly the meaning of
  ReShade's own `RESHADE_DEPTH_*`, so anything already worked out for a game carries over.
* **HUD on its own.** In most games the interface is composited onto the image rather than kept in its
  own buffer, so it is the difference between the finished image and the scene before it. Capture the
  clean scene as stream 1, select the finished image (usually the back buffer), then click **Isolate HUD
  Against Stream 1**: the stream carries the interface with a mask in its alpha channel. Exact wherever
  the interface is opaque, an approximation in its soft edges — the threshold and softness are
  adjustable. When a game *does* render its interface to its own render target with alpha, select that
  buffer directly instead: a plain copy is exact and cheaper.

Both run on D3D11 and D3D12, and neither touches the CPU.

**Dynamic resolution is handled.** A game that scales its resolution at run time normally keeps its
render targets at full size and draws into a corner of them; the texture never changes, so only the
viewport says how much of it holds the frame. The add-on follows that viewport and scales the rendered
area into a stream of stable size, rather than sending a picture with a stale margin or making the
sender resize several times a second. The overlay shows the rendered area next to the stream resolution. When the buffer you select is a back buffer —
which the finished image usually is — the stream follows whichever back buffer the swap chain is
presenting rather than the one resource you clicked: a flip-model swap chain rotates between several, and
capturing a fixed one would hand you a stale frame every other present.

## Recording several buffers at the same time

Several buffers of the same frame can be recorded simultaneously, each into its own file, so they can be
laid on top of each other in an editor afterwards: the scene without the HUD next to the final image, or
the colour next to the depth for a 3D conversion. This lives in CyGameCaptureOBS (one libobs video mix,
encoder and muxer per buffer) — see [its README](CyGameCaptureOBS/README.md#recording-several-buffers-at-once).

To keep the files honest, the producers publish a per-stream frame index in shared memory next to their
Spout senders (`CyGameCaptureCore/FrameSync.hpp`): every stream of one presented frame carries the same
number, and the recorder reports the drift between them.

The depth and HUD streams above are ordinary streams once produced, so they record like any other, and
the recorder offers a lossless mode (FFV1) next to the hardware encoder. How much of a 16-bit depth
buffer actually survives each one was measured rather than assumed — 214, 249 and 4945 distinct levels
respectively, out of 65536 — see
[the OBS plugin README](CyGameCaptureOBS/README.md#how-much-of-a-depth-buffer-survives-a-recording).
The short version: the hardware encoder is unusable for depth, lossless 8-bit RGB is linear and exact to
eight bits, lossless 16-bit keeps about twelve, and nothing gets the full sixteen through OBS because
libobs has no 16-bit unorm video mix. When the last bit matters, read the Spout stream directly.

For an **isolated HUD** the answer is simpler: its mask lives in the alpha channel, and only the 8-bit
lossless RGB mode has an alpha channel at all — the other two are YUV and drop the cut-out silently. The
plugin warns before a take when the chosen encoding would lose it.

## How many streams at once

There is **no artificial limit** in CyGameCapture. What actually limits you:

* **Spout's sender name table is machine wide** and holds **64 entries by default** — shared by every Spout
  application running on the computer, not just ours. `CreateSender` fails *silently* once it is full, so both the
  ReShade add-on and the Unreal plugin check it first and tell you how many slots are in use instead of appearing to
  work. The size can be raised in the registry under `HKCU\Software\Leading Edge\Spout\MaxSenders` (Spout's own
  SpoutSettings tool writes that key).
* **Receivers are not in that table at all**: as many as you like, bounded only by GPU memory.
* **GPU cost**: one copy per stream per frame (two on D3D12, through the D3D11On12 bridge). This is what really
  decides how many streams a given machine sustains.

The numbers you may still see in the code (`kSanityStreamLimit = 256` in the add-on, `StreamSanityLimit` in the
Unreal project settings) exist only so a runaway loop cannot create streams forever; raise them freely.

## Before publishing

Secret scanning with [gitleaks](https://github.com/gitleaks/gitleaks), configured by
[`.gitleaks.toml`](.gitleaks.toml) so the same check runs every time:

```bash
gitleaks dir . --config .gitleaks.toml --redact
```

It uses gitleaks' own rules and only skips what is never committed (build outputs, the packaged release, local
tooling). No finding is not a proof that there is none: the files that would be committed, the pictures used in the
documentation and the contents of a release package are worth a look of their own.

## Languages

The overlay is in English by default and switches to French from the selector next to the logo (saved as `Language`
in `ReShade.ini`); the AI assistant replies in the same language. Every text is written in English where it is used,
wrapped in `TR()`, and translated in [`Translations_fr.inl`](CyGameCaptureRS/Source/UI/Translations_fr.inl): a text
with no translation there simply shows in English. The OBS plugin follows OBS's own language (`data/locale`).

## Logo and icon

The logo and the Windows icon live in [`Resources/Branding`](Resources/Branding). Every binary embeds the icon
(the add-on, the OBS plugin, the test tools), and the add-on also embeds the logo, which the overlay draws at the top
of its window. The icon and the smaller PNGs are derived from `CyGameCapture_Logo.png`; to change the logo, replace that
file (or pass the new one) and run `python Tools/GenerateBranding.py` (needs Pillow). The results are committed, so a
normal build does not need Python.

## Documentation

* [CyGameCaptureUE/README.md](CyGameCaptureUE/README.md) and [CyGameCaptureUE/Docs/](CyGameCaptureUE/Docs/) - the Unreal plugin (architecture, sender, receiver, multi-stream, viewport capture, OBS, performance, troubleshooting).
* [Docs/CyGameCapture_Architecture.md](Docs/CyGameCapture_Architecture.md) - architecture, ReShade callbacks used, tracking / preview / capture strategies, Spout2 integration, D3D11 and D3D12 backends, synchronisation, roadmap, known limitations.
* [CyGameCaptureOBS/README.md](CyGameCaptureOBS/README.md) - the OBS plugin (build, install, source properties, the zero-copy path).
* [CyGameCaptureRS/ThirdParty/THIRD_PARTY_NOTICES.md](CyGameCaptureRS/ThirdParty/THIRD_PARTY_NOTICES.md),
  [CyGameCaptureOBS/ThirdParty/THIRD_PARTY_NOTICES.md](CyGameCaptureOBS/ThirdParty/THIRD_PARTY_NOTICES.md) and
  [Tests/CyGameCaptureVideoProbe/ThirdParty/THIRD_PARTY_NOTICES.md](Tests/CyGameCaptureVideoProbe/ThirdParty/THIRD_PARTY_NOTICES.md) - vendored SDK subsets and licences (ReShade headers, Dear ImGui header, Spout2, libobs headers, FFmpeg headers).

## Licence

Copyright (C) 2026 Cyberalien.

CyGameCapture is free software: you can redistribute it and/or modify it under the terms of the
[GNU Affero General Public License](LICENSE) as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version. It is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the licence for
more details.

This covers the ReShade add-on, the AI helper, the OBS plugin, `CyGameCaptureCore`, the tests and the tools.

**The Unreal Engine plugin is the exception: `CyGameCaptureUE` and its host project `CyGameCaptureUEHost` are under the
[MIT licence](CyGameCaptureUE/LICENSE).** The Unreal Engine EULA does not allow the engine to be combined with code under
GPL-family licences, so an AGPL plugin could not be used in an Unreal project. The plugin shares no code with the AGPL
components.

Every source file names its licence on its first lines (`SPDX-License-Identifier`). The third-party code keeps its own
licence, listed in the `THIRD_PARTY_NOTICES.md` files above: ReShade headers (BSD-3-Clause or MIT), Dear ImGui (MIT),
Spout2 (BSD-2-Clause), libobs headers (GPL-2.0-or-later, which the AGPL-3.0 OBS plugin can link through GPL-3.0's
section 13) and FFmpeg headers (LGPL-2.1-or-later, verification probe only).

## Scope note

CyGameCapture only targets applications where ReShade is normally usable. It contains nothing to bypass anti-cheat or ReShade blocking; such applications are simply unsupported.
