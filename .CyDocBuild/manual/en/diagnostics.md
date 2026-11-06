---
id: diagnostics
title: Test and verify a recording
description: Use the supplied receiver and decoder to inspect a saved image or recording.
section: User guide
parent: obs
order: 15
revision: cb201c3a28a9
---

The test suite includes D3D11/D3D12 applications with a simple scene and fake HUD, plus a Spout receiver. It lets you separate an installation problem from a game-specific buffer problem.

```text
CyGameCaptureSpoutReceiverTest.exe --list
CyGameCaptureSpoutReceiverTest.exe --frames 3 --out C:\Captures
```

Run with an existing writable output folder. The receiver lists CyGameCapture senders and writes received frames as BMP. If it receives the correct image, investigate the OBS source/format rather than buffer selection.

The optional **VideoProbe** decodes a saved file and reports codec, pixel format, component bit depth and distinct values:

```text
CyGameCaptureVideoProbe.exe "take.mkv" 2
CyGameCaptureVideoProbe.exe "take.mkv" 2 --dump 3 "C:\Captures\Alpha"
```

Component indices depend on the decoded pixel format; component 3 is alpha only if that format actually contains alpha. The tool uses FFmpeg libraries installed with OBS. Absence of alpha in the decoded format means it was not saved, even if the incoming Spout texture had one.
