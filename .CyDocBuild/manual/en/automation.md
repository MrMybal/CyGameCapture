---
id: automation
title: Assistant, terminal and MCP control
description: Connect to a live instance, discover its tools and control capture with the same permissions.
section: User guide
parent: assistant
order: 10
revision: 9e33cf3a9732
---

The November 20, 2026 update of the 0.1.0 Beta packages includes this integration and the complete English/French offline manual. Older downloads dated November 6 do not include CyMCPSDK: replace the binaries, helper, runtime, documentation and licenses together. Capture works without an AI account; no model, Python runtime or inference engine is installed by the application.

## Connect to the correct instance {#connect}

Keep `CyGameCaptureMCP.dll` beside the ReShade add-on and beside `CyGameCaptureOBS.dll`. Keep `CyGameCaptureAI.exe` beside the add-on. `CyGameCaptureCLI.exe` is the console companion. Unreal includes the MIT SDK runtime and console companion in `Binaries/ThirdParty/CyMCPSDK/Win64`; it does not depend on the AGPL ReShade helper.

Start the game, create an OBS CyGameCapture source, or open an Unreal project with the plugin enabled. Each instance writes a private connection descriptor under `%LOCALAPPDATA%\CyGameCapture\Automation`. The filename includes its component and process ID. ReShade/OBS/Unreal also log the descriptor's path. The descriptor contains an authentication token: do not publish or attach it to a bug report. It is removed on orderly shutdown. Stale descriptors after a crash are harmless; choose a live instance again.

```powershell
.\CyGameCaptureCLI.exe instances
.\CyGameCaptureCLI.exe list --instance 'C:\path\to\reshade-PID-random.json'
.\CyGameCaptureCLI.exe describe captures.start --instance 'C:\path\to\reshade-PID-random.json'
.\CyGameCaptureCLI.exe call app.status --instance 'C:\path\to\reshade-PID-random.json'
```

Calls need the live renderer, OBS source or Unreal world. The companion does not start a game, load an asset or create a capture scene for you. Listing providers and printing help/version do not require a GUI or a running instance.

## Permissions and execution {#permissions}

Remote control is **read only by default**. Enable it yourself in the ReShade assistant settings, the OBS source's **Allow MCP control of this source** property, or Unreal's assistant checkbox / `CyGameCapture.Automation.AllowMutations 1`. The same permission gates CLI, MCP and chat tools on every call. There is no `--confirm` bypass and no tool that enables its own permission. ReShade's existing Show/Apply proposal buttons remain available for manual decisions.

ReShade executes business calls during presentation, OBS on its source video tick, and Unreal on its game ticker. Network and provider workers do not manipulate GPU or engine objects. A command waiting more than 15 seconds for its host tick fails and is cancelled; it is not replayed when the host resumes. A failure can still occur after an action has begun, so read the state before deciding to repeat a mutation.

## Available business operations {#operations}

Use `describe NAME` for the authoritative input and output schemas. IDs and component paths refer to the current instance. No arbitrary shell, filesystem, asset-loading or source-code execution tool is exposed.

| Host | Read operations | Operations requiring control |
| --- | --- | --- |
| ReShade | `app.status`, `buffers.list`, `buffers.inspect`, `captures.list`, `timeline.list` | `buffers.select`, `buffers.moment`, `captures.start`, `captures.stop`, `captures.alpha`, `view.set`, `inspector.filters`, `assistant.cancel` |
| OBS source | `source.status`, `senders.list` | `source.sender`, `recording.start`, `recording.stop` |
| Unreal | `app.status`, `senders.list`, `streams.list`, `components.list` | `streams.stop_all`, `sender.start`, `sender.stop`, `sender.name`, `receiver.start`, `receiver.stop`, `receiver.sender` |

ReShade buffer/timeline lists and OBS/Unreal discovery lists accept `offset` and `limit` (maximum 128). Buffer discovery additionally filters width, height and exact format. Follow `next_offset` until it is null. Capture lists are bounded by the application's stream slots. OBS take operations act on **all recording-enabled CyGameCapture sources in that OBS process**, using their existing encoder and output-folder settings. They do not change those settings. Other OBS plugins and scenes are outside the registry.

ReShade `captures.start` supports `colour`, `depth` and `hud` transforms, an optional extra slot, secondary buffer and write moments. Nonzero moments require D3D11. Unreal operations address already existing capture components in open worlds; inspect `components.list` first. A missing component or unsupported renderer is an error. Settings, scene creation, file export, arbitrary image readback and asset editing remain in their existing interfaces.

```powershell
.\CyGameCaptureCLI.exe call buffers.list --args '{"limit":16,"width":1280}' --instance 'C:\path\to\connection.json'
# After selecting a real ID from that response and enabling control:
.\CyGameCaptureCLI.exe call captures.start --args '{"id":42}' --instance 'C:\path\to\connection.json'
.\CyGameCaptureCLI.exe call captures.stop --args '{"all":true}' --instance 'C:\path\to\connection.json'
```

`--args @C:\path\arguments.json` reads UTF-8 JSON from a file. Results/errors are JSON on stdout, diagnostics go to stderr. Exit codes: 0 success, 2 invalid arguments/JSON/schema, 3 permission refused, 1 other failures. `--help` prints readable help; `--version` prints application and SDK provenance. The legacy `CyGameCaptureAI.exe request.json` and `CyGameCaptureAI.exe --cli ...` paths remain supported, as do the existing test-app launch options and Unreal console commands.

## Integrated conversation {#conversation}

The D3D11 ReShade assistant uses its existing ImGui discussion window with SDK Conversation. Select Codex, Claude Code, OpenCode or `openai-compatible`; automatic selection still detects installed native clients. Native agents use your existing sign-in and native context management. To use LM Studio, Ollama's compatible `/v1` endpoint or another compatible service, start it yourself, enter its API URL and exact model ID. A local endpoint such as `http://127.0.0.1:1234/v1` is the default; no model is downloaded. `CyGameCaptureCLI providers --discover` reports available provider profiles without exposing credentials.

The answer appears progressively. **Cancel** interrupts generation without applying a pending proposal; errors appear in the discussion. Current-frame images retain their existing attachment workflow. Complete SDK sessions and readable discussion history are saved under `%LOCALAPPDATA%\CyGameCapture\AI\<game>`; reopening restores the text, but old buffer proposals are not restored as executable actions. **New discussion** archives the SDK checkpoint and starts again. These files contain private game images, prompts and tool results.

Unreal adds **Help → CyGameCapture assistant**, using a Slate panel and the same SDK registry. It supports `codex` and `openai-compatible`, streaming, Cancel, New conversation, restored history and an optional absolute-path image/text attachment. API credentials are read from `CYGC_AI_API_KEY`. Start a new conversation after changing provider/model/endpoint settings. The API context budget can change between turns. API context budgets are in serialized bytes, not tokens: 0 keeps full history; a smaller budget drops older complete turns while preserving system instructions, the active turn and tool/result pairs. The full history stays saved. A budget too small for the active turn produces an explicit error.

OBS remains a native libobs source plugin without a separate Qt chat dock. Use an external MCP assistant or explicitly allow its tools in the ReShade chat. This avoids adding a second GUI framework to the OBS module.

## External MCP clients and peer tools {#mcp}

```powershell
.\CyGameCaptureCLI.exe mcp-config --instance 'C:\path\to\connection.json'
.\CyGameCaptureCLI.exe --mcp --instance 'C:\path\to\connection.json'
```

The first command prints a client configuration containing the executable and descriptor path; the second is a stdio MCP relay. The relay exposes tools only. The native loopback HTTP `/mcp` endpoint also exposes resources and, for compatible clients, change subscriptions. Both use the same registry and per-instance permission. The native authenticated `/v1` endpoint retains full discovery when compact MCP is selected (`--full` for list/describe/call/stdio). The SDK negotiates MCP 2026-07-28 and legacy 2025-11-25 / 2025-06-18. Legacy stdio clients use initialize; HTTP requires the matching protocol/session headers. Prefer the supplied relay or SDK client over handcrafted requests. Tokens and private descriptor files are local credentials, not a public remote service.

ReShade's Codex/API chat optionally loads a user-selected **External tools allowlist file** (`AIExternalToolsFile`). Only the named tools are imported with their exact schemas. Example, for another local CyGameCapture OBS instance:

```json
[
  {
    "connection": "C:/private/obs-source-PID-random.json",
    "prefix": "obs",
    "tools": ["source.status", "senders.list"],
    "allow_mutations": false,
    "notifications": false
  }
]
```

Writes require an explicit peer grant, current primary-application control permission and the peer application's own permission. Remote failures return a typed error to the assistant. Notifications are opt-in and require server support. Connections are restricted to user-selected local loopback descriptors. Claude/OpenCode peer allowlists are not implemented in this panel and produce an explicit unsupported error; their primary application MCP connection remains supported. No arbitrary remote server is silently discovered.

## Independent optimisation settings {#optimisations}

These ReShade INI keys are under `[CYGAMECAPTURE]`. Transport/storage options apply when the device/host starts; change them before restarting the game. Control permission can change live.

| Key | Default | Effect |
| --- | --- | --- |
| `AutomationEnabled` | 1 | Start the authenticated local host |
| `AutomationAllowMutations` | 0 | Permit business mutations |
| `AutomationCompact` | 0 | Expose five discovery/resource gateway tools instead of the full small catalogue |
| `AutomationNotifications` | 1 | Publish tool/resource changes for compatible HTTP subscriptions |
| `AutomationCacheTTL` | 1000 | Discovery TTL in milliseconds; explicit refresh remains possible |
| `AutomationPerformance` | 0 | Private metadata-only JSONL under the Automation folder |
| `AutomationResultMemoryBytes` | 8388608 | Maximum retained result memory |
| `AutomationResultTTL` | 300000 | Retained-result lifetime in milliseconds |
| `AIEndpoint`, `AIKeyEnv` | local URL, empty | Compatible endpoint and environment variable containing its key |
| `AIContextBytes` | 0 | API context budget; native CLI agents retain native management |

Compact mode supplies `cy_tools_search`, `cy_tools_describe`, `cy_tools_call`, `cy_resources_read` and `cy_resources_grep`. Load the complete schema before invoking a discovered operation. Read `cygc://operations` and `cygc://state` through resources; follow chunk manifests/URIs for large text. Results over 16 KiB use private references when within the retention quota. Follow every `firstUri`/`nextUri` while the reference remains valid. Expiry fails explicitly and never replays the original action. The current paginated metadata operations do not justify disk spooling or filesystem/ripgrep access, so neither is enabled. If retention is exhausted the SDK can return the complete result inline instead of silently truncating it.

OBS exposes independent compact/notification/performance/cache properties; recreate the source to apply them. Unreal exposes `CyGameCapture.Automation.Compact`, `.Notifications`, `.CacheTTL` and `.Performance`, applied on subsystem startup (for example through project console-variable configuration); `-CyGameCaptureNoMCP` disables its host. Results use the shared 8 MiB / five-minute defaults. HTTP subscriptions invalidate cached discovery/resources before TTL; the stdio relay supports neither resource forwarding nor subscriptions, so its tools use fresh calls and CLI `--full` remains available. Mutations are never cached or retried. Performance files contain names, durations, byte counts, status, cache flags and timestamps, excluding messages, arguments, results and secrets.
