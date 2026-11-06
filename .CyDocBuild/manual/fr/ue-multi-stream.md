---
id: ue-multi-stream
title: Utiliser plusieurs flux Unreal
description: Donner un nom distinct à chaque vue et relier chaque flux à sa source OBS.
section: Unreal Engine
order: 20
parent: unreal
sourceRevision: 0bc3b7d261cd
---

## Nommer des vues indépendantes {#workflow}
Ajoutez un composant émetteur par caméra ou cible, avec des noms comme CameraA, CameraB et Depth. Gardez des noms distincts. AutoRename ajoute un suffixe numérique si un nom est déjà enregistré ; utilisez GetActiveSenderName au lieu de supposer que le nom demandé a été accepté sans changement.

Ajoutez une source OBS différente pour chaque nom réellement publié. Chaque flux augmente le travail GPU et utilise une place dans la table des émetteurs Spout commune à la machine. Une cadence personnalisée limite les copies, mais ne réduit pas nécessairement le coût d’une capture de scène exécutée séparément. Arrêtez les captures et émetteurs inutilisés. Le [guide d’enregistrement](doc:recording) explique les fichiers de sortie synchronisés.
