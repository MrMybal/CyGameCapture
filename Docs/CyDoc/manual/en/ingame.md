---
id: ingame
title: Play with the selected image
description: CyGameCapture — Play with the selected image
section: User guide
order: 5
revision: 31d92938aac0
---

Enable **Show selected buffer in game** or press `F10` to replace the image displayed to the player with the selected buffer. Toggle again to restore the normal view. The shortcut can be changed with `ViewToggleKey`; zero disables it.

This is separate from sending a stream: a correct in-game image does not prove that capture has been started or that OBS has selected the right sender. Confirm the output in the Capture tab.

The selected texture must match the back buffer’s size and have a copy-compatible format. Scaling and format conversion for this particular in-game replacement are not available. Use the clean scene or a moment capture when compatible; depth and HDR intermediates may not be suitable to display directly.

```image
{"id": "m1-view-mode-no-hud", "src": "assets/m1_view_mode_no_hud.png", "alt": "Test application displaying the selected clean buffer without its HUD", "controls": "minimal"}
```

Optional clean view for the player, independent of the OBS source selection.
