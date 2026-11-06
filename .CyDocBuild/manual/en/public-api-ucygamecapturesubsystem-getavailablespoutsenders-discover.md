---
id: public-api-ucygamecapturesubsystem-getavailablespoutsenders-discover
title: UCyGameCaptureSubsystem::GetAvailableSpoutSenders
description: Discover Spout producers across applications. Include CyGameCaptureSubsystem.h; module CyGameCaptureUE.
section: Developer API
parent: ue-subsystem
kind: api
order: 80
revision: a9395c9bbb96
---

Discover Spout producers across applications. Include CyGameCaptureSubsystem.h; module CyGameCaptureUE.

## Signature {#signature}

```cpp
TArray<FCySpoutSenderInfo> GetAvailableSpoutSenders(bool bForceRefresh = false)
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `bForceRefresh` | `bool` |  | Refresh discovery immediately instead of returning the cached list; default false. |

## Returns {#returns}

`TArray<FCySpoutSenderInfo>` — Sender information array; an empty array means none is currently known.
