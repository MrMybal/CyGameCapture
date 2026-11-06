---
id: recording
title: Enregistrer plusieurs buffers ensemble
description: Enregistrer plusieurs buffers dans des fichiers vidéo distincts et synchronisés.
section: Guide utilisateur
parent: obs
order: 8
sourceRevision: 2952ae3ded2f
---

Le plugin OBS natif peut enregistrer plusieurs buffers dans des **fichiers séparés et synchronisés**, indépendamment d’un enregistrement ordinaire du canevas OBS.

```diagram
{"type": "flow", "title": "Fichiers synchronisés séparés", "nodes": [{"id": "0", "label": "Frame N", "x": 20, "y": 20, "width": 260}, {"id": "1", "label": "Scène → Clean.mkv", "x": 310, "y": 20, "width": 260}, {"id": "2", "label": "HUD → HUD.mkv", "x": 20, "y": 150, "width": 260}, {"id": "3", "label": "Profondeur → Depth.mkv", "x": 310, "y": 150, "width": 260}], "edges": [{"from": "0", "to": "1"}, {"from": "0", "to": "2"}, {"from": "0", "to": "3"}]}
```

1. Créez une source OBS CyGameCapture par buffer avec un émetteur explicite.
2. Cochez **Enregistrer ce buffer** sur chaque source à inclure.
3. Choisissez le dossier de prise, l’encodage, le conteneur et la piste audio OBS (1 à 6).
4. Cliquez une fois sur **Démarrer l’enregistrement de tous les buffers sélectionnés**. Terminez avec **Arrêter l’enregistrement**.
5. Vérifiez chaque fichier, les compteurs, les images ignorées et le décalage annoncé. Les fichiers partagent l’horodatage de la prise et portent le nom de leur source/émetteur.

Chaque source dispose de son mix vidéo à la résolution native, de son encodeur et de son multiplexeur. Les producteurs publient les mêmes indices de frame ; l’enregistreur rapporte les différences quand il peut les vérifier. La synchronisation est mesurée ; cela ne garantit pas qu’aucune frame ne sera jamais manquée.

| Encodage | Utilisation | Limites |
| --- | --- | --- |
| Matériel (NVENC / AMF / QuickSync ; repli logiciel selon le choix) | Vidéo couleur habituelle, fichiers plus petits | La conversion YUV peut perdre précision, chrominance et alpha. Inadapté à la conservation de profondeur. |
| RGB 8 bits sans perte · FFV1 | Couleur 8 bits exacte et masques de HUD | Conserve l’alpha ; limité à 8 bits. |
| 4:4:4 16 bits sans perte · FFV1 | Profondeur plus précise | YUV, sans alpha. OBS utilise en interne des flottants demi-précision : les 16 bits unorm complets ne sont pas conservés. |

Préférez Matroska (`.mkv`) pour des prises récupérables. Un MP4 interrompu peut devenir inutilisable. Les modes sans perte passent par FFV1 et Matroska ; prévoyez de l’espace disque et vérifiez d’abord une courte prise. Une piste audio est un mix OBS : réglez son contenu dans les Propriétés audio avancées d’OBS. Chaque fichier peut recevoir le mix choisi ; cela ne capture pas directement le son du jeu.

**Démarrer la prise automatiquement** attend une image sur chaque source choisie et se déclenche une fois par exécution d’OBS. Désactivez-le pour un contrôle manuel. Les [outils de vérification](doc:diagnostics) permettent d’examiner ce que le fichier conserve réellement.
