---
id: ingame
title: Jouer avec l’image sélectionnée
description: Remplacer l’image affichée par le buffer choisi puis revenir avec F10.
section: Guide utilisateur
parent: inspector
order: 5
sourceRevision: 9858207a94ad
---

Activez **Show selected buffer in game** ou appuyez sur `F10` pour remplacer l’image affichée au joueur par le buffer sélectionné. Répétez pour revenir à la vue normale. Le raccourci se règle avec `ViewToggleKey` ; zéro le désactive.

Cette fonction est distincte de la diffusion : une image correcte en jeu ne prouve pas que la capture est démarrée ni qu’OBS reçoit le bon émetteur. Vérifiez la sortie dans l’onglet Capture.

La texture choisie doit avoir la taille du back buffer et un format compatible pour la copie. Le redimensionnement et la conversion de format pour ce remplacement en jeu ne sont pas disponibles. Utilisez la scène propre ou un moment compatible ; la profondeur et les images HDR intermédiaires ne conviennent pas forcément à l’affichage direct.

```image
{"id": "m1-view-mode-no-hud", "src": "assets/m1_view_mode_no_hud.png", "alt": "Application de test affichant le buffer propre sans HUD", "controls": "minimal"}
```

Vue propre facultative pour le joueur, indépendante de la sélection de source dans OBS.
