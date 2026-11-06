---
id: ue-getting-started
title: Start capturing in Unreal
description: Install the plugin and publish a first named camera or render target.
section: Unreal Engine
order: 20
parent: unreal
revision: d08ce07eccac
---

## Install and start {#start}
1. Close the editor and copy the matching CyGameCaptureUE package to `<Project>/Plugins/CyGameCaptureUE`.
2. Enable the plugin, restart and select D3D11 or D3D12 on Windows. Spout does not work with Vulkan or NullRHI.
3. Add a **CyGameCapture Spout Sender** component to an actor. Give Stream Name a useful name such as CameraA.
4. Assign a render target or choose Scene Capture. Keep Auto Start enabled and enter Play.
5. In OBS, add a CyGameCapture source and select the actual published name.

## Choose an image {#source}
[Render targets](doc:ue-render-target) publish an existing texture. [Scene captures](doc:ue-scene-capture) render a camera separately. [Viewport capture](doc:ue-viewport) shares the final window image, including its UI. A stream name normally follows `CyGameCaptureUE::<Project>::<Stream>`; read Get Active Sender Name after startup because a collision may add a suffix.

## Stop cleanly {#stop}
Stop Sender or leave Play to release the stream. A sender component owns one stream; use several components for independent views. See [sender controls](doc:ue-sender), [receiving](doc:ue-receiver) and [troubleshooting](doc:ue-troubleshooting).
