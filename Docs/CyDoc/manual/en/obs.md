---
id: obs
title: Receive and compose in OBS
description: CyGameCapture — Receive and compose in OBS
section: User guide
order: 7
revision: da80bead8304
---

1. Start the game’s capture first.
2. In OBS, Sources → + → **CyGameCapture**. Name the source by purpose, such as Clean, HUD or Depth.
3. Choose the exact sender. **Refresh the list** re-reads discovery. Enable **List every Spout sender** to include non-CyGameCapture applications.
4. Check status, resolution and FPS. Fit the source to the canvas and ensure it is visible in the current scene.

**Automatic** selects an available CyGameCapture sender; choose an explicit name for predictable multi-stream setups. **Reconnect automatically** reconnects when the selected sender returns. **Hold the last frame** retains the last picture during a missed transfer; it can look like a freeze while the producer is stopped. A previously selected sender may remain listed as not running.

Spout sharing is local to this PC. Use the same GPU for the game and OBS, especially on laptops with integrated and dedicated graphics. A visible sender name proves registration, not a healthy image. Use the output preview and the diagnostic receiver to isolate the problem.

For composition, put an isolated HUD source above the clean source. Transparent pixels require an alpha-preserving stream and a compatible recording mode if saving that layer. Separate sources can be different sizes: check their transforms.
