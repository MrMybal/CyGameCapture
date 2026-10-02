# Third-party components used by CyGameCaptureUE

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
