# Third-party components used by CyGameCaptureRS

| Component | Version | Licence | Location |
|-----------|---------|---------|----------|
| ReShade add-on SDK (headers only) | 6.8.0 (API 20), commit `18deaa52de0c425a78b329e9cb3c497281cd00ec` | BSD-3-Clause OR MIT | `reshade/` |
| Dear ImGui (headers only, function table is provided by ReShade at runtime) | 1.92.5 (IMGUI_VERSION_NUM 19250), commit `3912b3d9a9c1b3f17431aebafd86d2f40ee6e59c` | MIT | `imgui/` |
| Spout2 SDK (DirectX subset: SenderNames, SharedMemory, FrameCount, Utils, Copy, DirectX, SpoutDX, SpoutDX12) | commit `c2bcc12147711d12ace7d5f08e869d774d840f8a` (Sun Jul 19 14:05:47 2026 +0930) | Simplified BSD (2-clause) | `Spout2/` |

The full licence texts are kept next to each component (`reshade/LICENSE.md`, `imgui/LICENSE.txt`, `Spout2/LICENSE`, `Spout2/licence.txt`).
No source file of these components is modified; CyGameCaptureRS only compiles the subset it needs.
