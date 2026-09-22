===============================================================================
 CyGameCaptureOBS - OBS Studio plugin
===============================================================================

(Version francaise : LISEZMOI.txt)

Receives in OBS the GPU buffers that CyGameCaptureRS (ReShade add-on) and
CyGameCaptureUE (Unreal plugin) publish over Spout2, without any copy through
the CPU, and records several of them at once into separate files that start on
the same frame.

Requires OBS Studio 29.1 or later, 64-bit, on Windows 10/11. Built against and
tested with OBS Studio 29.1.3.


Install
-------------------------------------------------------------------------------

Close OBS, then double-click  Install-OBS-Plugin.cmd.

It copies the CyGameCaptureOBS folder into
%ProgramData%\obs-studio\plugins\CyGameCaptureOBS
No administrator rights are needed: that is where OBS looks for the plugins of
the current user on Windows.

By hand: copy the CyGameCaptureOBS folder of this package (with its bin and
data folders) into %ProgramData%\obs-studio\plugins\

Uninstall: Uninstall-OBS-Plugin.cmd, or delete that folder.


Use
-------------------------------------------------------------------------------

In OBS: Sources > + > CyGameCapture, then pick the stream in "Capture source".
The source reconnects on its own when the game restarts.

Recording several buffers at once: one CyGameCapture source per buffer, tick
"Record this buffer" on each, then "Start recording all selected buffers".

The plugin follows OBS's language (English or French).


-------------------------------------------------------------------------------
 LICENCE
-------------------------------------------------------------------------------

Copyright (C) 2026 Cyberalien. Free software under the GNU Affero General
Public License, version 3 or later (LICENSE.txt), provided WITHOUT ANY
WARRANTY. Source code: https://github.com/MrMybal/CyGameCapture
The plugin links libobs (GNU GPL v2 or later) and compiles part of the Spout2
SDK (BSD 2-clause): their licences are in the Licences folder.
