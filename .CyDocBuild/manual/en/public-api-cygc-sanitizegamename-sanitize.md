---
id: public-api-cygc-sanitizegamename-sanitize
title: cygc::SanitizeGameName
description: Clean a game executable stem for a Spout sender name. Include CyGameCaptureCore/SenderNaming.hpp; C++17.
section: Developer API
parent: core-api
kind: api
order: 80
revision: 4b8f357936bf
---

Clean a game executable stem for a Spout sender name. Include CyGameCaptureCore/SenderNaming.hpp; C++17.

## Signature {#signature}

```cpp
std::string cygc::SanitizeGameName(std::string_view exe_stem)
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `exe_stem` | `std::string_view` |  | Executable name without its file extension. |

## Returns {#returns}

`std::string` — Name with supported build suffixes removed and reserved separators replaced.
