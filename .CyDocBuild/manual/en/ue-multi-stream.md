---
id: ue-multi-stream
title: Use several Unreal streams
description: Give every view a unique name and connect each stream to its own OBS source.
section: Unreal Engine
order: 20
parent: unreal
revision: 0bc3b7d261cd
---

## Name independent views {#workflow}
Add one sender component per camera or target, using names such as CameraA, CameraB and Depth. Keep the names distinct. AutoRename adds a numeric suffix if a name is already registered; use GetActiveSenderName rather than assuming the requested name was accepted unchanged.

Add a separate OBS source for each actual sender name. Each stream adds GPU work and uses a machine-wide Spout sender slot. A custom frame rate limits copies but does not necessarily reduce the cost of a separately ticking scene capture. Stop unused captures and senders. The [recording guide](doc:recording) explains synchronised output files.
