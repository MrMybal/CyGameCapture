---
id: inspector
title: Trouver et inspecter les buffers
description: Trouver une cible persistante et vérifier son image, son format et ses écritures.
section: Guide utilisateur
order: 3
sourceRevision: f3cfd0e7fcdf
---

Un **buffer / render target** est une texture utilisée par le jeu pendant le dessin d’une frame. L’image finale affichée est le **back buffer**. Les autres cibles contiennent souvent des données intermédiaires : normales, éclairage ou profondeur. Une image ressemblant à la scène n’est donc pas forcément prête à être affichée.

La **planche de vignettes / Contact Sheet** montre les aperçus. Cliquez pour sélectionner un buffer. Le tableau indique son identifiant, sa taille, son format, ses écritures et son pourcentage de stabilité **Steady**. Une bonne source persistante est écrite presque à chaque frame. **Steady buffers only** masque les ressources temporaires ; désactivez-le pour rechercher une cible absente.

- Filtrez les textures écrites récemment, couleur, profondeur, MSAA, mip/array, taille relative au back buffer et format. Changez le tri si nécessaire.
- Utilisez **Précédent / Suivant**, les flèches ou une ligne du tableau. **Freeze Preview** fige l’inspection, sans mettre le jeu en pause.
- Vérifiez l’image entière : objets présents, absence de HUD, de flou, de couleurs anormales ou de bordures périmées. Pour une capture SDR, préférez la couleur finale après tonemapping.
- La résolution dynamique peut n’utiliser qu’une partie d’une texture. L’overlay indique la zone réellement rendue ; la capture la redimensionne vers une taille de flux stable.
- Les identifiants ne restent stables que pendant une exécution. Revérifiez-les après un redémarrage ou un changement de réglages graphiques.

```image
{"id": "m1-buffer-inspector", "src": "assets/m1_buffer_inspector.png", "alt": "Inspecteur avec liste des ressources, format, résolution, écritures et aperçu en direct", "controls": "minimal", "annotations": [{"id": "buffer-list", "type": "rect", "x": 0.49, "y": 0.34, "toX": 0.97, "toY": 0.47, "label": "Liste des buffers"}]}
```

Détails de la ressource et aperçu GPU. Les versions récentes ajoutent la planche de vignettes et les filtres de stabilité.

## Timeline et Debug {#timeline-and-debug}

**Timeline** liste les événements clear, draw, copy, resolve et present de la dernière frame. Filtrez les écritures ou la ressource choisie ; cliquer sur un événement sélectionne sa ressource. **Debug** affiche le moteur graphique, le numéro de frame et les compteurs du suivi. Activez les traces détaillées uniquement pour un diagnostic : le journal de chaque ressource peut devenir volumineux.

```image
{"id": "m1-timeline", "src": "assets/m1_timeline.png", "alt": "Chronologie des écritures de ressources puis de la présentation finale", "controls": "minimal"}
```

L’ordre des écritures explique quelle image existe à chaque moment.
