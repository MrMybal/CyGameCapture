---
id: public-api-ucygamecapturesubsystem-createsender-create-sender
title: UCyGameCaptureSubsystem::CreateSender
description: Crée un flux émetteur associé à un propriétaire via le sous-système moteur. Incluez CyGameCaptureSubsystem.h ; module CyGameCaptureUE. Associez cet appel à DestroySender.
section: API de développement
parent: ue-subsystem
kind: api
order: 80
sourceRevision: ec3513d61120
---

Crée un flux émetteur associé à un propriétaire via le sous-système moteur. Incluez CyGameCaptureSubsystem.h ; module CyGameCaptureUE. Associez cet appel à DestroySender.

## Signature {#signature}

```cpp
FSenderStreamPtr CreateSender(const FCySenderStreamConfig& Config, UObject* Owner, FString& OutError)
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `Config` | `const FCySenderStreamConfig&` |  | Réglages de l’émetteur, dont le nom, le mode source et la cadence. |
| `Owner` | `UObject*` |  | Propriétaire vivant du flux, utilisé pour sa libération. |
| `OutError` | `FString&` |  | Message d’erreur de sortie si la création échoue. |

## Retour {#returns}

`FSenderStreamPtr` — Pointeur partagé d’émetteur utilisable entre threads, ou pointeur invalide en cas d’échec. Consultez OutError avant utilisation.

## Références {#references}

[UCyGameCaptureSubsystem::GetAvailableSpoutSenders](doc:public-api-ucygamecapturesubsystem-getavailablespoutsenders-discover)
