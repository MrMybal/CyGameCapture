---
id: core-api
title: Native stream naming and synchronisation
description: Build sender names and read frame metadata without accessing image pixels.
section: Developer API
order: 20
parent: api-links
revision: cd81c9b44de9
---

## Names and includes {#naming}
Use C++17 and include `CyGameCaptureCore/SenderNaming.hpp`. SanitizeGameName(exe_stem) removes supported shipping/debug suffixes and replaces reserved separator characters. MakeSenderName(game_name, instance) builds `CyGameCaptureRS::<Game>`; instance values above one add `::<N>`. The final name is limited to 255 bytes.

```cpp
#include "CyGameCaptureCore/SenderNaming.hpp"
const auto Game = cygc::SanitizeGameName("Demo-Win64-Shipping");
const auto Sender = cygc::MakeSenderName(Game, 2); // CyGameCaptureRS::Demo::2
```

IsReShadeSender(), IsUnrealSender() and IsCyGameCaptureSender() classify names; SenderDisplayName() returns a view without the recognised prefix. Preserve the original string while using that view.

## Read frame metadata {#synchronisation}
Include `CyGameCaptureCore/FrameSync.hpp` on Windows. `std::vector<FrameSyncRecord> ReadFrameSync()` obtains available metadata snapshots. The overload `bool ReadFrameSync(const std::string& sender_name, FrameSyncRecord& out)` fills out for an exact sender and returns false when no snapshot is available.

A record carries the sender name, frame index, producer group, timing, dimensions, DXGI format and stream kind. Equal indices are meaningful within the same producer group, not between unrelated applications. Metadata does not contain image pixels and does not synchronise independent GPU producers. Stream kind and format definitions are in `CyGameCaptureCore/StreamInfo.hpp`.
