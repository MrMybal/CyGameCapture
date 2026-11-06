---
id: public-api-cygc-makesendername-sender-name
title: cygc::MakeSenderName
description: Build a ReShade sender name from a sanitised game name. Include CyGameCaptureCore/SenderNaming.hpp.
section: Developer API
parent: core-api
kind: api
order: 80
revision: 07b7458b2068
---

Build a ReShade sender name from a sanitised game name. Include CyGameCaptureCore/SenderNaming.hpp.

## Signature {#signature}

```cpp
std::string cygc::MakeSenderName(std::string_view game_name, unsigned int instance = 1)
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `game_name` | `std::string_view` |  | Sanitised game name. |
| `instance` | `unsigned int` |  | Instance index; default 1. |

## Returns {#returns}

`std::string` — CyGameCaptureRS::<Game>, with ::N for instances above one; at most 255 bytes.

## References {#references}

[cygc::SanitizeGameName](doc:public-api-cygc-sanitizegamename-sanitize)
