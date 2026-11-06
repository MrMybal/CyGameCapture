---
id: install
title: Install and update
description: Install the matching add-on and OBS plugin with their complete help folders.
section: User guide
parent: overview
order: 1
revision: 56747176e840
---

## Requirements {#requirements}

Use Windows 10/11, a 64-bit game and a supported DirectX renderer. D3D11 and D3D12 are the tested capture backends; D3D10 support is limited. Moment capture and AI analysis currently require D3D11. Vulkan and OpenGL are not supported by this capture pipeline. Use a game in which ReShade is allowed and works normally.

## ReShade {#reshade}

1. Install **ReShade 6.8.0 with add-on support** for the game and its actual graphics API. A plain ReShade build cannot load this add-on; the target API is 20.
2. Extract the ReShade archive. Copy `CyGameCaptureRS.addon64`, `CyGameCaptureAI.exe`, `Documentation` and the licence files together next to the game executable, or into the configured ReShade add-on directory.
3. Start the game. Open ReShade with `Home`. Find **CyGameCaptureRS – Buffer Inspector**. Its header includes a **User guide** button.

## OBS {#obs}

Use 64-bit OBS Studio 29.1 or later (tested against 29.1.3). Close OBS. Extract the entire OBS archive and run `Install-OBS-Plugin.cmd`, or use the test suite’s installer. Start OBS and add a **CyGameCapture** source. A generic Spout2 receiver is another option. The native plugin’s source properties include **User guide**; the manual is installed with its data.

## Update or remove {#update-or-remove}

Close the game and OBS before replacing loaded binaries. Keep an older package for rollback. Replace binaries, helper, documentation and notices together. Preserve `ReShade.ini`, OBS settings and scene collections. Do not replace your game’s configuration with the test application’s demo INI. To remove the add-on, remove its files from its installation directory; use the supplied OBS uninstaller for the OBS component. Unreal installation is described in [chapter 12](doc:unreal).
