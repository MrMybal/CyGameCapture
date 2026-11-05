---
id: glossary
title: Glossary and licences
description: CyGameCapture — Glossary and licences
section: User guide
order: 16
revision: 4a0158767d6b
---

| Term | Meaning |
| --- | --- |
| Frame / write / moment | A game image, an operation updating a texture, and a point between those operations. |
| Render target / back buffer | A texture being drawn into / the image presented by the swap chain. |
| HUD / UI / alpha | Game information and menus / transparency: zero transparent, one opaque. |
| Sender / receiver / Spout | An application publishing a named shared GPU texture / an application reading it / the local sharing system. |
| Tonemapping / HDR / SDR | Mapping high-range scene values into a display image / high dynamic range / standard dynamic range. |
| MSAA / resolve | Multiple samples per pixel / combining them into a single-sample texture. |
| Lossless / precision | Keeping the encoder’s input exactly does not undo conversions already applied before encoding. |

Native CyGameCapture components use **AGPL-3.0-or-later**. CyGameCaptureUE uses **MIT**. Third-party components retain their own licences. Full English texts, notices and source links ship in each component’s `LICENCES` folder. The legal texts remain in their original language; the French guide is explanatory.

Source and issue tracker: [github.com/MrMybal/CyGameCapture](https://github.com/MrMybal/CyGameCapture) (Internet required). The software is provided without warranty. This manual describes the current implemented behaviour, including its limitations.
