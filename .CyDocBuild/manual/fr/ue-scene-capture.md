---
id: ue-scene-capture
title: Diffuser une caméra dédiée
description: Configurer une capture de scène existante ou gérée et maîtriser son coût de rendu.
section: Unreal Engine
order: 20
parent: ue-sender
sourceRevision: b4aad0d9f356
---

## Configurer la capture de scène {#workflow}
Choisissez Scene Capture sur l’émetteur. Affectez une USceneCaptureComponent2D existante ou laissez Scene Capture vide pour que le composant en gère une. Si la capture ne possède pas de TextureTarget, le composant crée une cible avec Scene Capture Size et Scene Capture Format. Positionnez et configurez la capture pour obtenir la vue voulue.

Cette capture effectue un rendu distinct de la caméra du joueur. Ses indicateurs de visibilité, son post-traitement et sa source de capture déterminent l’image obtenue. Elle coûte un rendu de scène supplémentaire ; partager une cible de rendu existante est moins coûteux si elle contient déjà la bonne image.
