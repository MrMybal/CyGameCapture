---
id: derived
title: Profondeur et HUD isolé
description: CyGameCapture — Profondeur et HUD isolé
section: Guide utilisateur
order: 10
sourceRevision: 90b8a6ee50e7
---

## Profondeur {#depth}

Sélectionnez une texture de profondeur et cliquez sur **Use As Depth Stream**. CyGameCapture la linéarise en `R16G16B16A16_UNORM`. Réglez le **plan lointain**, le **Z inversé**, la **profondeur logarithmique** et l’**inversion verticale** selon le jeu, comme pour les réglages de profondeur ReShade. Des paramètres incorrects produisent une image inversée, presque blanche ou géométriquement fausse. Choisissez la profondeur de la scène principale, pas une carte d’ombres. Vérifiez les distances en vous déplaçant.

## Interface seule {#hud-on-its-own}

1. Capturez la scène propre en flux 1, depuis une cible distincte ou un moment intermédiaire.
2. Sélectionnez l’image finale en fin de frame, généralement le back buffer.
3. Cliquez sur **Isolate HUD Against Stream 1**.
4. Ajustez `HudThreshold` et `HudSoftness` pour rejeter les écarts de scène sans effacer le texte. `HudKeepFinalColour` conserve les couleurs finales de l’interface.

Le HUD dérivé correspond à la différence entre l’image finale et la scène propre, accompagnée d’un masque de transparence. Les zones opaques peuvent être exactes ; les contours doux et les effets restent approximatifs. Le mouvement, un post-traitement différent ou un mauvais moment peuvent laisser des résidus de scène. Si le jeu possède déjà un render target HUD transparent, capturez-le directement en préservant son alpha.

Les deux traitements dérivés fonctionnent en D3D11 et D3D12. Cela ne rend pas disponibles les moments intermédiaires en D3D12. Pour enregistrer un HUD transparent, choisissez le RGB 8 bits sans perte ; les autres chemins d’enregistrement perdent l’alpha.
