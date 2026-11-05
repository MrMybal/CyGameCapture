---
id: native-help-api
title: API native Windows d’aide contextuelle
description: Ouverture des pages CyDoc livrées depuis ReShade et OBS
section: API
kind: api
order: 100
sourceRevision: c99527e3e46c
---

Ouvrir une page CyDoc livrée avec le composant natif ReShade ou OBS.

## URL de fichier locale {#local-file-url}

`OpenUserGuideFile(file, french, page, anchor, media)` vérifie l’index local, transforme son chemin avec les API Windows, normalise l’encodage UTF-8 puis ajoute la route avec l’exemple d’intégration C++17 de CyDoc. Les espaces, accents, caractères Unicode, signes `%` et `#` du chemin sont traités avant l’ajout du fragment. Aucun repli HTTP n’est utilisé.

## Paramètres et résultat {#parameters}

`file` désigne l’`index.html` installé ; `french` sélectionne `fr` ou `en` ; `page` est un identifiant de page stable. Les paramètres facultatifs `anchor` et `media` ciblent une section ou une image.

Le résultat indique si Windows a accepté la demande d’ouverture locale, et non si le navigateur a terminé le rendu. Une absence de fichier ou une erreur d’association retourne `false` : le composant affiche alors son message traduit.

`OpenUserGuideBesideModule` résout le dossier de l’add-on plutôt que le répertoire courant du jeu.
