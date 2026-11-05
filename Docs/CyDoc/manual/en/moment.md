---
id: moment
title: Capture before the HUD is drawn
description: CyGameCapture — Capture before the HUD is drawn
section: User guide
order: 4
revision: b95c5230813f
---

Some games draw the finished scene and the interface into the same back buffer. At the end of the frame there is no clean target left. Select the back buffer and capture it **between those writes**.

```diagram
{"type": "flow", "title": "A moment before the HUD", "nodes": [{"id": "0", "label": "1 · Clear", "x": 20, "y": 20, "width": 260}, {"id": "1", "label": "2 · Clean scene", "x": 310, "y": 20, "width": 260}, {"id": "2", "label": "3 · HUD / menus", "x": 20, "y": 150, "width": 260}, {"id": "3", "label": "4 · Present", "x": 310, "y": 150, "width": 260}], "edges": [{"from": "0", "to": "1"}, {"from": "1", "to": "2"}, {"from": "2", "to": "3"}]}
```

Copy after 2: the stream stays clean while the game continues drawing the HUD.

1. Select the back buffer in the inspector.
2. Read **Writes of this buffer in the last frame**.
3. Move **Moment of the frame** back from the end until the preview shows the complete scene without the interface. A clear-only or partial image means you went too early.
4. Use this setting as the capture source. The preview, capture and in-game view use the selected moment.

Example: in Stray’s tested menu, `#005 after write 2` is after the tonemapper and before the interface. This is an example, not a universal setting: the ID and order depend on the game and scene. A logo drawn as part of the 3D scene remains.

```callout
{"type": "info", "title": "Note", "body": "D3D11 only. Multisampled targets cannot currently be captured at an intermediate moment. Each moment in use adds a full-sized GPU copy per frame. D3D12 writes are recorded in parallel, so this ordering feature is unavailable there."}
```
