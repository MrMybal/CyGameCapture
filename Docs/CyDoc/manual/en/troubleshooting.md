---
id: troubleshooting
title: Troubleshooting
description: CyGameCapture — Troubleshooting
section: User guide
order: 14
revision: 51882c1bcfa4
---

| Symptom | Checks and remedy |
| --- | --- |
| Add-on does not appear | Check 64-bit game, correct executable/folder, ReShade add-on build/API 20 and `ReShade.log`. Older API builds cannot load it. |
| Blank inspector preview | Choose a recently written colour target. Disable Freeze Preview. Check size/format and the reported error; wait for a snapshot if a moment was just selected. Try the back buffer at end of frame. |
| Works in game, nothing in stream | The F10 view and capture are separate. Click Use As Capture Source, check active status, sent frames and output preview, then choose that exact sender in OBS. |
| Preview correct, OBS transparent/black | Disable Preserve source alpha for colour. Verify OBS source visibility/transform, correct sender, same GPU and receiver format support. HUD masks should retain alpha. |
| HUD still visible / incomplete scene | Wrong buffer or moment. Move later if objects are missing, earlier if the HUD is present. Test gameplay and menus; different effects may use different writes. |
| Wrong colours / very dark image | Intermediate HDR or data buffer, wrong colour space or depth parameters. Choose a tonemapped colour buffer; HDR float output needs an appropriate receiver. |
| Frozen output | Disable Freeze Preview for inspection, check producer running and sent-frame counters, Hold last frame in OBS, source reselection after resize and sender reconnection. |
| No sender / too many senders | Start capture and inspect errors; verify name collisions and the machine-wide sender capacity. Close unused producers. |
| AI unavailable / no answer | D3D11 only. Check helper, client executable and signed-in account. Inspect local prompt/answer files; retry with visible gameplay. Provider/network problems affect the optional analysis. |
| HUD recording has a solid background | Use FFV1 lossless 8-bit RGB. Hardware/YUV and 16-bit 4:4:4 paths do not preserve alpha. |
| Depth loses detail after recording | Inspect the decoded precision with VideoProbe. OBS’s internal conversion prevents full 16-bit unorm retention; read Spout directly when every bit matters. |
| Guide button cannot open the manual | Restore the entire Documentation/UserGuide folder next to the add-on, or reinstall OBS plugin data. Open index.html manually to check the browser association. |

For a useful issue report include product version, game/executable, graphics API, GPU, ReShade/OBS version, selected buffer/format/moment, sender name and the first failing stage (selection → output → receiver → recording). Add a screenshot and relevant log excerpt with private paths/account information removed.
