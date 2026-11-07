# Historique des versions

[English](CHANGELOG.md) · [Présentation de l’application](README.fr.md)

<a id="current-version"></a>

## 0.1.0 — Bêta · mise à jour des paquets du 6 novembre 2026

CyGameCapture 0.1.0 est une version **Bêta**, disponible pour les essais et la validation avant une version stable. Les composants de l’application conservent la version 0.1.0 ; le plugin Unreal est également en 0.1.0. La documentation utilise le lecteur CyDoc 0.1.0, fixé à la révision amont `6ddf9a5` ; il s’agit de la version d’une dépendance, pas d’une version distincte de l’application.

[Application publiée et paquets séparés](https://github.com/MrMybal/CyGameCapture/releases/tag/v0.1.0) · [Empreintes de contrôle](https://github.com/MrMybal/CyGameCapture/releases/download/v0.1.0/SHA256SUMS.txt) · [Suivi des problèmes](https://github.com/MrMybal/CyGameCapture/issues)

### Changements dans les téléchargements actuels

- Livraison du manuel CyDoc sous la forme `Documentation/Documentation.html`, `DocAI.md` et `DocData/`. L’anglais est la langue par défaut ; les 37 pages ont une traduction française complète avec les mêmes identifiants de pages et de sections.
- Inclusion des six captures de l’application, de trois schémas SVG statiques, du zoom d’image, des annotations et de la comparaison. Seul le module image est activé ; Three.js n’est pas inclus.
- Ajout d’une navigation hiérarchique, d’une recherche en haut accessible par Ctrl+K, d’infobulles utiles, de thèmes et de la mémorisation des positions de lecture. Le même lecteur est publié sur GitHub Pages tout en conservant son fonctionnement local via `file://`, sans serveur.
- Mise à jour des aides ReShade, OBS et Unreal et de F1 dans ReShade pour ouvrir la page ou section locale correspondante. Conservation d’alias pour les anciens liens de chapitres.
- Présentation des procédures utilisateur, des signatures d’API, des paramètres, des includes et d’exemples courts. Retrait de l’exploration des implémentations complètes, des notes internes et des pages de maintenance documentaire du lecteur humain.
- Inclusion du manuel complet et des textes de licences anglais applicables dans les paquets natifs, OBS et Unreal. Les réglages utilisateur restent séparés des fichiers installés. CyGameCaptureCore reste la bibliothèque native du produit ; ni CyToolsCore ni CyUEMCPSDK n’est inclus.
- Livraison des composants natifs Win64 recompilés et des plugins Unreal Editor précompilés pour les versions 5.3 à 5.8.

### Comportements corrigés

- Les flux couleur et les aperçus ne disparaissent plus lorsqu’un jeu écrit un RGB valide avec un alpha nul, comme observé dans Stray. La sortie couleur rend l’alpha opaque sans modifier le RGB ; les flux de HUD calculés conservent leur masque. **Preserve source alpha (advanced)** rétablit la transparence d’origine d’un flux couleur lorsqu’elle est nécessaire.
- Le diagnostic du récepteur indique l’alpha réel sur deux bits des flux RGB10A2 et signale une erreur lorsqu’une image reçue ne peut pas être écrite sur disque.
- La génération documentaire traite de façon cohérente les fins de ligne Windows et Linux lors du contrôle de la traduction française des API.

### Limites connues

- La chaîne de capture cible Windows x64 D3D11/D3D12. La prise en charge de D3D10 est limitée ; Vulkan et OpenGL ne sont pas pris en charge.
- La capture à une écriture intermédiaire et la recherche de buffers par IA nécessitent D3D11. Les moments intermédiaires ne prennent pas en charge le MSAA. Le remplacement de l’image en jeu par F10 exige un buffer de taille et de format compatibles.
- Les sources ciblent Unreal 4.27–5.8. Les archives Editor précompilées couvrent 5.3–5.8 ; les compilations Unreal 4.27, 5.0–5.2 et les cibles Shipping n’ont pas été vérifiées. Une propriété Blueprint ne prouve pas à elle seule qu’un comportement est implémenté ; le réglage Auto Reconnect exposé par l’émetteur ne fournit actuellement pas de récupération automatique après une réinitialisation du périphérique.
- Le partage Spout reste local au PC. Utilisez le même GPU pour l’émetteur et le récepteur. La table système des émetteurs possède 64 entrées par défaut, partagées entre les applications Spout ; la charge GPU et la mémoire limitent le nombre de flux utilisables.
- Un HUD isolé par soustraction d’images est approximatif sur les contours doux et certains effets. Un render target transparent contenant déjà le HUD seul est préférable lorsqu’il existe.
- L’enregistrement matériel/YUV ne conserve ni l’alpha d’origine ni toute la précision de profondeur. Le mode FFV1 RGB 8 bits conserve l’alpha du HUD ; le chemin 16 bits 4:4:4 utilise du YUV sans alpha et ne conserve pas toutes les valeurs unorm 16 bits d’origine à travers OBS.
- Les clients IA sont optionnels et s’installent séparément. Le fournisseur choisi reçoit les images échantillonnées, les métadonnées et la conversation via le compte utilisateur ; la disponibilité des modèles, la facturation et les autorisations dépendent de ce fournisseur.

La version publiée ne fournit pas de liste des bugs ouverts ticket par ticket. Consultez le [véritable suivi des problèmes](https://github.com/MrMybal/CyGameCapture/issues) et signalez les problèmes reproductibles ; ces limites ne signifient pas qu’il n’existe aucun autre bug.

## 0.1.0 — première version · 22 septembre 2026

### CyGameCaptureRS

- Inspecteur de buffers : cibles de rendu et profondeur, vignettes, tableau triable, filtres de formats/résolutions/écritures, stabilité et aperçus GPU en direct.
- Publication Spout2 d’un ou plusieurs buffers sélectionnés sous le nom `CyGameCaptureRS::<Jeu>`, avec des flux supplémentaires numérotés. La résolution dynamique est mise à l’échelle vers une sortie de taille stable.
- Moments D3D11 avant le dessin du HUD ; flux de profondeur linéarisée et de HUD isolé.
- Affichage du buffer sélectionné avec F10, recherche IA optionnelle via Claude Code, Codex ou OpenCode et interface anglais/français.

### CyGameCaptureOBS

- Découverte des émetteurs CyGameCapture/Spout2, reconnexion du récepteur et affichage de la texture partagée sans rapatriement de l’image sur le CPU.
- Enregistrement de plusieurs flux dans des fichiers synchronisés séparés, avec encodage matériel et modes FFV1.
- Interface anglais/français ; compilation avec les interfaces d’OBS Studio 29.1.3.

### CyGameCaptureUE

- Plusieurs émetteurs et récepteurs Spout2 pour les render targets, les scene captures et le viewport du jeu, accessibles en Blueprint et C++.
- Intégration Win64 D3D11/D3D12 et paquets précompilés distincts pour Unreal Engine 5.3–5.8.
- Licence MIT ; les composants natifs ReShade, assistant IA, Core et OBS utilisent AGPL-3.0-or-later.
