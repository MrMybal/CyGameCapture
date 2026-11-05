---
id: glossary
title: Glossaire et licences
description: CyGameCapture — Glossaire et licences
section: Guide utilisateur
order: 16
sourceRevision: 4a0158767d6b
---

| Terme | Sens |
| --- | --- |
| Frame / écriture / moment | Une image du jeu, une opération qui modifie une texture et un point entre ces opérations. |
| Render target / back buffer | Une texture dans laquelle on dessine / l’image présentée par la swap chain. |
| HUD / UI / alpha | Informations et menus du jeu / transparence : zéro transparent, un opaque. |
| Émetteur / récepteur / Spout | Application publiant une texture GPU partagée nommée / application la lisant / système de partage local. |
| Tonemapping / HDR / SDR | Adaptation des valeurs de scène à l’affichage / grande plage dynamique / plage dynamique standard. |
| MSAA / resolve | Plusieurs échantillons par pixel / leur regroupement en une texture à un échantillon. |
| Sans perte / précision | Conserver exactement l’entrée de l’encodeur n’annule pas les conversions effectuées avant l’encodage. |

Les composants natifs CyGameCapture utilisent **AGPL-3.0-or-later**. CyGameCaptureUE utilise **MIT**. Les composants tiers conservent leurs licences respectives. Les textes complets en anglais, attributions et liens sources se trouvent dans le dossier `LICENCES` de chaque composant. Les textes juridiques restent dans leur langue d’origine ; le guide français est explicatif.

Sources et signalements : [github.com/MrMybal/CyGameCapture](https://github.com/MrMybal/CyGameCapture) (Internet nécessaire). Le logiciel est fourni sans garantie. Ce manuel décrit les fonctions réellement implémentées et leurs limites.
