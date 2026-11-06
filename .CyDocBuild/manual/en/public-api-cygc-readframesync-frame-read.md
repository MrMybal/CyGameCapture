---
id: public-api-cygc-readframesync-frame-read
title: cygc::ReadFrameSync
description: Read the current metadata snapshot for one Spout name. Include CyGameCaptureCore/FrameSync.hpp on Windows. This does not read pixels.
section: Developer API
parent: core-api
kind: api
order: 80
revision: 3d42bb3b8e2f
---

Read the current metadata snapshot for one Spout name. Include CyGameCaptureCore/FrameSync.hpp on Windows. This does not read pixels.

## Signature {#signature}

```cpp
bool cygc::ReadFrameSync(const std::string& sender_name, cygc::FrameSyncRecord& out)
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `sender_name` | `const std::string&` |  | Exact Spout sender name. |
| `out` | `cygc::FrameSyncRecord&` |  | Output record, used only when true is returned. |

## Returns {#returns}

`bool` — true and a filled output record when a snapshot exists; otherwise false.
