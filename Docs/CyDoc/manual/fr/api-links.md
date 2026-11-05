---
id: api-links
title: API et index des sources publiques
description: Interfaces C++ natives et Unreal réellement distribuées avec le produit
section: API
order: 90
sourceRevision: 1
---

## Interfaces natives {#native}

- [API Windows d'aide contextuelle](doc:native-help-api#parameters), extraite de l'annotation réelle de l'en-tête C++ (référence anglaise).
- [Synchronisation des frames](doc:source-cygamecapturecore-include-cygamecapturecore-framesync-hpp#source).
- [Noms des émetteurs](doc:source-cygamecapturecore-include-cygamecapturecore-sendernaming-hpp#source).
- [Métadonnées des flux](doc:source-cygamecapturecore-include-cygamecapturecore-streaminfo-hpp#source).

## Unreal Engine {#unreal-api}

- [Propriétés, événements et exemples de l'émetteur](doc:ue-sender).
- [Propriétés, événements et exemples du récepteur](doc:ue-receiver).
- [En-tête du sous-système](doc:source-cygamecaptureue-source-cygamecaptureue-public-cygamecapturesubsystem-h#source).
- [En-tête de la bibliothèque Blueprint](doc:source-cygamecaptureue-source-cygamecaptureue-public-cygamecaptureblueprintlibrary-h#source).

L'index ne parcourt que les en-têtes publics Core distribués et le répertoire Public Unreal. Il conserve code et commentaires, crée les liens d'inclusion/dépendance et les ancres de lignes, sans résoudre les surcharges C++, macros ou mécanismes de réflexion Unreal. Aucune base Doxygen, aucun projet TypeDoc, aucune implémentation privée, source du moteur Unreal, source de SDK tiers ni configuration utilisateur n'est indexé. Le C# Build.cs configure la compilation ; ce n'est pas une API C# destinée à l'utilisateur.
