---
id: derived
title: Depth and isolated HUD
description: Produce linearised depth or an isolated HUD with an alpha mask.
section: User guide
parent: inspector
order: 10
revision: e996709e8983
---

## Depth {#depth}

Select a depth texture and click **Use As Depth Stream**. CyGameCapture linearises it into `R16G16B16A16_UNORM`. Adjust **far plane**, **reversed Z**, **logarithmic depth** and **upside down** for the game, as with ReShade’s depth settings. Incorrect parameters produce an inverted, almost white or geometrically wrong result. Select a main scene depth buffer, not a shadow map. Verify near/far changes as you move.

## HUD on its own {#hud-on-its-own}

1. Capture the clean scene as stream 1, either from a separate target or an intermediate moment.
2. Select the finished image at the end of the frame, usually the back buffer.
3. Click **Isolate HUD Against Stream 1**.
4. Adjust `HudThreshold` and `HudSoftness` to reject scene differences without erasing text. `HudKeepFinalColour` keeps the final interface colours.

The derived HUD is the difference between the final image and the clean scene with a transparency mask. Opaque HUD areas can be exact; soft edges and effects are approximations. Motion, differing post-processing or a wrong moment can leave scene artefacts. When the game already has a transparent HUD-only render target, capture it directly and preserve its alpha instead.

Both derived passes work on D3D11 and D3D12. Their availability does not make D3D12 intermediate-moment capture available. To record a HUD with transparency use lossless 8-bit RGB; the other recording paths drop alpha.
