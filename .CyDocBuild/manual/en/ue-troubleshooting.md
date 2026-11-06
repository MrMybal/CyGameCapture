---
id: ue-troubleshooting
title: Fix an Unreal stream
description: Diagnose name collisions, GPU mismatch, resizing and HDR receiver problems.
section: Unreal Engine
order: 20
parent: unreal
revision: 6a1b80fa126d
---

## Check support and names {#diagnosis}
Run CyGameCapture.Spout.List to see the actual producer names. Check IsSpoutSupported and GetUnsupportedReason. Both applications must use the same GPU adapter. Use -dx11 or -dx12 on Windows; Vulkan, other platforms and NullRHI cannot share Spout textures.

| Symptom | Action |
| --- | --- |
| No sender | Enter Play, enable the plugin and check Auto Start and the source object. |
| Name gains ::2 | Another stream owns the requested name; select the actual name or choose a different one. |
| Receiver waits | Check exact SenderName and automatic reconnect. |
| Frozen output after resizing | Enable automatic target resizing or supply a matching target. |
| HDR colours look wrong | Use a compatible HDR workflow or an SDR source; copying does not tonemap. |
| Editor panels appear | Use New Editor Window or Standalone instead of embedded PIE. |

CyGameCapture.ListSenders, CyGameCapture.ListReceivers and CyGameCapture.Debug 1 help locate the affected stream. Turn verbose diagnostics off again when finished. Inspect the component's OnError event before changing unrelated settings.
