# CyGameCapture automation integration

The independent MIT C ABI host and console companion compile CyMCPSDK 0.7.1
at revision `34d4bcae126bc83d1a2925b5047dac36e3d09d92`. CMake first snapshots
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
ticker. Free is the default profile. Tokens, conversations and optional measures
are private user data and never package inputs.

## Access profiles

The host uses the SDK's `LocalAccessProfile::free` by default, `UnauthenticatedLoopback`
HTTP server on `127.0.0.1` and `Policy::allow_all()`. URL-only CLI/MCP connections
work without a descriptor, token, confirmation flag or legacy mutation switch:

```powershell
.\CyGameCaptureCLI.exe call app.status --url http://127.0.0.1:PORT/mcp
.\CyGameCaptureCLI.exe --mcp --url http://127.0.0.1:PORT/mcp
```

Missing profile selections migrate to Free; explicit selections are retained.
Local controlled (`local_controlled`) retains host mutation permissions without a
token; Token only (`token_only`) requires a token but allows all operations;
Protected (`protected_access`) requires both token and host mutation permission.
Argument validation remains enabled for every profile. Tokens are only generated
and stored in descriptors for the two token profiles. Change profiles in the
application and restart the host to apply. The old mutation switch is retained
and ignored only in Free/Token only. The C ABI exposes public active-profile
metadata separately from private connection descriptors.

## Local checks

From a built workspace, run these scripts individually with Python 3:

```powershell
python Automation/Tests/AccessProfiles.py
python Automation/Tests/LiveCapture.py
python Automation/Tests/Conversation.py
python Automation/Tests/Peers.py
python Automation/Tests/OBS.py
```

`AccessProfiles.py` tests real-app migration, four profiles, address-only reads/mutations, token enforcement and invalid arguments. Run `Conversation.py` with `CYGC_TEST_ACCESS_PROFILE=free` and `protected_access` for both paths.

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
