---
id: ue-subsystem
title: Sous-système C++ et fonctions Blueprint
description: Découvrir les émetteurs, gérer les flux et configurer le sous-système moteur.
section: Unreal Engine
order: 20
parent: unreal
sourceRevision: 7465db725128
---

## Includes et modules {#includes}
Ajoutez CyGameCaptureUE aux dépendances. Incluez `CyGameCaptureSubsystem.h` pour gérer directement les flux et `CyGameCaptureBlueprintLibrary.h` pour les fonctions de découverte utilisables en Blueprint. Le sous-système moteur possède les enregistrements de flux communs au processus ; les composants sont les propriétaires habituels des flux individuels.

## Découvrir et examiner {#discovery}
`UCyGameCaptureSubsystem::Get()` renvoie le sous-système ou null avant l’initialisation du moteur. IsSpoutSupported(), GetRHIName() et GetUnsupportedReason() expliquent sa disponibilité. GetAvailableSpoutSenders(bForceRefresh) liste les informations des producteurs ; FindSpoutSender(SenderName, OutInfo) indique si l’émetteur exact existe. MakeDefaultSenderName(StreamName) applique la convention de nommage du projet. GetAllStreamStats(Senders, Receivers) remplit les deux tableaux. StopAllStreams() arrête tous les flux de ce processus ; évitez cet appel lorsqu’un seul composant doit s’arrêter.

Les fonctions statiques d’UCyGameCaptureBlueprintLibrary exposent GetCyGameCaptureSubsystem(), IsSpoutSupported(), GetAvailableSpoutSenders(), GetAvailableSpoutSenderNames(), FindSpoutSender(), MakeDefaultSenderName() et GetAllStreamStats().

## Posséder directement un flux {#lifetime}
CreateSender(Config, Owner, OutError) et CreateReceiver(Config, Owner, OutError) renvoient des pointeurs partagés de flux utilisables entre threads. Vérifiez le pointeur renvoyé et OutError avant utilisation. Conservez le pointeur et associez-le à DestroySender(Stream) ou DestroyReceiver(Stream). Ne déréférencez jamais le résultat d’une création échouée. ResolveSenderName(RequestedName, Policy, OutFinalName, OutError) vérifie les collisions avant le démarrage.

```cpp
#include "CyGameCaptureSubsystem.h"
if (UCyGameCaptureSubsystem* Capture = UCyGameCaptureSubsystem::Get()) {
    const TArray<FCySpoutSenderInfo> Senders = Capture->GetAvailableSpoutSenders(true);
}
```

## Réglages du projet {#settings}
Project Settings → Plugins → CyGameCapture contrôle bEnableSpout, bAllowEditorCapture, le préfixe des émetteurs, leur limite, l’interrogation des récepteurs et les messages de diagnostic. Ces réglages ne rendent pas Vulkan compatible avec Spout. SetDebugOverlayEnabled(bool) modifie l’affichage du diagnostic à l’écran, sans changer le contenu capturé.
