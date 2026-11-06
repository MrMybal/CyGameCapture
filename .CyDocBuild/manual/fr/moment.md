---
id: moment
title: Capturer avant le dessin du HUD
description: Copier une image entre deux dessins avant la composition de l’interface.
section: Guide utilisateur
parent: inspector
order: 4
sourceRevision: 0e7211a8dd94
---

Certains jeux dessinent la scène finale et l’interface dans le même back buffer. À la fin de la frame, aucune cible propre ne subsiste. Sélectionnez le back buffer et capturez-le **entre ces écritures**.

```diagram
{"type": "flow", "title": "Un moment avant le HUD", "nodes": [{"id": "0", "label": "1 · Effacement", "x": 20, "y": 20, "width": 260}, {"id": "1", "label": "2 · Scène propre", "x": 310, "y": 20, "width": 260}, {"id": "2", "label": "3 · HUD / menus", "x": 20, "y": 150, "width": 260}, {"id": "3", "label": "4 · Présentation", "x": 310, "y": 150, "width": 260}], "edges": [{"from": "0", "to": "1"}, {"from": "1", "to": "2"}, {"from": "2", "to": "3"}]}
```

Copie après 2 : le flux est propre, puis le jeu dessine le HUD.

1. Sélectionnez le back buffer dans l’inspecteur.
2. Lisez **Writes of this buffer in the last frame**, la liste de ses écritures.
3. Reculez **Moment of the frame** depuis la fin jusqu’à obtenir dans l’aperçu une scène complète sans interface. Une image seulement effacée ou partielle signifie que vous êtes allé trop tôt.
4. Utilisez ce réglage comme source de capture. L’aperçu, la capture et la vue en jeu utilisent le moment choisi.

Exemple : dans le menu de Stray testé, `#005 après l’écriture 2` se situe après le tonemapping et avant l’interface. Ce n’est pas un réglage universel : l’identifiant et l’ordre dépendent du jeu et de la scène. Un logo dessiné dans la scène 3D reste présent.

```callout
{"type": "info", "title": "À savoir", "body": "D3D11 uniquement. Les cibles multi-échantillonnées ne sont pas encore capturables à un moment intermédiaire. Chaque moment utilisé ajoute une copie GPU de l’image complète par frame. D3D12 enregistre les écritures en parallèle : cette fonction d’ordonnancement y est indisponible."}
```
