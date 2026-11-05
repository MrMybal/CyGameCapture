---
id: performance
title: Performance and compatibility
description: CyGameCapture — Performance and compatibility
section: User guide
order: 13
revision: 5f1d4067c90b
---

- Normal streams require one GPU copy per frame; D3D12 uses a D3D11On12 bridge with additional work. Derived streams and moments add GPU passes/copies. CPU copy cost in the UI is not total GPU or encoder cost.
- Resolution, number of streams, recording encoders and disk bandwidth affect throughput. Start with one stream and add others while checking FPS, skipped frames and GPU load.
- There is no fixed product stream count. Spout’s machine-wide sender table has 64 entries by default, shared with other applications. Stop unused producers. Advanced users can change `HKCU\Software\Leading Edge\Spout\MaxSenders` through Spout’s settings. Receivers do not consume sender slots.
- Safety limits such as the add-on’s 256-stream sanity limit and Unreal’s StreamSanityLimit protect against runaway creation. They are not a performance guarantee.
- A float HDR/normal/lighting target is not an SDR display image. Pick the tonemapped scene or arrange conversion in the producing application. Format and resolution compatibility matter for every receiver.
- Textures can disappear after a mode change. Refresh, reselect and test after resizing, changing graphics settings or switching scenes.

| Feature | D3D11 | D3D12 |
| --- | --- | --- |
| Colour Spout capture, multiple streams | Yes | Yes, bridge |
| Depth and derived HUD | Yes | Yes |
| Intermediate write moment | Yes, non-MSAA | No |
| AI buffer search | Yes | No |
