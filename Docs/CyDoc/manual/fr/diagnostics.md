---
id: diagnostics
title: Tester et vérifier un enregistrement
description: CyGameCapture — Tester et vérifier un enregistrement
section: Guide utilisateur
order: 15
sourceRevision: c9e128024b22
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

Lors des mesures de profondeur, les chemins matériel, sans perte 8 bits et sans perte 16 bits ont conservé respectivement 214, 249 et 4945 valeurs distinctes. Ce sont des résultats de test, pas des garanties universelles. L’étiquette « 16 bits » de l’encodeur ne prouve pas à elle seule la conservation des 16 bits d’origine.
