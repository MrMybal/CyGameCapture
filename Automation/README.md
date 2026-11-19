# CyGameCapture automation integration

The independent MIT C ABI host and console companion compile CyMCPSDK 0.7.0
at revision `791d4d7d7680213b282ec8529d248a10f8494d77`. CMake first snapshots
that commit from `D:/_Project/CyMCPSDK`, preserving concurrent checkout edits.
Only when that checkout is unavailable does it fetch the pinned Git revision.
Set `CYGC_CYMCP_SOURCE_DIR` to select another local checkout containing the pin.

Core, Platform, Http, MCP and AI are enabled. Local engine installation, model
weights, CyUI and ChatUI are excluded. C++20 stays inside the native SDK runtime
and helper; the ReShade/OBS C++17 code and Unreal use the UTF-8 JSON C ABI.
CyUEMCPSDK 0.2.3 (`fd4846e`) was an integration reference, not a dependency.

`build.cmd Release` builds and stages the runtime, CLI and existing components.
Build this native runtime before building/staging Unreal. Complete product
usage, operations, permissions, provider settings and transport limitations
are in the bundled CyDoc reader's **Assistant, terminal and MCP control** page
(`automation`, in English and French).

The authoritative registries live in each application instance. CLI/MCP relays
copy schemas and forward calls, never reimplement capture behavior. ReShade
pumps calls during presentation, OBS on its video tick and Unreal on its game
ticker. Read-only is the default. Tokens, conversations and optional measures
are private user data and never package inputs.

## Local checks

From a built workspace, run these scripts individually with Python 3:

```powershell
python Automation/Tests/LiveCapture.py
python Automation/Tests/Conversation.py
python Automation/Tests/Peers.py
python Automation/Tests/OBS.py
```

`LiveCapture.py` uses the actual console companion and D3D11 application.
`Conversation.py` runs the real AI helper against a controlled local API fixture,
including a live game tool call. `Peers.py` checks explicit peer access, typed
business errors and primary-host denial. `OBS.py` uses the installed libobs,
the actual source plugin and a real D3D12-to-Spout-to-OBS transfer. All application
configuration is isolated under `build/`; vendor accounts are not used.

Build the excluded `CyGameCaptureTransportProbe` target using the same MSVC
environment as `build.cmd`, then run `python Automation/Tests/NativeRuntime.py`.
This validates chunk retrieval/expiry, grep, HTTP subscriptions and cache
invalidation, private measures, context reduction, attachments and native session
restoration. `Unreal.py` is an editor Python test for an isolated project with
PythonScriptPlugin and EditorScriptingUtilities enabled. It needs a normal editor
tick (`-ExecCmds="py <absolute path>/Automation/Tests/Unreal.py"`), not a blocking
Python commandlet. Set `CYGC_TEST_ROOT`, `CYGC_TEST_OUTPUT` and isolated user data
directories before launch. Python is a test dependency, not a shipped runtime.

`Tools/MakeRelease.ps1` and `Tools/MakeTestPackage.ps1` validate the actual
distributed binaries, complete licenses, reader resources and absence of private
user data. They neither publish releases nor push Git changes.
