---
id: help-navigation
title: Lire, rechercher et naviguer dans le manuel
description: Lecture locale, médias, accessibilité et traductions
section: Guide utilisateur
order: 18
sourceRevision: 2
---

## Lecture locale {#local-reading}

Le lecteur CyDoc complet est livré avec CyGameCapture. Ouvrez `Documentation/UserGuide/index.html` depuis ReShade ou OBS, ou `Docs/UserGuide/index.html` dans le plugin Unreal. Aucun serveur, compte, CDN ni réseau n'est nécessaire. Le lecteur nécessite JavaScript. Les liens facultatifs vers le dépôt des sources nécessitent Internet.

L'en-tête ReShade ouvre la présentation. **F1** lorsque l'inspecteur a le focus ouvre l'aide de l'onglet courant : [inspecteur](doc:inspector), [capture](doc:capture) ou [Timeline/Debug](doc:inspector#timeline-and-debug). Le bouton du guide de la source OBS ouvre [la réception OBS](doc:obs). Les touches du jeu ne sont pas remappées lorsque l'overlay est fermé.

Dans Unreal Editor, **Aide → Guide utilisateur CyGameCapture** ouvre [le chapitre Unreal](doc:unreal). La commande console `CyGameCapture.Help [en|fr] [url]` fait de même ; le dernier argument facultatif `url` affiche uniquement l'adresse locale.

## Trouver une information {#search}

Utilisez la navigation latérale, le sommaire de page et la recherche. La recherche filtre guides, API, sources et médias, ou un langage de code. Les exemples sont colorés lors de la génération. Les pages source proposent des liens lexicaux d'inclusion et des liens vers les lignes ; ce ne sont ni un compilateur C++ ni un exécuteur Blueprint.

## Images et comparaisons {#images}

La barre de l'image permet de zoomer, recentrer ou agrandir dans cette fenêtre. Les contrôles apparaissent au survol ou au focus clavier et restent visibles sur écran tactile. **Échap** réduit une vue agrandie. Déplacez l'image en glissant ; zoomez avec la molette ou les boutons. Un curseur compare les captures réelles ; la scène étant animée, elles ne montrent pas le même instant.

[Ouvrir la liste des buffers annotée](doc:inspector?media=m1-buffer-inspector&note=buffer-list).

Les réglages des images donnent accès aux outils d'annotation. Vos annotations, favoris, notes et historique restent dans ce navigateur/appareil ; exportez-les pour les sauvegarder. Les politiques de stockage `file://` peuvent limiter leur conservation. La navigation garde les positions pendant la session ; les liens explicites de section/média sont prioritaires. Déplacer tout le manuel peut changer son contexte de stockage local.

## Langue et thème {#language-and-theme}

L'anglais est la langue par défaut. Choisissez Français pour conserver la page et l'identifiant commun de section. Les chapitres du guide d'origine sont disponibles dans les deux langues. Les références techniques uniquement anglaises utilisent le repli anglais ; les instructions de test uniquement françaises restent accessibles comme page française. Le [rapport des traductions](doc:translation-report) signale les traductions absentes ou dépassées sans présenter une copie anglaise comme une traduction française.

Le thème initial Nord reprend le bleu de CyGameCapture ; le sélecteur propose aussi des thèmes clairs, sombres et à contraste élevé. Tab parcourt contrôles et liens ; Entrée/Espace les activent. Le tactile utilise le menu et les contrôles de médias CyDoc. Aucune donnée de lecture n'est envoyée à un serveur.
