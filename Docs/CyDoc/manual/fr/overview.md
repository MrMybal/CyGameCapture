---
id: overview
title: À quoi sert CyGameCapture
description: CyGameCapture — À quoi sert CyGameCapture
section: Guide utilisateur
order: 0
sourceRevision: 8885f330ff0d
---

CyGameCapture sélectionne des images dans le rendu d’un jeu et les partage avec **Spout2**. Vous pouvez conserver l’image normale avec son interface tout en envoyant une scène propre à OBS, capturer la profondeur, isoler le HUD ou diffuser plusieurs buffers à la fois.

- **Add-on ReShade :** inspecter un jeu, choisir une cible de rendu et la diffuser.
- **Plugin OBS :** recevoir un flux comme source et, si nécessaire, enregistrer plusieurs sources dans des fichiers séparés.
- **Plugin Unreal :** envoyer des render targets, des scene captures ou un viewport et recevoir des images Spout dans Unreal.
- **Assistant IA :** proposer un buffer et un moment de capture à partir des images d’une frame D3D11.

Le partage et la capture habituelle restent sur le GPU. L’encodage d’un enregistrement est une opération distincte ; l’assistant lit de petites images dans son propre processus uniquement lorsque vous demandez une analyse.

```diagram
{"type": "flow", "title": "Le chemin de capture", "nodes": [{"id": "0", "label": "Jeu + ReShade", "x": 20, "y": 20, "width": 260}, {"id": "1", "label": "Buffer choisi", "x": 310, "y": 20, "width": 260}, {"id": "2", "label": "Texture Spout", "x": 20, "y": 150, "width": 260}, {"id": "3", "label": "OBS / récepteur", "x": 310, "y": 150, "width": 260}], "edges": [{"from": "0", "to": "1"}, {"from": "1", "to": "2"}, {"from": "2", "to": "3"}]}
```

```callout
{"type": "info", "title": "À savoir", "body": "Les captures montrent une petite scène de test animée pour rendre chaque fonction identifiable. Ce sont de vraies captures de l’application."}
```
