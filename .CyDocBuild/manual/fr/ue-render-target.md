---
id: ue-render-target
title: Diffuser une cible de rendu existante
description: Partager une texture déjà produite par Unreal sans rendre une seconde caméra.
section: Unreal Engine
order: 20
parent: ue-sender
sourceRevision: ce7b309068af
---

## Partager une texture existante {#workflow}
Créez ou sélectionnez une UTextureRenderTarget2D, réglez Source Mode de l’émetteur sur Render Target puis affectez cet objet. Démarrez l’émetteur lorsque la source est disponible. Cette méthode copie la texture existante sans effectuer un second rendu de la scène.

Utilisez RGBA8 pour une sortie SDR ordinaire. Les textures HDR flottantes nécessitent un récepteur compatible ; la copie n’effectue pas de tonemapping. L’[API de l’émetteur](doc:ue-sender) indique les formats sources pris en charge. Si l’image est vide, affichez d’abord la cible dans Unreal et vérifiez que votre caméra, Canvas ou matériau y écrit effectivement.
