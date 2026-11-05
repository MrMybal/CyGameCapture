---
id: unreal
title: Émetteur et récepteur Unreal Engine
description: CyGameCapture — Émetteur et récepteur Unreal Engine
section: Guide utilisateur
order: 11
sourceRevision: 040f39d0fe22
---

Installez le paquet correspondant à votre version Unreal dans `<Projet>\Plugins\CyGameCaptureUE`, activez le plugin et redémarrez. Les archives précompilées couvrent les versions indiquées dans leur nom. Les sources ciblent UE 4.27–5.8 ; la compilation 4.27 n’est pas encore vérifiée. Les RHI pris en charge sont Win64 D3D11 et D3D12. Le manuel hors connexion se trouve aussi dans `Docs/UserGuide/index.html` du plugin.

## Envoyer une image {#send-an-image}

1. Ajoutez un composant **CyGameCapture Spout Sender** à un acteur.
2. Choisissez **Render Target** (affectez un `UTextureRenderTarget2D`), **Scene Capture** (composant existant ou vide pour en créer un), ou **Game Viewport** (aucun second rendu de scène).
3. Choisissez le nom du flux et gardez **Auto Start** activé. Begin Play publie `CyGameCaptureUE::Projet::Flux`.
4. Sélectionnez cet émetteur dans OBS ou un autre récepteur Spout.

Les scene captures gérées exposent une taille et un format, et coûtent un rendu de scène supplémentaire. Une fréquence personnalisée limite les copies GPU. Les cibles prises en charge comprennent RGBA8, RGBA8 sRGB, RGB10A2, RGBA16f et RGBA32f. Les autres formats produisent une erreur ; les images HDR flottantes demandent un récepteur adapté ou une conversion SDR. Les politiques de collision sont Fail, Auto Rename ou Replace If Owned By This Process. N’attribuez pas le même nom fixe à plusieurs flux actifs.

## Recevoir une image {#receive-an-image}

1. Ajoutez un composant **CyGameCapture Spout Receiver**.
2. Indiquez le nom exact ; **Get Available Spout Sender Names** liste les candidats.
3. Gardez **Internal Render Target** et **Auto Connect** pour une sortie gérée. Le mode User Render Target demande votre propre cible avec redimensionnement automatique facultatif.
4. Sur **On Connected**, récupérez **Get Receiver Texture** et affectez-la au paramètre de texture d’un matériau dynamique. Pour UMG, utilisez un matériau UI sur un widget Image.

Les événements signalent connexion, déconnexion, arrivée de frame, taille, format et erreurs. Les événements de frame peuvent être limités en fréquence. Le redimensionnement conserve l’objet render target mais peut recréer sa texture interne. Les flux s’arrêtent avec EndPlay et la destruction du monde.

## Réglages et diagnostic {#settings-and-diagnosis}

Project Settings → Plugins → CyGameCapture contrôle les valeurs initiales, la capture éditeur et les limites. Le PIE dans le viewport principal capture le back buffer de toute la fenêtre éditeur ; utilisez New Editor Window ou Standalone pour isoler le jeu.

```text
CyGameCapture.ListSenders
CyGameCapture.ListReceivers
CyGameCapture.Spout.List
CyGameCapture.Debug 1
```

Les composants Blueprint exposent Start/Stop, les sources, l’état de connexion et les statistiques. En C++, utilisez `UCyGameCaptureSubsystem`, `CreateSender`, `CreateReceiver` et les destructions correspondantes. Les références développeur détaillées sont livrées dans les autres fichiers `Docs/*.md` du plugin.


## Aide locale {#local-help}

Utilisez **Aide → Guide utilisateur CyGameCapture** dans l’éditeur, ou `CyGameCapture.Help`. La langue actuelle de l’éditeur choisit anglais ou français. `CyGameCapture.Help fr url` affiche l’URL locale encodée de la page Unreal sans ouvrir de navigateur.
