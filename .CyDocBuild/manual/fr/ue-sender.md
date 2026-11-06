---
id: ue-sender
title: Émetteur Unreal : propriétés et API
description: Configurer l’émetteur, consulter ses appels publics et réagir à ses événements.
section: Unreal Engine
order: 20
parent: unreal
sourceRevision: 4aaf4ab41141
---

## Configurer le composant {#properties}
Incluez `CyGameCaptureSpoutSenderComponent.h` et ajoutez le module `CyGameCaptureUE` aux dépendances du Build.cs du jeu. Le composant Blueprint se nomme CyGameCapture Spout Sender.

| Propriété | Comportement |
| --- | --- |
| StreamName | Nom court utilisé par la convention de nommage. |
| bUseCustomSenderName, SenderName | Utilisation d’un nom Spout personnalisé exact. |
| NameCollisionPolicy | Fail, AutoRename ou ReplaceIfOwnedByThisProcess. Le remplacement ne prend jamais l’émetteur d’un autre processus. |
| SourceMode | RenderTarget, SceneCapture ou GameViewport. |
| RenderTarget, SceneCapture | Objets sources existants ; une SceneCapture vide permet une caméra gérée. |
| SceneCaptureSize, SceneCaptureFormat | Taille et format d’une cible de scène gérée. |
| FrameRateMode, CustomFrameRate | Chaque image ou une cadence maximale personnalisée. |
| bAutoStart | Démarrage lors de BeginPlay. |
| bFlushAfterCopy | Soumission immédiate des copies D3D11 ; augmente le travail CPU. |
| bDebug | Messages détaillés du flux. |

La propriété exposée bAutoReconnect de l’émetteur n’implémente pas la récupération après réinitialisation du périphérique dans le composant actuel. Après une telle réinitialisation, examinez l’erreur puis recréez ou redémarrez la source au lieu de compter sur ce réglage.

## Appels publics {#functions}
| Signature | Résultat et comportement |
| --- | --- |
| `bool StartSender()` | Démarre la source configurée ; renvoie false si sa préparation échoue. Consultez OnError. |
| `void StopSender()` | Libère le flux de ce composant. |
| `bool IsSenderActive() const` | Indique si le composant possède un émetteur actif. |
| `void SetSenderName(const FString& NewSenderName)` | Définit un nom complet personnalisé ; redémarre un émetteur actif. |
| `void SetRenderTarget(UTextureRenderTarget2D* NewRenderTarget)` | Sélectionne le mode RenderTarget et la cible fournie. |
| `void SetSceneCapture(USceneCaptureComponent2D* NewSceneCapture)` | Sélectionne le mode SceneCapture. |
| `void UseGameViewport()` | Sélectionne le back buffer final. |
| `FString GetActiveSenderName() const` | Nom réellement publié, vide avant le démarrage. |
| `FIntPoint GetSenderResolution() const` | Largeur et hauteur actuelles. |
| `FString GetSenderFormat() const` | Libellé du format actuel. |
| `float GetSenderFPS() const` | Cadence du flux. |
| `FCyGameCaptureStreamStats GetSenderStats() const` | Statistiques actuelles du flux. |
| `UTextureRenderTarget2D* GetSourceRenderTarget() const` | Cible source lorsqu’une cible est utilisée. |
| `USceneCaptureComponent2D* GetActiveSceneCapture() const` | Composant de capture existant ou géré. |

## Événements et durée de vie {#events}
OnStarted, OnStopped, OnResolutionChanged(Width, Height) et OnError(Error) signalent les changements d’état. EndPlay, la désinscription et la disparition du propriétaire ou du monde arrêtent le flux. Utilisez [l’API du sous-système](doc:ue-subsystem) pour gérer directement les flux en C++.

```cpp
#include "CyGameCaptureSpoutSenderComponent.h"
// Sender et Target désignent des objets vivants appartenant à votre acteur.
Sender->SetRenderTarget(Target);
if (Sender->StartSender()) {
    const FString PublishedName = Sender->GetActiveSenderName();
}
```

Les cibles RGBA8, RGBA8 sRGB, RGB10A2, RGBA16f et RGBA32f sont prises en charge. Les cibles flottantes contiennent des valeurs HDR linéaires et nécessitent un récepteur adapté. Un format incompatible produit une erreur ; changer un nom de fichier ou un libellé de format ne convertit pas les couleurs.
