---
id: configuration
title: Languages, shortcuts and configuration
description: CyGameCapture — Languages, shortcuts and configuration
section: User guide
order: 12
revision: 3c2a9691c4a7
---

The overlay starts in **English**. Choose English or Français next to the logo; the choice is saved in `ReShade.ini`. The guide button opens the corresponding edition. OBS follows its own language; unsupported languages use the English guide. The manual’s language links switch editions while keeping the same chapter. The default `index.html` is always English.

Settings are stored in `ReShade.ini` under `[CYGAMECAPTURE]`. Close the game before editing manually and back up the file. Key names and values are the same in both languages.

```ini
[CYGAMECAPTURE]
Language=en
PreserveSourceAlpha=0
ViewToggleKey=121
AIClient=auto
```

| Key | Default | Meaning |
| --- | --- | --- |
| `Language` | `en` | Language of the overlay: `en` (English) or `fr` (French). Also switched from the selector next to the logo |
| `SenderName` | `CyGameCaptureRS::<exe name>` | Override the Spout sender base name |
| `ViewToggleKey` | `121` (F10) | Virtual key code toggling "show selected buffer in game" (0 disables) |
| `PreserveSourceAlpha` | `0` | Default for new colour streams: 0 makes alpha opaque without changing RGB, 1 preserves the game's alpha. HUD streams always preserve their mask. |
| `StandaloneWindow` | `1` | Show the standalone inspector window when the overlay opens |
| `FlushAfterCopy` | `0` | Flush the immediate context after the copies, like Spout's own sender (costs ~200 us CPU per frame, no extra safety) |
| `Verbose` | `0` | Log every tracked resource creation/destruction |
| `AutoCapture` | *(empty)* | Test / bootstrap hook: `1280x720:R8G8B8A8_UNORM[;1280x720:D32_FLOAT:depth][;1280x720:R8G8B8A8_UNORM:hud]` selects matching buffers automatically once the frame is stable. The `:depth` and `:hud` suffixes pick the derived streams; a `:hud` entry subtracts whatever the first entry selected. An entry can also name one buffer exactly, `#42` (ids are stable within a run, see `DumpBuffers`), and read it at a moment of the frame, `#42@2` (right after its 2nd write); `#5@2;#5:hud` gives the game without its interface on stream 1 and the interface alone on stream 2 |
| `DumpBuffers` | `0` | At that frame, write every buffer written during the frame to `ReShade.log` once (id, size, active area, format, writes, steadiness), to choose a buffer from a log file alone |
| `AIClient` | `auto` | AI assistant client: `auto` (first installed of Claude Code, Codex, OpenCode), `claude`, `codex`, `opencode` |
| `AIModel` | *(empty)* | Model passed to the client (`--model`); empty for the client's default |
| `AIClientPath` | *(empty)* | Full path of the client's `.exe` when it is not found automatically |
| `AILanguage` | *(the overlay's language)* | Language of the AI assistant's replies, when it should differ from the overlay's: `fr` or `en` |
| `AISearchOnStart` | `0` | Test hook: presses the AI assistant's button once at that frame (60 at the earliest) |
| `OpenOverlayOnStart` | `0` | Open the ReShade overlay by itself once the game has settled. For automated checks: sending the overlay key lands in whichever window has the focus |
| `DepthFarPlane` | `1000` | Distance the depth linearisation maps to white. Game specific, same meaning as `RESHADE_DEPTH_LINEARIZATION_FAR_PLANE` |
| `DepthReversed` | `0` | The depth buffer is 1 at the near plane (common in modern engines) |
| `DepthLogarithmic` | `0` | The depth buffer is distributed logarithmically |
| `DepthUpsideDown` | `0` | The depth buffer is stored flipped compared to the colour buffer |
| `HudThreshold` | `0.02` | Below this much difference between the two buffers, a pixel counts as pure scene |
| `HudSoftness` | `0.08` | Difference at which the HUD mask is fully opaque |
| `HudKeepFinalColour` | `1` | Keep the finished colours in the HUD stream instead of the raw difference |

`Home` opens ReShade by default (ReShade can remap it). `F10` toggles the selected image in game. `Ctrl`+`1`…`4` selects inspector, capture, timeline and debug while the overlay is focused. Left/Right browse buffers. `Ctrl`+`Enter` sends an assistant message.


Press **F1** while the ReShade inspector is focused to open help for its current tab. Timeline and Debug target the same explicit section in the inspector guide.
