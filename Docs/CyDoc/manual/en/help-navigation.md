---
id: help-navigation
title: Read, search and navigate this manual
description: Offline reading, media controls, accessibility and translations
section: User guide
order: 18
revision: 2
---

## Local reading {#local-reading}

The complete CyDoc reader is shipped with CyGameCapture. Open `Documentation/UserGuide/index.html` from ReShade or OBS, or `Docs/UserGuide/index.html` in the Unreal plugin. No server, account, CDN or network connection is needed. JavaScript is required by the reader. External source repository links are optional and need Internet access.

The ReShade header opens the overview. **F1** while its inspector is focused opens the current tab's help: [inspector](doc:inspector), [capture](doc:capture) or [Timeline/Debug](doc:inspector#timeline-and-debug). The OBS source's guide button opens [OBS reception](doc:obs). This does not remap keys while the game overlay is closed.

In Unreal Editor, **Help → CyGameCapture user guide** opens the [Unreal chapter](doc:unreal). The console command `CyGameCapture.Help [en|fr] [url]` does the same; the optional final `url` argument only prints the local address.

## Find information {#search}

Use the sidebar, page contents and search. Search can filter guide, API, source and media content, or a code language. Code examples are highlighted when the manual is built. Source pages provide lexical include links and line links; they are not a C++ compiler or Blueprint executor.

## Images and comparisons {#images}

Use the image toolbar to zoom, reset or enlarge within this window. The controls appear on hover or keyboard focus and remain visible on touch devices. Press **Escape** to reduce an enlarged view. Pan by dragging; use the wheel or zoom buttons. A comparison slider reveals the two actual screenshots; the test scene was animated, so they do not represent the same instant.

[Open the annotated buffer list](doc:inspector?media=m1-buffer-inspector&note=buffer-list).

Image settings expose annotation tools. Personal annotations, bookmarks, notes and reading history are local to this browser/device; export them for backup if useful. Browser policies for `file://` storage can restrict persistence. Navigation preserves reading positions during the reader session; explicit section/media links take priority. Moving the entire manual to another path can change its file-origin storage context.

## Language and theme {#language-and-theme}

English is the default. Select Français to keep the current page and common section ID. All original user-guide chapters are available in both languages. Existing English-only technical references fall back to English; French-only test instructions remain available as a French page. The [translation report](doc:translation-report) identifies missing or outdated translations rather than presenting English copies as French translations.

The initial Nord theme matches CyGameCapture's blue interface; the theme picker also provides light, dark and high-contrast choices. Tab reaches controls and links; Enter/Space activate them. Touch navigation uses CyDoc's menu and media controls. Reading data are never sent to a server.
