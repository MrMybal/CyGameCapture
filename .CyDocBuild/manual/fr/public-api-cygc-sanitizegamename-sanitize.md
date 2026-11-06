---
id: public-api-cygc-sanitizegamename-sanitize
title: cygc::SanitizeGameName
description: Nettoie le nom de base de l’exécutable pour un émetteur Spout. Incluez CyGameCaptureCore/SenderNaming.hpp ; C++17.
section: API de développement
parent: core-api
kind: api
order: 80
sourceRevision: 4b8f357936bf
---

Nettoie le nom de base de l’exécutable pour un émetteur Spout. Incluez CyGameCaptureCore/SenderNaming.hpp ; C++17.

## Signature {#signature}

```cpp
std::string cygc::SanitizeGameName(std::string_view exe_stem)
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `exe_stem` | `std::string_view` |  | Nom de l’exécutable sans son extension. |

## Retour {#returns}

`std::string` — Nom sans les suffixes de compilation pris en charge, avec remplacement des séparateurs réservés.
