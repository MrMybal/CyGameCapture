# Receiver — `UCyGameCaptureSpoutReceiverComponent`

Receives any Spout2 sender (OBS, Resolume, TouchDesigner, another Unreal instance, CyGameCaptureRS, custom
tools) into an Unreal `UTextureRenderTarget2D`, entirely on the GPU.

## Properties

| Category | Property | Meaning |
|----------|----------|---------|
| Receiver | **Receiver Name** | local name shown in statistics / console output |
| Receiver | **Sender Name** | the Spout sender to connect to (use *Get Available Spout Sender Names* to list them) |
| Behaviour | **Auto Connect** | start on BeginPlay |
| Behaviour | **Auto Reconnect** | keep polling while the sender is missing, reconnect when it reappears |
| Behaviour | **Frame Event Rate Hz** | rate of the `On Frame Received` Blueprint event; 0 disables it (never floods the game thread at 120 Hz) |
| Output | **Output Mode** | `Internal Render Target` (the component owns one, always matched to the sender) or `User Render Target` |
| Output | **Target Render Target** | your own render target (User mode) |
| Output | **Resize Target Automatically** | re-create the user target when the sender size / format changes |
| Debug | **Debug** | per-stream verbose logging |

## Blueprint nodes (category *CyGameCapture | Receiver*)

`Start Receiver`, `Stop Receiver`, `Is Receiver Active`, `Is Connected`, `Set Sender Name`,
`Set Target Render Target`, **`Get Receiver Texture`**, `Get Receiver Resolution`, `Get Receiver Format`,
`Get Receiver FPS`, `Get Receiver Stats`.

Events: `On Connected`, `On Disconnected`, `On Frame Received` (throttled), `On Resolution Changed (W, H)`,
`On Format Changed (Format)`, `On Error (Error)`.

## Use the image in a material

```
Event BeginPlay
 └─► Create Dynamic Material Instance (target: your mesh / TV screen)  ──► store as MID

Event On Connected (receiver component)
 └─► Get Receiver Texture ──► MID: Set Texture Parameter Value (Parameter Name = "SpoutTexture")
```

The material needs a `Texture Sample Parameter 2D` named `SpoutTexture`. Since the render target is created
with the sender's format, 8-bit streams arrive as sRGB textures (correct colours) and float streams as linear
HDR. `On Resolution Changed` re-creates the render target, and the same `UTextureRenderTarget2D` object is kept,
so the material parameter does not need to be re-assigned.

## Use the image in UMG

Two options:

1. **Material**: build a UI material (Material Domain = User Interface, Blend Mode = Opaque/Translucent) with a
   `Texture Sample Parameter 2D`. Assign it to an `Image` widget brush and set the parameter with a dynamic
   material instance as above.
2. **Direct brush**: `Image → Set Brush from Texture (Get Receiver Texture)`. A `UTextureRenderTarget2D` is a
   `UTexture` so it works directly; Match Size or a custom Image Size sets the display size.

## Resolution and format changes

The sender may change size (window resize, OBS canvas change) or format at any time. The receiver polls the
sender info every `ReceiverPollIntervalSeconds` (project settings, 0.25 s by default), re-opens the shared
texture, re-creates the target when allowed and fires `On Resolution Changed` / `On Format Changed`. While the
target does not match the sender no copy is done (the image freezes on the last frame instead of tearing).

## C++

```cpp
FCyReceiverStreamConfig Config;
Config.ReceiverName = TEXT("OBS_Program");
Config.SenderName   = TEXT("OBS_Program");     // whatever OBS publishes
FString Error;
auto Receiver = UCyGameCaptureSubsystem::Get()->CreateReceiver(Config, Owner, Error);
Receiver->SetTargetRenderTarget(MyRenderTarget);
Receiver->OnConnected.AddLambda([](FCyReceiverStream& S)
{
    const FCySpoutSenderInfo Info = S.GetConnectedSenderInfo();   // size, format, owner executable
});
```

## Troubleshooting

| Symptom | Cause |
|---------|-------|
| `Is Connected` stays false | no sender with that exact name (`CyGameCapture.Spout.List` shows the real names) |
| Connected but black / frozen | user target with `Resize Target Automatically = false` and a different size/format |
| `OpenSharedResource failed` in the log | the sender runs on another GPU adapter (laptop hybrid graphics): force both applications on the same GPU |
| No image on a Vulkan build | Spout needs D3D11/D3D12 (`-dx11` / `-dx12`) |
