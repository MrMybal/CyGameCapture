---
id: troubleshooting
title: Dépannage
description: CyGameCapture — Dépannage
section: Guide utilisateur
order: 14
sourceRevision: 51882c1bcfa4
---

| Symptôme | Vérifications et correction |
| --- | --- |
| L’add-on n’apparaît pas | Vérifiez jeu 64 bits, exécutable/dossier correct, version ReShade avec add-ons/API 20 et `ReShade.log`. Les anciennes API ne le chargent pas. |
| Aperçu de l’inspecteur vide | Choisissez une cible couleur récemment écrite. Désactivez l’aperçu figé. Vérifiez taille/format et l’erreur ; attendez une copie si vous venez de choisir un moment. Essayez le back buffer en fin de frame. |
| Fonctionne en jeu, rien dans le flux | La vue F10 et la capture sont distinctes. Cliquez sur Use As Capture Source, vérifiez état actif, images envoyées et aperçu de sortie, puis choisissez cet émetteur exact dans OBS. |
| Aperçu correct, OBS transparent/noir | Désactivez la préservation d’alpha pour la couleur. Vérifiez visibilité/transformation de source, émetteur, GPU commun et format accepté. Les masques de HUD doivent conserver l’alpha. |
| HUD encore visible / scène incomplète | Mauvais buffer ou moment. Avancez si des objets manquent, reculez si le HUD apparaît. Testez jeu et menus : les effets peuvent utiliser des écritures différentes. |
| Mauvaises couleurs / image très sombre | Buffer HDR intermédiaire ou de données, espace couleur incorrect ou réglages de profondeur. Choisissez une couleur après tonemapping ; le HDR flottant demande un récepteur adapté. |
| Sortie figée | Désactivez l’aperçu figé pour l’inspection, vérifiez producteur et compteurs, conservation de dernière image dans OBS, resélection après redimensionnement et reconnexion. |
| Aucun émetteur / trop d’émetteurs | Démarrez la capture et lisez les erreurs ; vérifiez collisions de noms et capacité Spout de la machine. Fermez les producteurs inutiles. |
| IA indisponible / sans réponse | D3D11 uniquement. Vérifiez assistant, exécutable du client et compte connecté. Lisez les fichiers prompt/answer ; réessayez avec du jeu visible. Le réseau/fournisseur peut bloquer l’analyse facultative. |
| Fond opaque dans l’enregistrement du HUD | Utilisez FFV1 RGB 8 bits sans perte. Les chemins matériel/YUV et 4:4:4 16 bits ne gardent pas l’alpha. |
| Perte de détail dans la profondeur enregistrée | Contrôlez la précision décodée avec VideoProbe. La conversion interne OBS empêche de garder tous les 16 bits unorm ; lisez Spout directement si chaque bit compte. |
| Le bouton du guide n’ouvre rien | Restaurez tout le dossier Documentation/UserGuide près de l’add-on ou réinstallez les données du plugin OBS. Ouvrez index.html manuellement pour vérifier l’association du navigateur. |

Pour un signalement utile, indiquez version du produit, jeu/exécutable, API graphique, GPU, versions ReShade/OBS, buffer/format/moment, nom d’émetteur et première étape en échec (sélection → sortie → récepteur → enregistrement). Ajoutez une capture et un extrait de journal en masquant chemins privés et informations de compte.
