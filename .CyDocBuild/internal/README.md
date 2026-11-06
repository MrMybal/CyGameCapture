# CyGameCapture documentation tooling

CyDoc 0.1.0 is pinned to commit 6ddf9a582defb2f11fbb3f992afef6f98dcadc93.
The vendored archive and full MIT licence are in `../vendor`.

From the application root:

```bat
npm --prefix .CyDocBuild ci
npm --prefix .CyDocBuild run docs:build
npm --prefix .CyDocBuild run docs:check
npm --prefix .CyDocBuild run docs:test
build.cmd Release
Tools\MakeRelease.cmd
Tools\MakeTestPackage.cmd
```

Generation invokes the public CyDoc `check` CLI, which writes the export.
All sources, tooling and API adapter inputs live in .CyDocBuild. The generated
Documentation directory contains only Documentation.html, DocAI.md and DocData.
Only image is enabled. The three static diagrams require no graph runtime;
there are no application videos, GIFs or meshes. Three.js is not installed.

Edit paired English/French Markdown with common page, anchor and media IDs.
Set the same parent IDs in both editions. Update the English revision and the
French sourceRevision after reviewing the actual translation. The API adapters
contain explicit declarations, not implementation bodies. Their translation.json
records the reviewed English input hash. The native help API comes from a real
annotated C++ comment and a separately translated French page.

Internal archives and temporary logs stay here, outside the content directories.
They are ignored by Git and never copied to the reader, search or distribution.
Archived references retain the older manuals for comparison and recovery.

The Pages workflow generates the same export; a Pages-only index redirect retains
the public root URL. It uploads only the generated reader tree. Push and publishing
remain subject to the user's explicit authorisation; this update does not publish.
