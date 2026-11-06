---
id: public-api-cygc-makesendername-sender-name
title: cygc::MakeSenderName
description: Construit un nom d’émetteur ReShade à partir d’un nom de jeu nettoyé. Incluez CyGameCaptureCore/SenderNaming.hpp.
section: API de développement
parent: core-api
kind: api
order: 80
sourceRevision: 07b7458b2068
---

Construit un nom d’émetteur ReShade à partir d’un nom de jeu nettoyé. Incluez CyGameCaptureCore/SenderNaming.hpp.

## Signature {#signature}

```cpp
std::string cygc::MakeSenderName(std::string_view game_name, unsigned int instance = 1)
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `game_name` | `std::string_view` |  | Nom du jeu nettoyé. |
| `instance` | `unsigned int` |  | Indice d’instance ; 1 par défaut. |

## Retour {#returns}

`std::string` — CyGameCaptureRS::<Game>, avec ::N pour les instances supérieures à un ; 255 octets au maximum.

## Références {#references}

[cygc::SanitizeGameName](doc:public-api-cygc-sanitizegamename-sanitize)
