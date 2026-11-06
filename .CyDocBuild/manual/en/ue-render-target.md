---
id: ue-render-target
title: Send an existing render target
description: Share a texture already produced by Unreal without rendering a second camera.
section: Unreal Engine
order: 20
parent: ue-sender
revision: ce7b309068af
---

## Share an existing texture {#workflow}
Create or select a UTextureRenderTarget2D, then set the sender's Source Mode to Render Target and assign that object. Start the sender after the source is available. This copies the existing texture without rendering the scene a second time.

Use RGBA8 for ordinary SDR output. Float HDR textures require a compatible receiver; a copy does not perform tonemapping. The [sender API](doc:ue-sender) lists supported source formats. If the image is empty, first display the render target inside Unreal and check that your camera, Canvas or material actually writes to it.
