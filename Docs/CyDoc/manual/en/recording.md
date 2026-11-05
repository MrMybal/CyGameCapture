---
id: recording
title: Record several buffers together
description: CyGameCapture — Record several buffers together
section: User guide
order: 8
revision: 4e91f32a524b
---

The native OBS plugin can record several buffers into **separate synchronised files**, independently of an ordinary recording of the OBS canvas.

```diagram
{"type": "flow", "title": "Separate synchronised files", "nodes": [{"id": "0", "label": "Frame N", "x": 20, "y": 20, "width": 260}, {"id": "1", "label": "Scene → Clean.mkv", "x": 310, "y": 20, "width": 260}, {"id": "2", "label": "HUD → HUD.mkv", "x": 20, "y": 150, "width": 260}, {"id": "3", "label": "Depth → Depth.mkv", "x": 310, "y": 150, "width": 260}], "edges": [{"from": "0", "to": "1"}, {"from": "0", "to": "2"}, {"from": "0", "to": "3"}]}
```

1. Create one CyGameCapture OBS source per buffer and choose explicit senders.
2. Enable **Record this buffer** on each source to include it in the take.
3. Select the take folder, encoding, container and OBS audio track (1–6).
4. Click **Start recording all selected buffers** once. Stop with **Stop recording**.
5. Check each file and the status counters, skipped frames and reported drift. Files share a take time stamp and include their source/sender name.

Each source uses a video mix at its native resolution, encoder and muxer. Producers publish matching frame indices; the recorder reports differences when it can verify them. Frame synchronisation is measured, not a promise that no frame can ever be missed.

| Encoding | Use | Limits |
| --- | --- | --- |
| Hardware (NVENC / AMF / QuickSync; software fallback if selected) | Normal colour video; smaller files | YUV conversion can lose colour precision, chroma and alpha. Not appropriate for preserving depth. |
| Lossless 8-bit RGB · FFV1 | Exact 8-bit colour and isolated HUD masks | Preserves alpha; limited to 8 bits. |
| Lossless 16-bit 4:4:4 · FFV1 | Higher-precision depth | YUV, no alpha. OBS internally uses half floats, so full 16-bit unorm precision is not retained. |

Use Matroska (`.mkv`) for recoverable takes. An interrupted MP4 may be unusable. Lossless modes use FFV1 and the Matroska path; allow enough disk space and verify a short take first. An audio track is an OBS mix: configure its contents in OBS’s Advanced Audio Properties. Each file can receive the selected mix; this does not create game audio directly.

**Start the take by itself** begins when every selected source has a picture, once per OBS run. Disable it for manual control. See [verification tools](doc:diagnostics) to inspect what a file really preserved.
