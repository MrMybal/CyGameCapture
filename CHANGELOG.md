# Changelog

## 0.1.0 - 2026-09-22

First release.

### CyGameCaptureRS - ReShade add-on (ReShade 6.8.0 with add-on support, Direct3D 10 / 11 / 12)

* **Buffer Inspector**: every render target and depth buffer of the frame, with a contact sheet of thumbnails, a
  sortable table (resolution, rendered area under dynamic resolution, format, writes, how steady each buffer is), filters
  and a live GPU preview.
* **Spout2 output on the GPU**: the selected buffer is copied into a texture the add-on owns and published as
  `CyGameCaptureRS::<Game>`; several buffers can be sent at once (`::2`, `::3`...). Dynamic resolution is scaled into an
  output of stable size.
* **Moment of the frame** (Direct3D 11): reads a buffer right after one of its writes, for games that draw their
  interface straight into the final image.
* **Derived streams**: depth as a greyscale picture, and the interface alone against the clean scene.
* **Show selected buffer in game** (F10): play without the interface.
* **AI assistant** (Direct3D 11): asks Claude Code, Codex or OpenCode, installed and signed in by the user, which buffer
  and moment hold the picture without the interface, then continues as a discussion. Every proposal waits for the
  user's click.
* Interface in English or French; an About box gives the licence and the address of the source code.

### CyGameCaptureOBS - OBS Studio plugin (OBS 29.1 or later, 64-bit)

* **CyGameCapture** source: lists the CyGameCapture streams (and any Spout2 sender on request), reconnects on its own,
  and draws the shared texture without a copy.
* **Recording several buffers at once** into separate files that start on the same frame, with the hardware encoder or
  lossless FFV1 modes (8-bit RGB, 16-bit 4:4:4 for depth).
* English and French.

### CyGameCaptureUE - Unreal Engine plugin (Win64, Direct3D 11 / 12)

* Spout2 **senders and receivers**, as many at a time as needed: render target, scene capture and game viewport sources
  (the viewport mode reuses the frame the engine already rendered), Blueprint and C++.
* Prebuilt for Unreal Engine 5.3, 5.4, 5.5, 5.6, 5.7 and 5.8 (one code base, written for 4.27 to 5.8; 4.27 is not
  compiled yet).
* MIT licence.
