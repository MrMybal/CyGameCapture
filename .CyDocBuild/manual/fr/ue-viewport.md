---
id: ue-viewport
title: Partager le viewport du jeu
description: Partager l’image finale et choisir la bonne fenêtre PIE ou autonome.
section: Unreal Engine
order: 20
parent: ue-sender
sourceRevision: 2d3d79d68c42
---

## Capturer l’image finale de la fenêtre {#workflow}
Choisissez Game Viewport ou appelez UseGameViewport(). Aucune cible de rendu n’est nécessaire. L’émetteur copie le back buffer final sans effectuer le rendu d’une seconde caméra. L’interface déjà composée dans cette fenêtre reste dans le flux ; choisissez Scene Capture ou une cible de rendu propre existante pour produire une vue séparée.

La lecture PIE intégrée utilise le back buffer de la fenêtre de l’éditeur, qui peut inclure ses panneaux. Utilisez New Editor Window ou Standalone pour une vue isolée du jeu. La capture dans l’éditeur dépend aussi des réglages du plugin dans le projet. Un redimensionnement peut changer les dimensions du flux ; vérifiez le récepteur après un changement de mode d’affichage.
