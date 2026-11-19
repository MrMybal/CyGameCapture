# Third-party components used by CyGameCaptureUE

| Component | What is used | Licence | Location |
|-----------|--------------|---------|----------|
| **Spout2 SDK** (https://github.com/leadedge/Spout2) | Sender name registry, shared memory, frame counter / access mutex, utilities: `SpoutUtils`, `SpoutSharedMemory`, `SpoutSenderNames`, `SpoutFrameCount` are compiled into the plugin module. `SpoutCopy`, `SpoutDirectX`, `SpoutDX`, `SpoutDX12` are vendored for reference and future use but are **not** compiled. | Simplified BSD (2-clause) | `Spout2/` |

Full licence texts: `Spout2/LICENSE` and `Spout2/licence.txt`.

No Spout2 source file is modified. The plugin includes the `.cpp` files through thin wrappers in
`CyGameCaptureUE/Private/ThirdPartyWrappers/`, guarded by the engine's `AllowWindowsPlatformTypes` /
`PreWindowsApi` / `PostWindowsApi` / `HideWindowsPlatformTypes` headers, so no Windows macro leaks into engine code.

Spout is compiled into the module. The source-built MIT CyMCPSDK C ABI runtime and CLI are staged in `Binaries/ThirdParty/CyMCPSDK/Win64`; retain this directory in plugin builds.

The GPU work (shared texture creation, copies, D3D11On12 bridge) is implemented by CyGameCaptureUE itself on the
engine's device; the Spout SDK is only used for the CPU-side sender registry and synchronisation primitives that
define the Spout protocol.


## CyDoc native route helper

`CyDoc/cydoc_link.hpp` is the C++17 integration example from CyDoc 0.1.0, commit `6ddf9a582defb2f11fbb3f992afef6f98dcadc93`, under MIT. The original licence is retained in `CyDoc/LICENSE.txt`. The offline reader carries its own full CyDoc and product notices.

CyMCPSDK 0.7.0 and its native dependencies are compiled into the MIT C ABI runtime. See `CyMCPSDK-Notices.md` and the complete MIT texts in LICENCES. AGPL ReShade/OBS components are not linked into this runtime.
