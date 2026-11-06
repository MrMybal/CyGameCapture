---
id: unreal
title: Unreal Engine sender and receiver
description: Send Unreal targets and cameras or receive a Spout texture in a material.
section: User guide
order: 11
revision: 293a15c713e4
---

Install the package matching your Unreal version into `<Project>\Plugins\CyGameCaptureUE`, enable the plugin and restart. Supplied prebuilt archives cover the engine versions named on those archives. Source compatibility targets UE 4.27–5.8; use the version labelled on your prebuilt archive. Win64 D3D11 and D3D12 are the supported RHIs. The offline manual is also at `Documentation/Documentation.html` in the plugin.

## Send an image {#send-an-image}

1. Add a **CyGameCapture Spout Sender** component to an actor.
2. Choose **Render Target** (assign a `UTextureRenderTarget2D`), **Scene Capture** (use an existing component or leave empty to create one), or **Game Viewport** (no second scene render).
3. Choose a stream name and keep **Auto Start** on. Begin Play publishes `CyGameCaptureUE::Project::Stream`.
4. Select that sender in OBS or another Spout receiver.

Managed scene captures expose size/format settings and cost another scene render. Custom frame rate limits GPU copies. Supported targets include RGBA8, RGBA8 sRGB, RGB10A2, RGBA16f and RGBA32f. Other formats raise an error; HDR float images need a suitable receiver or conversion to SDR. Name collision policies are Fail, Auto Rename or Replace If Owned By This Process. Do not give multiple live streams the same fixed name.

## Receive an image {#receive-an-image}

1. Add a **CyGameCapture Spout Receiver** component.
2. Set the exact sender name; **Get Available Spout Sender Names** lists candidates.
3. Keep **Internal Render Target** and **Auto Connect** on for a managed output. User Render Target mode needs your own target and optional automatic resizing.
4. On **On Connected**, get **Get Receiver Texture** and assign it to a dynamic material’s texture parameter. For UMG, use a UI material on an Image brush.

Events report connection, disconnection, frame arrival, size, format and errors. Frame events can be throttled. Resize handling preserves the render target object but may recreate its underlying texture. Streams stop on EndPlay/world teardown.

## Settings and diagnosis {#settings-and-diagnosis}

Project Settings → Plugins → CyGameCapture controls defaults, editor capture and limits. PIE in the main editor viewport captures the whole editor window back buffer; use New Editor Window or Standalone for an isolated game view.

```text
CyGameCapture.ListSenders
CyGameCapture.ListReceivers
CyGameCapture.Spout.List
CyGameCapture.Debug 1
```

Blueprint components expose Start/Stop, source setters, connection state and statistics. C++ uses `UCyGameCaptureSubsystem`, `CreateSender`, `CreateReceiver` and matching destroy calls. See the [sender](doc:ue-sender), [receiver](doc:ue-receiver) and [subsystem](doc:ue-subsystem) references.


## Local help {#local-help}

Use **Help → CyGameCapture user guide** in the editor, or `CyGameCapture.Help`. The current editor language selects English or French. `CyGameCapture.Help fr url` prints the encoded local Unreal-page URL for verification without opening a browser.
