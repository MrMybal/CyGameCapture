# Performance

## Design rules

* No `FlushRenderingCommands` per frame (only on a receiver's connection event, a rare occurrence).
* No GPU readback, no CPU texture copy, no `WaitForGPU` on the render thread.
* Persistent resources: the shared texture and the D3D12 intermediates are created once and only re-created when
  the source size / format changes.
* A frame skipped by the frame-rate limiter costs **zero** GPU work: the check happens before anything is issued.
* If the render thread is more than one frame behind, the game thread drops the frame instead of queueing
  another copy (`Dropped Frames` in the stats).

## Measured (RTX 3090, UE 5.5, host test harness, 4 senders + 1 loop-back receiver)

Streams: 1280×720 BGRA8 (render target), 640×360 RGBA16F (render target), 960×540 BGRA8 (scene capture),
1280×720 RGB10A2 (game viewport), plus a receiver reading the first sender back into a render target.

| RHI | Sustained frame rate | Frames sent per sender (25 s) | Dropped | CPU cost of issuing one copy |
|-----|---------------------|-------------------------------|---------|------------------------------|
| D3D11 | 172–179 fps | ~3790 | 0–1 | 0.001–0.008 ms |
| D3D12 | 115–120 fps | ~2800 | 0–9 | 0.001–0.004 ms |

The D3D12 figures include the extra copy of the D3D11On12 bridge and its fence wait (on a worker thread). The
difference between the two RHIs on this scene also reflects the general D3D12 overhead of the test project, not
only the plugin.

`Copy Time Ms` in the statistics is the **CPU** time spent issuing the copy (RHI thread / worker), not GPU time:
measuring GPU time would need timestamp queries with a readback, which the plugin deliberately avoids.

## Reproducing the benchmark

```
UnrealEditor-Cmd.exe <Project>.uproject -game -windowed -resx=1280 -resy=720 -dx11 ^
    -CyGCTest -CyGCTestSeconds=25 -log -abslog="Bench_dx11.log" -unattended -nosplash
```

Flags of the host harness: `-CyGCTest` (spawn the test actor), `-CyGCTestSeconds=N` (auto exit),
`-CyGCReceive=Name1,Name2` (receivers to create), `-CyGCNoViewport` (skip the viewport sender).
The log prints a report every 2 seconds with per-stream resolution, format, fps, frame count, drops and copy time.

In a real project, `CyGameCapture.Debug 1` shows the same numbers on screen, and `stat unit` / `stat gpu` give
the overall cost in context.

## Guidelines

| Goal | What to do |
|------|------------|
| Lowest latency | D3D11, `Every Frame`, `Flush After Copy = true` (costs CPU, saves at most one frame) |
| Lowest overhead | limit each sender to what the consumer needs (`Custom Frame Rate`) |
| 4K streams | prefer BGRA8 over RGBA16F (half the bandwidth); one 4K copy is ~2–3× the cost of a 1080p one |
| VR | keep the viewport sender at 60–72 fps rather than the full HMD rate, and check `stat unit` on the render thread |
| Many streams | senders share nothing, so cost scales linearly: 8 × 1080p copies ≈ 8 × one copy |

## Memory

Each sender allocates one shared texture (W × H × bytes-per-pixel) plus, on D3D12, three intermediates of the
same size. Each connected receiver allocates its opened shared texture reference (no extra memory) plus, on
D3D12, one intermediate. Example: 1080p BGRA8 sender = 8 MB (D3D11) or 32 MB (D3D12).
