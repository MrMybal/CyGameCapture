---
id: ue-receiver
title: Récepteur Unreal : propriétés et API
description: Connecter un émetteur exact et afficher sa texture avec un matériau dynamique.
section: Unreal Engine
order: 20
parent: unreal
sourceRevision: a6962ad4778c
---

## Connecter un émetteur {#properties}
Incluez `CyGameCaptureSpoutReceiverComponent.h` du module `CyGameCaptureUE`. SenderName doit correspondre au nom publié exact. ReceiverName est seulement le nom d’affichage local. bAutoConnect démarre lors de BeginPlay ; bAutoReconnect poursuit la recherche lorsque le producteur disparaît.

OutputMode sélectionne InternalRenderTarget ou UserRenderTarget. La cible interne s’adapte automatiquement au producteur. Le mode utilisateur utilise TargetRenderTarget et bResizeTargetAutomatically. FrameEventRateHz limite les appels d’OnFrameReceived ; zéro désactive cet événement sans arrêter la réception GPU.

## Appels publics {#functions}
| Signature | Résultat et comportement |
| --- | --- |
| `bool StartReceiver()` | Lance la réception ; la reconnexion automatique peut attendre un producteur absent. |
| `void StopReceiver()` | Libère la connexion. |
| `bool IsReceiverActive() const` | Indique si la réception fonctionne. |
| `bool IsConnected() const` | Indique si un producteur est connecté. |
| `void SetSenderName(const FString& NewSenderName)` | Change de producteur et reconnecte un récepteur actif. |
| `void SetTargetRenderTarget(UTextureRenderTarget2D* NewTarget)` | Sélectionne une cible de sortie. |
| `UTextureRenderTarget2D* GetReceiverTexture() const` | Cible à utiliser dans un matériau ; vérifiez qu’elle n’est pas nulle. |
| `FIntPoint GetReceiverResolution() const` | Dimensions actuelles. |
| `FString GetReceiverFormat() const` | Libellé du format actuel. |
| `float GetReceiverFPS() const` | Cadence de réception. |
| `FCyGameCaptureStreamStats GetReceiverStats() const` | Statistiques de réception. |

## Afficher la texture {#material}
Créez un matériau avec un paramètre Texture Sample Parameter 2D nommé SpoutTexture. Lors d’OnConnected, récupérez Get Receiver Texture et affectez-le avec Set Texture Parameter Value sur une instance dynamique du matériau. Dans UMG, utilisez un matériau dont Material Domain vaut User Interface sur la brosse du widget Image. Ne transmettez pas une cible de rendu à Set Brush from Texture : son entrée de texture n’est pas une entrée de cible de rendu.

```cpp
#include "CyGameCaptureSpoutReceiverComponent.h"
if (UTextureRenderTarget2D* Texture = Receiver->GetReceiverTexture()) {
    MaterialInstance->SetTextureParameterValue(TEXT("SpoutTexture"), Texture);
}
```

## Événements et changements {#events}
OnConnected, OnDisconnected, OnFrameReceived, OnResolutionChanged, OnFormatChanged et OnError décrivent l’état du récepteur. Le sous-système interroge les informations du producteur à l’intervalle ReceiverPollIntervalSeconds. Si le redimensionnement automatique est désactivé, une cible incompatible peut conserver l’ancienne image figée. Placez les deux applications sur le même adaptateur GPU et utilisez D3D11/D3D12.
