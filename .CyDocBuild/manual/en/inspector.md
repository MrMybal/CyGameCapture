---
id: inspector
title: Find and inspect buffers
description: Find a persistent render target and verify its image, format and write timing.
section: User guide
order: 3
revision: f3cfd0e7fcdf
---

A **buffer / render target** is a texture used by the game while drawing a frame. The final display image is the **back buffer**. Many other targets contain intermediate data such as normals, lighting or depth: an image resembling the scene is not necessarily ready to display.

The **Contact Sheet** shows thumbnail previews. Click one to select it. The table lists its ID, size, format, writes and **Steady** percentage. A good persistent source is written almost every frame. **Steady buffers only** hides temporary resources; disable it when investigating a missing target.

- Use filters for recently written textures, colour, depth, MSAA, mip/array resources, size relative to the back buffer and format. Change sorting if necessary.
- Use **Previous / Next**, the arrow keys or a row click to browse. **Freeze Preview** freezes inspection; it does not pause the game.
- Check the full image: no missing objects, HUD, blur, unexplained colours or stale borders. Prefer the final tonemapped colour for normal SDR capture.
- Dynamic resolution can render into only part of a full-sized texture. The overlay reports the active rendered area and the capture scales it to a stable stream size.
- Buffer IDs are stable only within a run. Recheck them after restarting the game or changing graphics settings.

```image
{"id": "m1-buffer-inspector", "src": "assets/m1_buffer_inspector.png", "alt": "Buffer Inspector with resource list, format, resolution, writes and live image preview", "controls": "minimal", "annotations": [{"id": "buffer-list", "type": "rect", "x": 0.49, "y": 0.34, "toX": 0.97, "toY": 0.47, "label": "Buffer list"}]}
```

Resource details and GPU preview. Recent versions also include the contact sheet and steadiness filters.

## Timeline and Debug {#timeline-and-debug}

**Timeline** lists clear, draw, copy, resolve and present events in the last frame. Filter to writes or the selected resource; clicking an event selects its resource. **Debug** shows backend, frame index and tracker counts. Enable verbose tracking only while investigating: logging every resource can produce large logs.

```image
{"id": "m1-timeline", "src": "assets/m1_timeline.png", "alt": "Timeline listing resource writes and the final present event", "controls": "minimal"}
```

The write order explains which image exists at each moment.
