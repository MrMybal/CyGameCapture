===============================================================================
 CyGameCaptureRS - installing the ReShade add-on in a game
===============================================================================

(Version francaise : LISEZMOI.txt)

1. Install ReShade
-------------------------------------------------------------------------------

Download ReShade 6.8.0 from https://reshade.me and pick the build WITH ADD-ON
SUPPORT (the installer says so explicitly: ordinary builds refuse external
add-ons).

Install it for the game as usual, choosing the game's API (DirectX 10/11/12).

The version matters: the add-on targets add-on API 20, the one of ReShade
6.8.0. An older version (6.7.x = API 18) will not load it.


2. Copy the add-on
-------------------------------------------------------------------------------

Copy  CyGameCaptureRS.addon64  next to the game executable, where ReShade put
its dxgi.dll / d3d11.dll.

Copy  CyGameCaptureAI.exe  into the same folder too: it runs the AI assistant
(see below). Everything else works without it.

To keep them somewhere else, give the folder in ReShade.ini:

    [ADDON]
    AddonPath=.\path\to\the\folder


3. Check
-------------------------------------------------------------------------------

Start the game, then open ReShade.log next to the executable. It should
contain:

    Registered add-on "CyGameCaptureRS" ... using ReShade API version 20.
    [CyGameCaptureRS] Add-on initialized ...
    [CyGameCaptureRS] Renderer: D3D11 (Spout output available)

If the log says the renderer is not supported, the game uses an API the add-on
does not handle (only D3D10, D3D11 and D3D12 are).


4. Pick a buffer
-------------------------------------------------------------------------------

Press Home to open the ReShade overlay. The "CyGameCaptureRS - Buffer
Inspector" window opens with it. The language selector next to the logo
switches the interface to French; "About" shows the licence and the address of
the source code.

  - Buffer Inspector tab: the contact sheet shows every buffer of the frame
    side by side; the table below gives resolution, format, writes and how
    steady each buffer is. "Steady buffers only" hides the ones an engine only
    borrows for a frame or two.
  - browse with Previous / Next, the Left / Right keys, or by clicking. The
    live GPU preview of the selection appears below.
  - "Use As Capture Source" sends that buffer over Spout2
    (sender CyGameCaptureRS::<game>).
  - "Add As Extra Stream" sends another one at the same time (::2, ::3...).
  - "Show selected buffer in game" (or F10) shows the selected buffer to the
    player: play without the interface.

The buffer to look for is usually the last colour target written BEFORE the
interface is drawn. Many games (Unreal Engine 4 ones for instance) draw their
interface straight into the final image: no buffer holds the picture without
it. The "Moment of the frame" slider then reads a buffer at a given point of
the frame (after its K-th write), before the interface is drawn (D3D11).

Receive the stream in OBS with the CyGameCaptureOBS plugin (Sources > + >
CyGameCapture) or the generic Spout2 plugin, or in any Spout2 application.


5. The AI assistant
-------------------------------------------------------------------------------

At the top of the Buffer Inspector tab, "AI Assistant" section:

  1. show a scene of the game where the interface is VISIBLE;
  2. pick the AI client (Claude Code, Codex or OpenCode, already installed and
     signed in on this PC);
  3. click "Find the setting that removes the game's interface";
  4. the "CyGameCapture - AI Assistant" discussion opens; a minute or two
     later the reply shows with its proposal: "Show" to see it in the
     preview, "Apply" to send it, "Apply, and send the interface alone too"
     to get the interface alone on a second stream;
  5. then answer in the discussion ("the subtitles are still there"...).

Nothing is applied without your click. What goes to the AI provider, with YOUR
account: small pictures of the frame and the list of buffers. CyGameCapture
stores no key. Everything sent and received stays readable in
%LOCALAPPDATA%\CyGameCapture\AI\<game>.

D3D11 games only for now.


6. Settings (ReShade.ini, section [CYGAMECAPTURE])
-------------------------------------------------------------------------------

    Language            en | fr, language of the interface
    SenderName          Spout sender name (default CyGameCaptureRS::<game>)
    ViewToggleKey       key showing the selected buffer in the game
                        (default 121 = F10; 0 disables it)
    StandaloneWindow    show the standalone window with the overlay (default 1)
    AIClient            auto | claude | codex | opencode
    AIModel             model to use (default: the client's)
    AIClientPath        path of the client's .exe when it is not found
    AILanguage          fr | en, when the assistant should reply in another
                        language than the interface's

The full list is in the project's README.


-------------------------------------------------------------------------------
 SCOPE
-------------------------------------------------------------------------------

CyGameCapture only targets applications where ReShade is normally usable. It
contains nothing to bypass anti-cheat, a protection or ReShade blocking, and
nothing to hide itself or inject into other processes. An application that
blocks ReShade is simply unsupported.


-------------------------------------------------------------------------------
 LICENCE
-------------------------------------------------------------------------------

Copyright (C) 2026 Cyberalien. Free software under the GNU Affero General
Public License, version 3 or later (LICENSE.txt), provided WITHOUT ANY
WARRANTY. Source code: https://github.com/MrMybal/CyGameCapture
The third-party licences come with the package.
