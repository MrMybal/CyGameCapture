---
id: public-api-ucygamecapturesubsystem-createreceiver-create-receiver
title: UCyGameCaptureSubsystem::CreateReceiver
description: Create an owned receiver stream. Include CyGameCaptureSubsystem.h; module CyGameCaptureUE. Pair with DestroyReceiver and supply a destination target.
section: Developer API
parent: ue-subsystem
kind: api
order: 80
revision: 5ebde36dfc90
---

Create an owned receiver stream. Include CyGameCaptureSubsystem.h; module CyGameCaptureUE. Pair with DestroyReceiver and supply a destination target.

## Signature {#signature}

```cpp
FReceiverStreamPtr CreateReceiver(const FCyReceiverStreamConfig& Config, UObject* Owner, FString& OutError)
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `Config` | `const FCyReceiverStreamConfig&` |  | Exact sender name, receiver name and connection settings. |
| `Owner` | `UObject*` |  | Live stream owner. |
| `OutError` | `FString&` |  | Output error message when creation fails. |

## Returns {#returns}

`FReceiverStreamPtr` — Thread-safe shared receiver pointer, or an invalid pointer on failure.

## References {#references}

[UCyGameCaptureSubsystem::GetAvailableSpoutSenders](doc:public-api-ucygamecapturesubsystem-getavailablespoutsenders-discover)
