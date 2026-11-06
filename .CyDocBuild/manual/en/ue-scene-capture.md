---
id: ue-scene-capture
title: Send a dedicated camera
description: Set up an existing or managed scene capture and control its extra render cost.
section: Unreal Engine
order: 20
parent: ue-sender
revision: b4aad0d9f356
---

## Configure Scene Capture {#workflow}
Choose Scene Capture on the sender. Assign an existing USceneCaptureComponent2D, or leave Scene Capture empty so the component manages one. If the capture lacks a TextureTarget, the component creates a target using Scene Capture Size and Scene Capture Format. Position and configure the capture for the view you need.

The capture renders separately from the player camera. Its visibility flags, post-processing and capture source control whether the desired scene appears. This costs a separate scene render; sharing an existing render target is cheaper when it already contains the right image.
