---
id: capture
title: Flux, aperçus et alpha
description: CyGameCapture — Flux, aperçus et alpha
section: Guide utilisateur
order: 6
sourceRevision: c04c07b603ef
---

**Use As Capture Source** sélectionne le flux principal. **Add As Extra Stream** ajoute un buffer sans le remplacer. Les noms suivent `CyGameCaptureRS::Jeu`, puis `::2`, `::3`, etc. Arrêtez chaque flux devenu inutile avec son bouton Stop.

L’onglet **Capture** indique l’état, l’émetteur, la source, la résolution, le format, les FPS, les images envoyées/ignorées et le coût CPU de copie. **Output preview (what OBS receives)** affiche la texture publiée. L’aperçu de l’inspecteur montre seulement la sélection actuelle, pas forcément ce qu’un flux existant envoie.

```image
{"id": "m1-capture-tab-two-streams", "src": "assets/m1_capture_tab_two_streams.png", "alt": "Onglet Capture avec deux flux actifs et grand aperçu de sortie", "controls": "minimal"}
```

Vérifiez le nom de l’émetteur et l’aperçu de sortie avant de régler le récepteur.

## Image transparente alors que le RGB est correct {#transparent-image-even-though-rgb-is-correct}

Certains jeux laissent l’alpha à zéro dans une image couleur complète. Les flux couleur sont **opaques par défaut**, sans modifier le RGB. Activez **Preserve source alpha (advanced)** uniquement si vous avez besoin de la transparence d’origine. Ce réglage est propre à chaque flux ; `PreserveSourceAlpha` définit la valeur initiale des nouveaux flux. Les flux HUD conservent toujours leur masque. L’inspecteur affiche le RGB de façon opaque : il peut donc paraître correct alors qu’un récepteur rend invisible une texture à alpha zéro préservé.

**Flush after copy** soumet immédiatement le travail D3D11. Laissez-le normalement désactivé ; essayez-le pour diagnostiquer une latence de livraison. Il ne corrige ni une mauvaise source ni l’absence de récepteur et ajoute du travail CPU.
