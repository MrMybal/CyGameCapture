---
id: core-api
title: Nommage et synchronisation des flux natifs
description: Construire les noms d’émetteurs et lire les métadonnées sans accéder aux pixels.
section: API de développement
order: 20
parent: api-links
sourceRevision: cd81c9b44de9
---

## Noms et includes {#naming}
Utilisez C++17 et incluez `CyGameCaptureCore/SenderNaming.hpp`. SanitizeGameName(exe_stem) retire les suffixes de compilation pris en charge et remplace les caractères réservés aux séparateurs. MakeSenderName(game_name, instance) construit `CyGameCaptureRS::<Game>` ; une instance supérieure à un ajoute `::<N>`. Le nom final est limité à 255 octets.

```cpp
#include "CyGameCaptureCore/SenderNaming.hpp"
const auto Game = cygc::SanitizeGameName("Demo-Win64-Shipping");
const auto Sender = cygc::MakeSenderName(Game, 2); // CyGameCaptureRS::Demo::2
```

IsReShadeSender(), IsUnrealSender() et IsCyGameCaptureSender() identifient les noms ; SenderDisplayName() renvoie une vue sans le préfixe reconnu. Conservez la chaîne d’origine pendant l’utilisation de cette vue.

## Lire les métadonnées d’image {#synchronisation}
Incluez `CyGameCaptureCore/FrameSync.hpp` sous Windows. `std::vector<FrameSyncRecord> ReadFrameSync()` récupère les instantanés de métadonnées disponibles. La surcharge `bool ReadFrameSync(const std::string& sender_name, FrameSyncRecord& out)` remplit out pour un émetteur exact et renvoie false si aucun instantané n’est disponible.

Un enregistrement contient le nom de l’émetteur, l’indice d’image, le groupe du producteur, le temps, les dimensions, le format DXGI et le type de flux. Des indices identiques sont significatifs dans un même groupe de production, pas entre applications indépendantes. Les métadonnées ne contiennent pas les pixels et ne synchronisent pas des producteurs GPU indépendants. Les définitions du type de flux et des formats figurent dans `CyGameCaptureCore/StreamInfo.hpp`.
