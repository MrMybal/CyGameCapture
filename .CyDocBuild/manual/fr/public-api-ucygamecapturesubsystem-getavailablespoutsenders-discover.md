---
id: public-api-ucygamecapturesubsystem-getavailablespoutsenders-discover
title: UCyGameCaptureSubsystem::GetAvailableSpoutSenders
description: Découvre les producteurs Spout des différentes applications. Incluez CyGameCaptureSubsystem.h ; module CyGameCaptureUE.
section: API de développement
parent: ue-subsystem
kind: api
order: 80
sourceRevision: a9395c9bbb96
---

Découvre les producteurs Spout des différentes applications. Incluez CyGameCaptureSubsystem.h ; module CyGameCaptureUE.

## Signature {#signature}

```cpp
TArray<FCySpoutSenderInfo> GetAvailableSpoutSenders(bool bForceRefresh = false)
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `bForceRefresh` | `bool` |  | Actualise immédiatement la découverte au lieu de renvoyer le cache ; false par défaut. |

## Retour {#returns}

`TArray<FCySpoutSenderInfo>` — Tableau des informations d’émetteurs ; un tableau vide signifie qu’aucun n’est actuellement connu.
