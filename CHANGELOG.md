# Changelog

[Français](CHANGELOG.fr.md) · [Application overview](README.md)

<a id="current-version"></a>

## 0.1.0 — Beta · package update of 2026-11-06

CyGameCapture 0.1.0 is a **Beta** release, available for testing and validation before a stable release. The application components retain version 0.1.0; the Unreal plugin is also 0.1.0. The documentation uses CyDoc reader 0.1.0, pinned to upstream revision `6ddf9a5`; this is a dependency version, not a separate application release.

[Published application and individual packages](https://github.com/MrMybal/CyGameCapture/releases/tag/v0.1.0) · [Checksums](https://github.com/MrMybal/CyGameCapture/releases/download/v0.1.0/SHA256SUMS.txt) · [Issue tracker](https://github.com/MrMybal/CyGameCapture/issues)

### Changes in the current downloads

- Ship the CyDoc manual as `Documentation/Documentation.html`, `DocAI.md` and `DocData/`. English is the default; all 37 pages have complete French translations with common page and section identifiers.
- Include the six application screenshots, three static SVG diagrams, image zoom, annotations and comparison. Only the image module is enabled; Three.js is not included.
- Add hierarchical navigation, header search with Ctrl+K, useful page tooltips, themes and remembered reading positions. Publish the same reader on GitHub Pages while retaining local `file://` use without a server.
- Update ReShade, OBS and Unreal help entries and ReShade F1 to open the matching local page or section. Keep aliases for older chapter links.
- Provide user workflows, developer API signatures, parameter descriptions, includes and short examples. Remove raw implementation browsing, internal working notes and documentation-maintenance pages from the human reader.
- Include the complete manual and applicable English license texts in the native, OBS and Unreal packages. Keep user settings separate from installed application files. CyGameCaptureCore remains the product's native library; neither CyToolsCore nor CyUEMCPSDK is bundled.
- Supply rebuilt native Win64 components and prebuilt Unreal Editor plugins for 5.3 through 5.8.

### Fixed behavior

- Colour streams and previews no longer disappear when a game writes valid RGB with zero alpha, as observed in Stray. Colour output makes alpha opaque without changing RGB; derived HUD streams preserve their mask. **Preserve source alpha (advanced)** restores source transparency for an individual colour stream when needed.
- Receiver diagnostics report the actual two-bit alpha of RGB10A2 streams and report an error when a frame dump cannot be written.
- Documentation generation handles Windows and Linux line endings consistently when checking the French API translation.

### Known limitations

- The capture pipeline targets Windows x64 D3D11/D3D12. D3D10 support is limited; Vulkan and OpenGL are unsupported.
- Intermediate-write capture and AI buffer search require D3D11. Intermediate moments do not support MSAA. F10 in-game replacement requires a compatible buffer size and format.
- Source compatibility targets Unreal 4.27–5.8. Prebuilt Editor archives cover 5.3–5.8; Unreal 4.27, 5.0–5.2 builds and cooked Shipping targets have not been verified. Blueprint properties alone do not establish implemented behavior; the sender's exposed Auto Reconnect setting does not currently provide automatic device-reset recovery.
- Spout sharing is local to the PC. Use the same GPU for producer and receiver. The machine-wide sender table has 64 slots by default, shared across Spout applications; GPU load and memory constrain useful stream counts.
- An isolated HUD obtained by subtracting scene images is approximate at soft edges and effects. An existing transparent HUD-only render target is preferable when available.
- Hardware/YUV recording does not preserve source alpha or full depth precision. FFV1 8-bit RGB retains HUD alpha; the 16-bit 4:4:4 path uses YUV without alpha and does not retain every original 16-bit unorm value through OBS.
- AI clients are optional and installed separately. The selected provider receives sampled images, metadata and conversation through the user's account; model availability, charges and permissions belong to that provider.

The published release does not provide a ticket-by-ticket list of outstanding bugs. Check the [actual issue tracker](https://github.com/MrMybal/CyGameCapture/issues) and report reproducible problems; these limitations are not claims that no other bugs exist.

## 0.1.0 — first release · 2026-09-22

### CyGameCaptureRS

- Buffer Inspector: render targets and depth buffers, thumbnail contact sheet, sortable resource table, format/resolution/write filters, steadiness and live GPU previews.
- Spout2 publication of one or several selected buffers as `CyGameCaptureRS::<Game>`, with numbered additional streams. Dynamic resolution is scaled into a stable output size.
- D3D11 frame moments before HUD rendering; derived linearised depth and isolated HUD streams.
- F10 selected-buffer view, optional AI search through Claude Code, Codex or OpenCode, and English/French interface.

### CyGameCaptureOBS

- CyGameCapture/Spout2 sender discovery, receiver reconnection and shared-texture display without CPU image readback.
- Recording several streams into separate synchronised files, with hardware encoding and FFV1 modes.
- English/French interface; built against OBS Studio 29.1.3.

### CyGameCaptureUE

- Multiple Spout2 senders and receivers for render targets, scene captures and the game viewport, through Blueprint and C++.
- Win64 D3D11/D3D12 integration and separate prebuilt packages for Unreal Engine 5.3–5.8.
- MIT license; native ReShade, AI helper, Core and OBS components use AGPL-3.0-or-later.
