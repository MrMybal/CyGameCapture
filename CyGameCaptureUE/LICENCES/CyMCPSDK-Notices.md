# CyMCPSDK integration

CyMCPSDK 0.7.0, source revision 791d4d7d7680213b282ec8529d248a10f8494d77.
Source: https://github.com/MrMybal/CyMCPSDK (access may require permission).
Built from the local checkout when present; the pinned Git revision is the fallback.

Compiled modules: Core, Platform, Http, MCP, AI. No Local engine installer, CUDA runtime,
model weights, CyUI or ChatUI is bundled. The existing ReShade ImGui ABI and Unreal Slate
panels use the SDK Conversation service. Local models use an already running compatible API.

The independent MIT adapter and console companion are in Automation/. The native ReShade,
AI helper, Core and OBS product components retain AGPL-3.0-or-later; Unreal remains MIT.
No AGPL product library is linked into Unreal or into the MIT automation adapter.

Bundled native SDK dependencies:
- nlohmann/json 3.12.0: MIT, nlohmann-json-MIT.txt.
- cpp-httplib 0.66.0: MIT, cpp-httplib-MIT.txt.
- PicoSHA2 161cb3fc4170fa7a3eca9e582cebd27cc4d1fe29: MIT, PicoSHA2-MIT.txt.

Retain CyMCPSDK-MIT.txt and CyGameCaptureAutomation-MIT.txt with these notices.
Windows HTTP, crypto and process functions use operating-system libraries. Provider clients
and model services are separately installed under their own terms; no Python runtime is included.
