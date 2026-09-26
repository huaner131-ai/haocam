# HaoCam Architecture

HaoCam is a Windows-first, GPU-first beauty camera for VTubers, streamers and
webcam users. The architecture rule that governs everything:

```
UI  ->  HaoCam API  ->  Provider Interface  ->  SDK Adapter  ->  External SDK
```

The UI **never** talks to Facebetter, OpenMakeupSDK or Snap directly, and the
SDKs are never wired to each other. HaoCam owns the pipeline; the SDKs are
replaceable providers.

## Layer map

```
                Qt / QML UI  (ui/qml, dark modern camera layout)
                     |
        ui/controllers (Qt controllers, GUI thread)
                     |
        +------------+---------------------------+
        |                                        |
  EngineController                         VideoView (QQuickItem)
        |                              (scene-graph render thread)
        v                                        |
  haocam_core (platform-neutral C++20)           |
  +------------------------+                     |
  | camera/  CameraManager |<-- frames --+       |
  | effects/ EffectManager |             |       |
  | face/    IFaceTracker  |             v       |
  | frame/   Frame model   |      D3D11 compositor
  | core/    log/cfg/etc.  |      (Windows-only files)
  +------------------------+             |
                     |                   |
              FINAL GPU TEXTURE  <-------+
                     |
        +------------+------------+
        v            v            v
     Preview     Recording    Virtual Camera
    (QRhi       (Phase 5,     (Phase 6)
   import)       FFmpeg)
```

## Threading model (spec section 32)

| Thread           | Owner            | Responsibility                                   |
|------------------|------------------|--------------------------------------------------|
| GUI thread       | Qt               | QML, controllers, settings                        |
| Camera thread    | `MediaFoundationCapture` | Source Reader `ReadSample` loop, GPU frame copies |
| Watchdog thread  | `CameraManager`  | FPS measurement, reconnect with backoff           |
| Engine thread    | `EngineController` | consumes FrameQueue(3), drives `EffectManager::process` (Phase 2) |
| Tracking thread  | `TrackingWorker` | latest-slot face tracking + smoothing (Phase 2)   |
| Beauty thread    | `FacebetterProvider` | Facebetter engine init/retry + per-frame process (Phase 2) |
| Scene-graph thread | Qt Quick       | Preview presentation via QRhi import              |
| Web runtime      | Phase 3/4        | WebView2 bridge for OpenMakeup / Snap             |

Locking: one `std::mutex` per pool/ring, atomics for frame ids and stats.
The camera->engine handoff is a bounded SPSC `FrameQueue` (drop-oldest).

## Frame flow (Phase 1, Windows)

1. Media Foundation opens the UVC camera with `IMFDXGIDeviceManager` wrapping
   the **Qt Quick D3D11 device** (`VideoView` obtains it through
   `QSGRendererInterface` on the first rendered frame).
2. The Source Reader outputs NV12. Samples surface as `ID3D11Texture2D`
   (GPU-resident). Decoder array slices are copied into a pooled texture with
   `SHADER_RESOURCE` binding (`CopySubresourceRegion`, GPU->GPU).
3. The frame travels through `core::FrameQueue<Frame>` (capacity 3).
4. `Compositor::process` runs the passes:
   `ColorConvert` (NV12->BGRA8 + brightness/contrast/saturation + mirror) and
   `Composite` (into the final ring texture). GPU time is measured with
   timestamp queries.
5. The final texture is published into a 3-slot ring; `VideoView` imports the
   newest slot with `QRhiTexture::createFrom` (same-device requirement is
   satisfied by design) and displays it via
   `QQuickWindow::createTextureFromRhiTexture`.

There is **one** effect pipeline and **one** final texture; outputs fan out
from it (recording and virtual camera will consume the same texture in later
phases, never re-run effects).

## Effect order (spec sections 19/26)

`EffectGraph` owns the canonical order:

```
CameraCapture, ColorConversion, FaceTracking, FaceReshape, Beauty,
Makeup, AR, ColorLut, FinalComposite, Outputs
```

Providers are attached per stage by `EffectManager`. Stage enable flags are
data: when an SDK is unavailable its stage is disabled and the pipeline
continues (spec section 21). The Settings UI lists every provider slot with
honest availability from `EffectManager::providerStatus()`.

## SDK isolation contracts

* `IBeautyProvider` (Phase 2, Facebetter) - smoothing/whitening/rosy/sharpen
  + face reshape.
* `IMakeupProvider` (Phase 3, OpenMakeupSDK through the web runtime) -
  foundation/blush/lipstick/eyeliner/mascara/eyeshadow.
* `IARProvider` (Phase 4, Snap Camera Kit through the web runtime) - lenses.

See `docs/SDK_INTEGRATION.md` for platform research and licensing rules.
