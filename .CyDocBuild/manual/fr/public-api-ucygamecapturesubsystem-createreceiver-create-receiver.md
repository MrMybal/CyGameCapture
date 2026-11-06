---
id: public-api-ucygamecapturesubsystem-createreceiver-create-receiver
title: UCyGameCaptureSubsystem::CreateReceiver
description: Crée un flux récepteur associé à un propriétaire. Incluez CyGameCaptureSubsystem.h ; module CyGameCaptureUE. Associez cet appel à DestroyReceiver et fournissez une cible de destination.
section: API de développement
parent: ue-subsystem
kind: api
order: 80
sourceRevision: 5ebde36dfc90
---

Crée un flux récepteur associé à un propriétaire. Incluez CyGameCaptureSubsystem.h ; module CyGameCaptureUE. Associez cet appel à DestroyReceiver et fournissez une cible de destination.

## Signature {#signature}

```cpp
FReceiverStreamPtr CreateReceiver(const FCyReceiverStreamConfig& Config, UObject* Owner, FString& OutError)
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `Config` | `const FCyReceiverStreamConfig&` |  | Nom exact de l’émetteur, nom du récepteur et réglages de connexion. |
| `Owner` | `UObject*` |  | Propriétaire vivant du flux. |
| `OutError` | `FString&` |  | Message d’erreur de sortie si la création échoue. |

## Retour {#returns}

`FReceiverStreamPtr` — Pointeur partagé de récepteur utilisable entre threads, ou pointeur invalide en cas d’échec.

## Références {#references}

[UCyGameCaptureSubsystem::GetAvailableSpoutSenders](doc:public-api-ucygamecapturesubsystem-getavailablespoutsenders-discover)
