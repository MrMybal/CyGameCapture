---
id: overview
title: What CyGameCapture does
description: CyGameCapture — What CyGameCapture does
section: User guide
order: 0
revision: 8885f330ff0d
---

CyGameCapture selects images inside a game’s rendering pipeline and shares them through **Spout2**. You can keep the normal game image with its interface while sending a clean scene to OBS, capture depth, isolate the HUD or send several buffers at once.

- **ReShade add-on:** inspect a running game, choose a render target and publish it.
- **OBS plugin:** receive a stream as a source and optionally record several sources into separate files.
- **Unreal plugin:** send render targets, scene captures or a viewport, and receive Spout images inside Unreal.
- **AI helper:** propose a buffer and a capture moment from images of a D3D11 frame.

Sharing and the normal capture path stay on the GPU. Encoding a recording is a separate operation; the AI helper reads small images in its own process when you request an analysis.

```diagram
{"type": "flow", "title": "The capture path", "nodes": [{"id": "0", "label": "Game + ReShade", "x": 20, "y": 20, "width": 260}, {"id": "1", "label": "Selected buffer", "x": 310, "y": 20, "width": 260}, {"id": "2", "label": "Spout texture", "x": 20, "y": 150, "width": 260}, {"id": "3", "label": "OBS / receiver", "x": 310, "y": 150, "width": 260}], "edges": [{"from": "0", "to": "1"}, {"from": "1", "to": "2"}, {"from": "2", "to": "3"}]}
```

```callout
{"type": "info", "title": "Note", "body": "The screenshots use a small animated test scene so each function is easy to identify. They are actual application captures, not mock-ups."}
```
