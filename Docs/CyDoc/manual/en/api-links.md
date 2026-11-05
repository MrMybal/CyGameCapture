---
id: api-links
title: API and public source index
description: Native C++ and Unreal interfaces actually distributed with the product
section: API
order: 90
revision: 1
---

## Native interfaces {#native}

- [Windows contextual help API](doc:native-help-api#parameters), extracted from the actual C++ header annotation.
- [Frame synchronisation](doc:source-cygamecapturecore-include-cygamecapturecore-framesync-hpp#source).
- [Sender naming](doc:source-cygamecapturecore-include-cygamecapturecore-sendernaming-hpp#source).
- [Stream metadata](doc:source-cygamecapturecore-include-cygamecapturecore-streaminfo-hpp#source).

## Unreal Engine {#unreal-api}

- [Sender properties, events and code examples](doc:ue-sender).
- [Receiver properties, events and code examples](doc:ue-receiver).
- [Subsystem header](doc:source-cygamecaptureue-source-cygamecaptureue-public-cygamecapturesubsystem-h#source).
- [Blueprint function library header](doc:source-cygamecaptureue-source-cygamecaptureue-public-cygamecaptureblueprintlibrary-h#source).

The source index scans only the distributed Core public headers and Unreal Public directory. It preserves file contents and comments, generates include/dependency navigation and line anchors, and does not resolve C++ overloads, macros or Unreal reflection semantics. No Doxygen database, TypeDoc project, private implementation, Unreal Engine source, third-party SDK source or user settings are indexed. C# Build.cs is build configuration rather than a user-facing C# API.
