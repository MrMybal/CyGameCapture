---
id: quickstart
title: Première capture sans interface
description: Envoyer une scène propre à OBS en gardant le HUD visible pour le joueur.
section: Guide utilisateur
parent: overview
order: 2
sourceRevision: 57401adcbbb7
---

1. Affichez une scène de jeu avec le HUD visible. Évitez un écran de chargement noir.
2. Ouvrez l’overlay avec `Home`, puis **Inspecteur de buffers**.
3. Choisissez dans les vignettes une image complète, aux bonnes couleurs, sans HUD. Vérifiez-la dans l’aperçu. Si elle n’existe pas, essayez [un moment avant le HUD](doc:moment) ou [l’assistant IA](doc:assistant).
4. Cliquez sur **Use As Capture Source** (utiliser comme source). Dans **Capture**, vérifiez la résolution, les FPS, les images envoyées et l’aperçu de sortie.
5. Dans OBS, ajoutez **CyGameCapture** et sélectionnez `CyGameCaptureRS::<Jeu>`. Actualisez la liste si nécessaire.
6. Fermez l’overlay ReShade. Gardez votre HUD habituel dans le jeu pendant qu’OBS reçoit la scène sélectionnée.

```image
{"id": "player-and-output", "src": "assets/m1_player_view_with_hud.png", "compare": "assets/m1_spout_receiver_clean_scene.png", "alt": "Jeu de test avec barres de HUD et panneau bleu", "labels": ["Joueur avec HUD", "Sortie Spout"], "controls": "minimal"}
```

Vue du joueur : le HUD reste visible.

Sortie Spout : scène propre. La scène animée a été capturée à un autre moment.
