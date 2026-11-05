---
id: quickstart
title: Your first clean capture
description: CyGameCapture — Your first clean capture
section: User guide
order: 2
revision: 95e820f2619c
---

1. Reach actual gameplay with a visible HUD. Avoid a black loading screen.
2. Open the overlay with `Home`, then **Buffer Inspector**.
3. Choose a complete, correctly coloured image without the HUD in the contact sheet. Inspect it in the preview. If none exists, try [a moment before the HUD](doc:moment) or the [AI assistant](doc:assistant).
4. Click **Use As Capture Source**. In **Capture**, check the stream’s resolution, FPS, sent frames and output preview.
5. In OBS, add **CyGameCapture** and select `CyGameCaptureRS::<Game>`. Use Refresh if necessary.
6. Close the ReShade overlay. Keep your usual HUD in the game while OBS receives the selected scene.

```image
{"id": "player-and-output", "src": "assets/m1_player_view_with_hud.png", "compare": "assets/m1_spout_receiver_clean_scene.png", "alt": "Test game with HUD bars and a blue interface panel", "labels": ["Player with HUD", "Spout output"], "controls": "minimal"}
```

Player view: the HUD remains visible.

Spout output: clean scene. The animated scene was captured at a different moment.
