---
id: api-links
title: Intégrer les API publiques C++
description: Choisir les interfaces publiques natives ou Unreal de votre intégration.
section: API de développement
order: 20
sourceRevision: 9cff5741a04b
---

## Interfaces natives {#native}
Le [nommage et les métadonnées d’image](doc:core-api) expliquent les conventions natives partagées. L’[API d’aide Windows](doc:native-help-api#parameters) ouvre une page, une section ou une image du manuel installé. Les en-têtes appartiennent à CyGameCaptureCore ; les composants natifs conservent leur licence AGPL.

## Interfaces Unreal {#unreal-api}
Utilisez le module CyGameCaptureUE et ses includes publics. Commencez par le [composant émetteur](doc:ue-sender), le [composant récepteur](doc:ue-receiver) ou le [sous-système et les fonctions Blueprint](doc:ue-subsystem). Conservez les objets propriétaires, vérifiez les échecs de création et libérez les flux que vous possédez directement. Le plugin Unreal est sous licence MIT.

Ces références décrivent les appels publics pris en charge et de courts exemples. Elles ne constituent ni un compilateur, ni un éditeur Blueprint, ni une garantie de stabilité des détails d’implémentation privés.
