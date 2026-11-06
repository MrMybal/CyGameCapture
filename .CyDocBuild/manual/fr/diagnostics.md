---
id: diagnostics
title: Tester et vérifier un enregistrement
description: Utiliser le récepteur et le décodeur fournis pour examiner une image ou une vidéo.
section: Guide utilisateur
parent: obs
order: 15
sourceRevision: cb201c3a28a9
---

Le dossier de test contient des applications D3D11/D3D12 avec une scène simple et un faux HUD, ainsi qu’un récepteur Spout. Il permet de distinguer un problème d’installation d’un problème propre aux buffers d’un jeu.

```text
CyGameCaptureSpoutReceiverTest.exe --list
CyGameCaptureSpoutReceiverTest.exe --frames 3 --out C:\Captures
```

Choisissez un dossier de sortie existant et accessible en écriture. Le récepteur liste les émetteurs CyGameCapture et enregistre les frames reçues en BMP. S’il reçoit la bonne image, examinez la source/le format OBS plutôt que la sélection du buffer.

L’outil facultatif **VideoProbe** décode un fichier et indique codec, format des pixels, profondeur des composantes et valeurs distinctes :

```text
CyGameCaptureVideoProbe.exe "take.mkv" 2
CyGameCaptureVideoProbe.exe "take.mkv" 2 --dump 3 "C:\Captures\Alpha"
```

Les indices de composantes dépendent du format décodé ; la composante 3 est l’alpha uniquement si le format le contient réellement. L’outil utilise les bibliothèques FFmpeg installées avec OBS. Un format décodé sans alpha prouve que celui-ci n’a pas été enregistré, même si la texture Spout d’origine en possédait un.
