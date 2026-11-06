---
id: ue-troubleshooting
title: Dépanner un flux Unreal
description: Diagnostiquer collisions de noms, différences de GPU, redimensionnement et HDR.
section: Unreal Engine
order: 20
parent: unreal
sourceRevision: 6a1b80fa126d
---

## Vérifier la compatibilité et les noms {#diagnosis}
Exécutez CyGameCapture.Spout.List pour voir les noms réels des producteurs. Consultez IsSpoutSupported et GetUnsupportedReason. Les deux applications doivent utiliser le même adaptateur GPU. Utilisez -dx11 ou -dx12 sous Windows ; Vulkan, les autres plateformes et NullRHI ne permettent pas le partage de textures Spout.

| Symptôme | Action |
| --- | --- |
| Aucun émetteur | Lancez la lecture, activez le plugin et vérifiez Auto Start ainsi que l’objet source. |
| Le nom reçoit ::2 | Un autre flux possède le nom demandé ; sélectionnez le nom réel ou choisissez-en un autre. |
| Le récepteur attend | Vérifiez le SenderName exact et la reconnexion automatique. |
| Sortie figée après redimensionnement | Activez l’adaptation automatique de la cible ou fournissez une cible compatible. |
| Couleurs HDR incorrectes | Utilisez une chaîne HDR compatible ou une source SDR ; la copie ne réalise pas de tonemapping. |
| Les panneaux de l’éditeur apparaissent | Utilisez New Editor Window ou Standalone au lieu de la lecture PIE intégrée. |

CyGameCapture.ListSenders, CyGameCapture.ListReceivers et CyGameCapture.Debug 1 aident à repérer le flux concerné. Désactivez les diagnostics détaillés après utilisation. Examinez l’événement OnError du composant avant de modifier des réglages sans rapport.
