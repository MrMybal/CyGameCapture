---
id: public-api-cygc-readframesync-frame-read
title: cygc::ReadFrameSync
description: Lit l’instantané actuel de métadonnées d’un nom Spout. Incluez CyGameCaptureCore/FrameSync.hpp sous Windows. Cet appel ne lit pas les pixels.
section: API de développement
parent: core-api
kind: api
order: 80
sourceRevision: 3d42bb3b8e2f
---

Lit l’instantané actuel de métadonnées d’un nom Spout. Incluez CyGameCaptureCore/FrameSync.hpp sous Windows. Cet appel ne lit pas les pixels.

## Signature {#signature}

```cpp
bool cygc::ReadFrameSync(const std::string& sender_name, cygc::FrameSyncRecord& out)
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `sender_name` | `const std::string&` |  | Nom exact de l’émetteur Spout. |
| `out` | `cygc::FrameSyncRecord&` |  | Enregistrement de sortie, à utiliser uniquement si true est renvoyé. |

## Retour {#returns}

`bool` — true avec un enregistrement de sortie rempli si un instantané existe ; false sinon.
