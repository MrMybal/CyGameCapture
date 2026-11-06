---
id: ue-viewport
title: Share the game viewport
description: Share the final window image and choose the correct PIE or standalone window.
section: Unreal Engine
order: 20
parent: ue-sender
revision: 2d3d79d68c42
---

## Capture the final window image {#workflow}
Choose Game Viewport or call UseGameViewport(). No render target is required. The sender copies the final back buffer and does not render a second camera. UI already composited into that window remains in the stream; choose Scene Capture or an existing clean render target to produce a separate view.

Embedded PIE uses the editor window's back buffer, which can include editor panels. Use New Editor Window or Standalone for an isolated game view. Editor capture also depends on the plugin's project settings. Resize events can change the stream size; check the receiver after switching display mode.
