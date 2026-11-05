---
id: assistant
title: Utiliser l’assistant IA
description: CyGameCapture — Utiliser l’assistant IA
section: Guide utilisateur
order: 9
sourceRevision: 6cf420d9e927
---

L’assistant utilise un client **Claude Code, Codex ou OpenCode** installé et connecté à votre compte. Il est facultatif ; la capture manuelle fonctionne sans lui. CyGameCapture n’intègre pas de modèle et ne stocke pas de clé API fournisseur.

1. En D3D11, affichez une véritable scène de jeu avec son interface visible.
2. Choisissez un client dans **Assistant IA** et cliquez sur la recherche. Une fenêtre de discussion s’ouvre.
3. L’add-on échantillonne des buffers et des moments dans une image GPU partagée. `CyGameCaptureAI.exe` lit ces images hors du jeu et demande au client choisi d’identifier une scène complète sans HUD.
4. Lisez l’explication et la proposition. **Show / Voir** la sélectionne pour inspection ; **Apply / Appliquer** l’utilise pour la capture ; **Appliquer et envoyer aussi l’interface seule** demande également un flux HUD dérivé.
5. Précisez par exemple « les sous-titres restent ». Gardez **Joindre l’image actuelle** activé pour fournir des captures récentes. Envoyez avec le bouton ou `Ctrl`+`Entrée`. **Nouvelle discussion** remet la conversation à zéro.

La proposition contient un identifiant de buffer et, si nécessaire, un moment d’écriture. Elle est vérifiée par rapport à la frame analysée. Inspectez-la : un changement de scène peut l’invalider. Un chargement noir, un jeu masqué ou un HUD absent fournissent trop peu d’indices au modèle.

## Confidentialité et diagnostic {#privacy-and-diagnosis}

Le fournisseur du client choisi reçoit les images échantillonnées, les métadonnées des buffers et la conversation via votre compte. Les 12 derniers messages peuvent accompagner la requête suivante. L’activité peut consommer votre quota fournisseur. Les fichiers locaux se trouvent dans `%LOCALAPPDATA%\CyGameCapture\AI\<jeu>` : `prompt.txt`, `answer.txt` et `picture_NN.png`. Ils peuvent contenir des images du jeu et vos échanges. Supprimez-les lorsqu’ils sont inutiles et masquez les informations privées avant de les partager.

Réglages facultatifs : `AIClient`, `AIModel`, `AIClientPath` et `AILanguage`. Si un client est absent, installez-le et connectez-vous séparément, puis indiquez son exécutable si la détection échoue. Si le programme assistant manque, remettez-le près de l’add-on. L’analyse aux moments de la frame n’est pas disponible en D3D12.
