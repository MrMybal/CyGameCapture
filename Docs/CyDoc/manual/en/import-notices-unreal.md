---
id: notices-unreal
title: Unreal third-party notices
description: CyGameCaptureUE/Source/ThirdParty/THIRD_PARTY_NOTICES.md
section: Licences
order: 192
revision: 61ac7bb888b6
---

# Third-party components used by CyGameCaptureUE {#third-party-components-used-by-cygamecaptureue}

| Component | What is used | Licence | Location |
|-----------|--------------|---------|----------|
| **Spout2 SDK** (https://github.com/leadedge/Spout2) | Sender name registry, shared memory, frame counter / access mutex, utilities: `SpoutUtils`, `SpoutSharedMemory`, `SpoutSenderNames`, `SpoutFrameCount` are compiled into the plugin module. `SpoutCopy`, `SpoutDirectX`, `SpoutDX`, `SpoutDX12` are vendored for reference and future use but are **not** compiled. | Simplified BSD (2-clause) | `Spout2/` |

Full licence texts: `Spout2/LICENSE` and `Spout2/licence.txt`.

No Spout2 source file is modified. The plugin includes the `.cpp` files through thin wrappers in
`CyGameCaptureUE/Private/ThirdPartyWrappers/`, guarded by the engine's `AllowWindowsPlatformTypes` /
`PreWindowsApi` / `PostWindowsApi` / `HideWindowsPlatformTypes` headers, so no Windows macro leaks into engine code.

No prebuilt library, DLL or absolute path is used: `RunUAT BuildPlugin` produces a self-contained plugin.

The GPU work (shared texture creation, copies, D3D11On12 bridge) is implemented by CyGameCaptureUE itself on the
engine's device; the Spout SDK is only used for the CPU-side sender registry and synchronisation primitives that
define the Spout protocol.


## CyDoc native route helper {#cydoc-native-route-helper}

`CyDoc/cydoc_link.hpp` is the C++17 integration example from CyDoc 0.1.0, commit `774f2a7eabf7dd181a98ffc5add115b580b67f42`, under MIT. The original licence is retained in `CyDoc/LICENSE.txt`. The offline reader carries its own full CyDoc and product notices.
