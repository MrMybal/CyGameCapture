---
id: install
title: Installer et mettre à jour
description: CyGameCapture — Installer et mettre à jour
section: Guide utilisateur
order: 1
sourceRevision: dfbab9405563
---

## Prérequis {#requirements}

Utilisez Windows 10/11, un jeu 64 bits et un moteur DirectX compatible. D3D11 et D3D12 sont les chemins de capture testés ; la prise en charge D3D10 est limitée. La capture à un moment de la frame et l’analyse IA nécessitent D3D11. Vulkan et OpenGL ne sont pas pris en charge par ce pipeline. Choisissez un jeu dans lequel ReShade est autorisé et fonctionne normalement.

## ReShade {#reshade}

1. Installez **ReShade 6.8.0 avec prise en charge des add-ons** pour le jeu et son API graphique réelle. La version standard ne charge pas cet add-on ; l’API ciblée est la 20.
2. Extrayez l’archive ReShade. Copiez ensemble `CyGameCaptureRS.addon64`, `CyGameCaptureAI.exe`, `Documentation` et les licences près de l’exécutable du jeu, ou dans le répertoire d’add-ons configuré dans ReShade.
3. Lancez le jeu. Ouvrez ReShade avec `Home`. Repérez **CyGameCaptureRS – Buffer Inspector**. Son en-tête propose le bouton **Guide utilisateur**.

## OBS {#obs}

Utilisez OBS Studio 64 bits, version 29.1 ou ultérieure (testé avec 29.1.3). Fermez OBS. Extrayez toute l’archive OBS et lancez `Install-OBS-Plugin.cmd`, ou utilisez l’installateur du dossier de test. Relancez OBS et ajoutez une source **CyGameCapture**. Un récepteur Spout2 générique convient également. Les propriétés de la source native proposent **Guide utilisateur** ; le manuel est installé avec ses données.

## Mettre à jour ou retirer {#update-or-remove}

Fermez le jeu et OBS avant de remplacer les fichiers utilisés. Conservez un ancien paquet pour revenir en arrière. Remplacez ensemble les binaires, l’assistant, la documentation et les licences. Gardez votre `ReShade.ini`, vos réglages OBS et vos collections de scènes. Ne remplacez pas la configuration de votre jeu par l’INI de démonstration. Pour retirer l’add-on, supprimez ses fichiers de son répertoire d’installation ; utilisez le désinstallateur fourni pour OBS. L’installation Unreal est décrite au [chapitre 12](doc:unreal).
