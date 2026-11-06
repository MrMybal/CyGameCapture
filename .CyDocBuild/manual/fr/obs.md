---
id: obs
title: Recevoir et composer dans OBS
description: Choisir l’émetteur Spout exact et composer couleur, profondeur ou HUD dans OBS.
section: Guide utilisateur
order: 7
sourceRevision: f320641d39d1
---

1. Démarrez d’abord la capture dans le jeu.
2. Dans OBS, Sources → + → **CyGameCapture**. Nommez la source selon son rôle : Scène, HUD ou Profondeur.
3. Choisissez l’émetteur exact. **Actualiser la liste** relance la découverte. Activez **Lister tous les émetteurs Spout** pour inclure les autres applications.
4. Vérifiez l’état, la résolution et les FPS. Ajustez la source au canevas et assurez-vous qu’elle est visible dans la scène actuelle.

**Automatique** choisit un émetteur CyGameCapture disponible ; préférez un nom explicite pour plusieurs flux. **Reconnexion automatique** retrouve l’émetteur choisi lorsqu’il revient. **Conserver la dernière image** garde l’image précédente pendant un transfert manqué : cela peut ressembler à un gel si le producteur est arrêté. Un ancien émetteur sélectionné peut rester affiché comme inactif.

Le partage Spout est local à ce PC. Utilisez le même GPU pour le jeu et OBS, notamment sur un portable avec GPU intégré et dédié. Un nom visible prouve l’enregistrement de l’émetteur, pas la bonne livraison de l’image. L’aperçu de sortie et le récepteur de diagnostic permettent d’isoler le problème.

Pour composer, placez une source HUD isolée au-dessus de la scène propre. Les pixels transparents nécessitent un flux conservant l’alpha et, pour enregistrer ce calque, un mode compatible. Les sources peuvent avoir des tailles différentes : contrôlez leurs transformations.
