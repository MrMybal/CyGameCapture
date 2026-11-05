---
id: performance
title: Performances et compatibilité
description: CyGameCapture — Performances et compatibilité
section: Guide utilisateur
order: 13
sourceRevision: 5f1d4067c90b
---

- Un flux normal demande une copie GPU par frame ; D3D12 utilise un pont D3D11On12 avec un travail supplémentaire. Les flux dérivés et les moments ajoutent des passes/copies GPU. Le coût CPU affiché n’est pas le coût total du GPU ou de l’encodeur.
- La résolution, le nombre de flux, les encodeurs et le débit disque influencent le résultat. Commencez par un flux, puis ajoutez-en en surveillant FPS, images ignorées et charge GPU.
- Le produit n’impose pas un nombre fixe de flux. La table Spout de la machine contient 64 émetteurs par défaut, partagés avec les autres applications. Arrêtez les producteurs inutiles. Les utilisateurs avancés peuvent modifier `HKCU\Software\Leading Edge\Spout\MaxSenders` via les réglages Spout. Les récepteurs ne consomment pas de places d’émetteur.
- Les limites de sécurité, comme les 256 flux de l’add-on et StreamSanityLimit dans Unreal, protègent d’une création incontrôlée. Elles ne garantissent pas les performances.
- Une texture HDR flottante, de normales ou d’éclairage n’est pas une image SDR prête à l’affichage. Choisissez la scène après tonemapping ou prévoyez une conversion chez le producteur. Format et résolution doivent convenir au récepteur.
- Des textures peuvent disparaître après un changement de mode. Actualisez, resélectionnez et vérifiez après un redimensionnement, un changement graphique ou de scène.

| Fonction | D3D11 | D3D12 |
| --- | --- | --- |
| Capture couleur Spout, plusieurs flux | Oui | Oui, avec pont |
| Profondeur et HUD dérivé | Oui | Oui |
| Moment d’écriture intermédiaire | Oui, sans MSAA | Non |
| Recherche de buffer par IA | Oui | Non |
