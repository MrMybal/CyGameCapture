---
id: public-api-ucygamecapturesubsystem-createsender-create-sender
title: UCyGameCaptureSubsystem::CreateSender
description: Create an owned sender stream through the engine subsystem. Include CyGameCaptureSubsystem.h; module CyGameCaptureUE. Pair with DestroySender.
section: Developer API
parent: ue-subsystem
kind: api
order: 80
revision: ec3513d61120
---

Create an owned sender stream through the engine subsystem. Include CyGameCaptureSubsystem.h; module CyGameCaptureUE. Pair with DestroySender.

## Signature {#signature}

```cpp
FSenderStreamPtr CreateSender(const FCySenderStreamConfig& Config, UObject* Owner, FString& OutError)
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `Config` | `const FCySenderStreamConfig&` |  | Sender settings including name, source mode and timing. |
| `Owner` | `UObject*` |  | Live stream owner used for lifetime cleanup. |
| `OutError` | `FString&` |  | Output error message when creation fails. |

## Returns {#returns}

`FSenderStreamPtr` — Thread-safe shared sender pointer, or an invalid pointer on failure. Inspect OutError before use.

## References {#references}

[UCyGameCaptureSubsystem::GetAvailableSpoutSenders](doc:public-api-ucygamecapturesubsystem-getavailablespoutsenders-discover)
