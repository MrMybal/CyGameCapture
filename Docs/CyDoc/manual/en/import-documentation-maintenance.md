---
id: documentation-maintenance
title: Build and maintain the documentation
description: Docs/CyDoc/README.md
section: Maintenance
order: 210
revision: 3b8ae812b7d0
---

# CyGameCapture documentation with CyDoc {#cygamecapture-documentation-with-cydoc}

[Documentation](https://mrmybal.github.io/CyGameCapture/#/en/overview) · [Documentation en français](https://mrmybal.github.io/CyGameCapture/#/fr/overview)

The shipped manual is an application-owned CyDoc 0.1.0 export at `Docs/UserGuide` and
`CyGameCaptureUE/Docs/UserGuide`. Both are generated directories. Open `index.html` using
`file://`; `en.html` and `fr.html` remain compatibility entry points for the former guide.
The native components now open encoded file URLs with CyDoc page/section/media routes.

## Public reader and deployment {#public-reader-and-deployment}

`.github/workflows/documentation.yml` regenerates and tests the reader on each push to
`main` or manual workflow dispatch, then uploads **only `Docs/UserGuide`** to GitHub Pages.
The site uses the exact same export and relative resource providers as the embedded reader;
there is no separate website renderer. English/French URLs use stable CyDoc hash routes.
The workflow neither builds nor uploads native application binaries, user configurations,
logs, captures, browser notes, the repository root or the build workspace. The public source
index remains limited to the distributed headers configured in `cydoc.config.json`.

Pages publishing source must be **GitHub Actions** (`build_type: workflow`). Actions are
pinned to their upstream commit IDs. Build permissions are read-only; only the deployment
job can write Pages and request its deployment identity token. All reader functionality is
still tested using `file://` before deployment. The in-application help keeps its local URLs.

## Rebuild and verify {#rebuild-and-verify}

```bat
npm ci
npm run docs:build
npm run docs:check
npm run docs:test
build.cmd Release
Tools\MakeRelease.cmd
Tools\MakeTestPackage.cmd
```

After changing the Unreal integration, rebuild its distributed Editor DLLs with
`powershell -NoProfile -ExecutionPolicy Bypass -File Tools/BuildUnrealDocumentation.ps1`
before making a release. This uses installed UE 5.3–5.8 SDKs and isolated host projects,
waits for Unreal's build mutex and limits compilation to two actions per build. Game
targets consume the shipped plugin source; this command does not validate Shipping targets.

Node.js 22+ is a generation dependency only. The pinned local CyDoc archive, upstream Git
commit, licence and SHA256 are in `Resources/CyDoc`. The package lock pins generation/test
dependencies. Tests use an installed Chrome/Edge or `CYDOC_BROWSER`; no browser is downloaded
automatically. The application's ordinary build runs generation, then deploys every reader
resource. Distribution scripts regenerate too, and inspect the resulting archives.

`docs:build` invokes CyDoc's `check` CLI, which validates **and writes** an export to
`build/CyDoc-export`. The application host uses public `mountViewer` and `createLink` APIs,
classic-script module loaders and CyDoc's packed-local-resource provider pattern. The host
adds the Nord default, storage namespace and old-link adapters; it does not patch the
library, its manifest, compiler or media bundles. `docs:check` is the application's static
validation of the already exported manual. `docs:test` tests that export through `file://`.

## Author and translate {#author-and-translate}

Edit pages in `manual/en` and `manual/fr`. Keep IDs, explicit heading anchors and media IDs
identical across translations. Increment `revision` on the English original when its meaning
changes and update the French `sourceRevision` only after translating that change. CyDoc
writes `translation-report.json` and exposes a translation-status page. Existing English-only
technical references intentionally fall back to English; they are not falsely marked translated.
The French-only original test-suite instructions remain accessible in French.

`references.json` maps original project READMEs, architecture/API guides, installation text,
changelog and third-party notices to stable CyDoc IDs. `ImportDocumentation.mjs` refreshes
their generated `import-*.md` pages each build. Edit the original references, not those copies.
Original code blocks and installation text are retained. Source repository links outside the
bounded index remain optional external links, listed in `build-info.json`.

The 17 original bilingual chapters were migrated to authored Markdown, including all six
screenshots and three diagrams. `migration-inventory.json` records the old chapter addresses.
Images use minimal controls, a labelled A/B comparison and a built-in buffer-list annotation.
Only **image** is enabled. The three diagrams compile to SVG, so graph is unnecessary. There
are no application videos, GIF demonstrations, 3D models or media collections requiring video,
gif, mesh or gallery. Three.js is neither installed in the application toolchain nor shipped.

## Source documentation and factual limits {#source-documentation-and-factual-limits}

CyDoc scans only `CyGameCaptureCore/Include/CyGameCaptureCore` and
`CyGameCaptureUE/Source/CyGameCaptureUE/Public`, whose source is intended for distribution.
The native help API uses a real `@doc` annotation. The index retains comments/source, lexical
includes and code line anchors. It does not infer overloads, expand macros or execute Unreal
Blueprints. C# Build.cs is build configuration, not a public C# API. No Doxygen or TypeDoc tool
is needed or simulated. Engine, private and third-party implementation sources and local
settings are excluded. Indexed sources retain their original licences; full notices ship
inside each reader's `notices` directory.

The native architecture reference contains historical “future/reserved” labels for an OBS
plugin and D3D12 bridge now implemented. Its imported page flags those statements explicitly;
the current capability matrix is authoritative. Profiles are still planned. Existing timing,
precision and benchmark results remain identified as measurements, not new guarantees.

Reading position, code-tab state and navigation scroll are provided by CyDoc. Notes and
bookmarks depend on browser file-origin storage policy; moving a manual can change that
context. CyDoc loads all pages at once and has no segmented search index. JavaScript is now
required, unlike the former static fallback. The target is Windows 10/11 with a modern
browser; other OSes, WebViews, codecs and WebGL are not part of this application's targets.

Unreal replaces its former online documentation entry with **Help → CyGameCapture user guide** and `CyGameCapture.Help [en|fr] [url]` (the last argument prints the URL without opening a browser).
Its build rules stage `Docs/UserGuide/...` as NonUFS files so an exported application
can launch the browser reader outside its cooked pak data. The staged-game behaviour
still needs a real cooked-project test; Editor compilation alone does not verify it.
