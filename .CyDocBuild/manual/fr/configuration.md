---
id: configuration
title: Langues, raccourcis et configuration
description: Changer langue, raccourcis et valeurs de capture sans perdre vos réglages.
section: Guide utilisateur
parent: preferences
order: 12
sourceRevision: 16843945a428
---

L’overlay démarre en **anglais**. Choisissez English ou Français près du logo ; le choix est conservé dans `ReShade.ini`. Le bouton du guide ouvre l’édition correspondante. OBS suit sa propre langue ; les langues non prises en charge utilisent le guide anglais. Les liens de langue du manuel conservent le chapitre courant. Le fichier initial `Documentation.html` est toujours en anglais.

Les réglages se trouvent dans `ReShade.ini`, section `[CYGAMECAPTURE]`. Fermez le jeu avant une modification manuelle et sauvegardez le fichier. Les noms des clés et leurs valeurs restent identiques dans les deux langues.

```ini
[CYGAMECAPTURE]
Language=fr
PreserveSourceAlpha=0
ViewToggleKey=121
AIClient=auto
```

| Clé | Défaut | Signification |
| --- | --- | --- |
| `Language` | `en` | Langue de l’overlay : en ou fr ; modifiable près du logo. |
| `SenderName` | `CyGameCaptureRS::<exe name>` | Nom de base personnalisé des émetteurs Spout. |
| `ViewToggleKey` | `121` (F10) | Code de touche Windows pour la vue en jeu ; 121 = F10, 0 désactive. |
| `PreserveSourceAlpha` | `0` | Valeur initiale des nouveaux flux couleur : 0 opaque, 1 alpha d’origine. Le HUD garde son masque. |
| `StandaloneWindow` | `1` | Afficher l’inspecteur dans sa fenêtre lorsque l’overlay est ouvert. |
| `FlushAfterCopy` | `0` | Soumettre immédiatement après copie ; ajoute du travail CPU. |
| `Verbose` | `0` | Journaliser chaque création/destruction de ressource pour diagnostic. |
| `AutoCapture` | vide | Amorçage/test : sélectionner par taille:format, par #ID ou #ID@écriture. Séparer les flux par ;. Suffixes :depth et :hud. Exemple : #5@2;#5:hud. Les identifiants peuvent changer au redémarrage. |
| `DumpBuffers` | `0` | À cette frame, écrire une fois la liste des buffers dans ReShade.log. |
| `AIClient` | `auto` | Client IA : auto, claude, codex ou opencode. Auto choisit le premier installé. |
| `AIModel` | vide | Modèle transmis au client ; vide pour son modèle par défaut. |
| `AIClientPath` | vide | Chemin complet de l’exécutable du client si la détection échoue. |
| `AILanguage` | langue de l’overlay | Langue des réponses IA : fr ou en ; vide pour suivre l’overlay. |
| `AISearchOnStart` | `0` | Test : lancer une analyse à cette frame, au plus tôt à la frame 60. |
| `OpenOverlayOnStart` | `0` | Test : ouvrir l’overlay une fois le jeu stabilisé. |
| `DepthFarPlane` | `1000` | Distance du plan lointain, mappée sur le blanc par la linéarisation. |
| `DepthReversed` | `0` | 1 lorsque le plan proche est encodé à 1 dans la profondeur du jeu. |
| `DepthLogarithmic` | `0` | 1 pour une profondeur logarithmique. |
| `DepthUpsideDown` | `0` | 1 si la profondeur est inversée verticalement par rapport à la couleur. |
| `HudThreshold` | `0.02` | En dessous de cet écart, considérer le pixel comme scène seule. |
| `HudSoftness` | `0.08` | Écart à partir duquel le masque HUD devient opaque. |
| `HudKeepFinalColour` | `1` | Garder les couleurs finales dans le HUD plutôt que la différence brute. |

`Home` ouvre ReShade par défaut (ReShade peut modifier cette touche). `F10` bascule l’image sélectionnée en jeu. `Ctrl`+`1`…`4` choisit inspecteur, capture, timeline et debug lorsque l’overlay a le focus. Les flèches gauche/droite parcourent les buffers. `Ctrl`+`Entrée` envoie un message à l’assistant.


Appuyez sur **F1** lorsque l’inspecteur ReShade a le focus pour ouvrir l’aide de son onglet courant. Timeline et Debug ciblent la même section explicite du guide de l’inspecteur.
