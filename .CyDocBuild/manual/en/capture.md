---
id: capture
title: Streams, previews and alpha
description: Start streams, inspect output previews and choose how alpha is transmitted.
section: User guide
parent: inspector
order: 6
revision: f389891ee70b
---

**Use As Capture Source** selects the main stream. **Add As Extra Stream** publishes another buffer without replacing it. Sender names use `CyGameCaptureRS::Game`, then `::2`, `::3`, etc. Use each stream’s Stop button when it is no longer needed.

The **Capture** tab reports active status, sender, source, resolution, format, FPS, sent/skipped frames and CPU copy cost. **Output preview (what OBS receives)** displays the published texture. The inspector preview only proves what is selected, not what an existing stream is sending.

```image
{"id": "m1-capture-tab-two-streams", "src": "assets/m1_capture_tab_two_streams.png", "alt": "Capture tab with two active streams and a large output preview", "controls": "minimal"}
```

Check the sender name and output preview before configuring the receiver.

## Transparent image even though RGB is correct {#transparent-image-even-though-rgb-is-correct}

Some games leave alpha at zero in a complete colour image. Colour streams are **opaque by default**; RGB is unchanged. Enable **Preserve source alpha (advanced)** only when you actually need the source transparency. This is per stream; `PreserveSourceAlpha` sets the default for new streams. HUD-derived streams always preserve their mask. The inspector preview displays RGB opaquely, so it can look fine even when a receiver blends a preserved zero-alpha texture away.

**Flush after copy** submits D3D11 work immediately. Leave it off normally; try it when diagnosing delivery latency. It is not a repair for a wrong source or missing receiver and adds CPU work.
