---
id: api-links
title: Integrate the public C++ APIs
description: Choose the public native or Unreal interfaces for your integration.
section: Developer API
order: 20
revision: 9cff5741a04b
---

## Native interfaces {#native}
[Naming and frame metadata](doc:core-api) explain the shared native conventions. The [Windows help API](doc:native-help-api#parameters) opens a page, section or image in the installed manual. Headers belong to CyGameCaptureCore and the native components retain their AGPL licence.

## Unreal interfaces {#unreal-api}
Use the CyGameCaptureUE module and its public includes. Begin with the [sender component](doc:ue-sender), [receiver component](doc:ue-receiver) or [subsystem and Blueprint helpers](doc:ue-subsystem). Keep owned objects alive, check creation failures and release directly owned streams. The Unreal plugin is MIT licensed.

These references describe supported public calls and short examples. They are not a compiler, a Blueprint editor or a promise that arbitrary private implementation details are stable.
