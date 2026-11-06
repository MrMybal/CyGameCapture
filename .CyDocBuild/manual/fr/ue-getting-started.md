---
id: ue-getting-started
title: Première capture dans Unreal
description: Installer le plugin et publier une première caméra ou cible de rendu nommée.
section: Unreal Engine
order: 20
parent: unreal
sourceRevision: d08ce07eccac
---

## Installer et démarrer {#start}
1. Fermez l’éditeur et copiez le paquet CyGameCaptureUE adapté dans `<Project>/Plugins/CyGameCaptureUE`.
2. Activez le plugin, redémarrez et sélectionnez D3D11 ou D3D12 sous Windows. Spout ne fonctionne pas avec Vulkan ni NullRHI.
3. Ajoutez un composant **CyGameCapture Spout Sender** à un acteur. Donnez à Stream Name un nom utile, par exemple CameraA.
4. Affectez une cible de rendu ou choisissez Scene Capture. Gardez Auto Start activé et lancez la lecture.
5. Dans OBS, ajoutez une source CyGameCapture et sélectionnez le nom réellement publié.

## Choisir une image {#source}
Les [cibles de rendu](doc:ue-render-target) publient une texture existante. Les [captures de scène](doc:ue-scene-capture) rendent séparément la vue d’une caméra. La [capture du viewport](doc:ue-viewport) partage l’image finale de la fenêtre, interface comprise. Le nom d’un flux suit normalement `CyGameCaptureUE::<Project>::<Stream>` ; consultez Get Active Sender Name après le démarrage, car une collision peut ajouter un suffixe.

## Arrêter proprement {#stop}
Arrêtez l’émetteur avec Stop Sender ou quittez la lecture pour libérer le flux. Un composant émetteur possède un flux ; utilisez plusieurs composants pour des vues indépendantes. Consultez les [commandes de l’émetteur](doc:ue-sender), la [réception](doc:ue-receiver) et le [dépannage](doc:ue-troubleshooting).
