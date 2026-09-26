# GPU Pipeline (Phase 1 + Phase 2 tracking path)

## Principles honored (spec section 18)

1. Frames stay GPU-resident: camera NV12 -> pooled NV12 (GPU copy) -> BGRA8
   final texture, no CPU round trip on the hardware path.
2. The only CPU involvement is metadata, uniform updates and, on the rare
   software-converter path, a single upload.
3. Textures come from `D3D11TexturePool` (size/format/bind-keyed reuse).
4. The camera->engine queue is capacity-3 drop-oldest (bounded latency).
5. One pipeline, one final texture; preview/recording/virtual-camera consume
   it (recording/virtual camera arrive in Phases 5/6).

## Single-device design

The effect pipeline runs on the **Qt Quick scene graph's own D3D11 device**:

* `VideoView::updatePaintNode` fetches it once via
  `QSGRendererInterface::getResource(window, DeviceResource/DeviceContextResource)`
  and hands it to `EngineController::attachRenderDevice`.
* Media Foundation wraps the same device (`IMFDXGIDeviceManager`), and
  `ID3D11Multithread::SetMultithreadProtected(TRUE)` is enabled so the camera
  thread, engine thread and scene-graph thread can submit safely.
* The final texture is imported back into the scene graph with
  `QRhiTexture::createFrom` and displayed via
  `QQuickWindow::createTextureFromRhiTexture` - Qt renders the quad with its
  own state management; HaoCam adds no custom Qt shaders.

This avoids fragile cross-device resource sharing (keyed mutexes, NT
handles) in Phase 1. `IRenderDevice` keeps the door open for a standalone
device + sharing when Direct3D 12 support lands.

## Passes (src/graphics/shaders)

| Pass        | Stage (spec 19)      | Input            | Output          | Notes                                   |
|-------------|----------------------|------------------|-----------------|-----------------------------------------|
| ColorConvert| 2 Color Conversion + 8 Color/LUT | NV12 (R8 + R8G8 SRVs) | pooled BGRA8 | BT.709 limited-range expansion, brightness/contrast/saturation, mirror |
| Composite   | 9 Final Composite    | processed BGRA8  | final ring BGRA8| letterbox-free 1:1; aspect handled by UI sizing |
| TrackerCopy | (off-graph helper)   | processed BGRA8  | BGRA8 RT <=480px | `tracker_ps.hlsl` swizzles BGRA->RGBA in the target; fullscreen VS |

Shaders are embedded into the binary at build time
(`cmake/EmbedFile.cmake`) and compiled at startup with `D3DCompile`
(d3dcompiler_47 ships with Windows). The source of truth is the `.hlsl`
files.

## Output ring

`Compositor` publishes into 3 ring slots. `VideoView` displays the newest
slot and reports consumption via `notifyOutputConsumed`; the engine waits
(bounded) before overwriting an unconsumed slot so the preview never tears.

## Phase 2: tracker + beauty pixel traffic (readback tradeoff, spec section 20)

Frame pixels stay GPU-resident end to end. Two consumers need pixels that
only exist on the GPU (MediaPipe CPU inference, Facebetter CPU processing);
HaoCam handles both with ONE controlled readback per consumer, never a
full-resolution download:

1. **Tracking**: `D3D11PixelSource` downscales the last composited texture
   to <=480 px wide on the GPU, then copies it into a 2-slot staging ring
   (reused, mapped with normal read usage). At 16:9 that is a ~0.9 MB
   readback per *tracked* frame (30/s default) - not per camera frame
   (up to 60/s) and not per preview frame. The copy + read time is
   measured (`TrackingStats.lastProcessMs` covers read + infer).
   Rationale: MediaPipe FaceLandmarker runs on CPU with RGBA bytes; the
   downscale halves the tracked-pixel count vs 720p and keeps the upload
   at a fixed small size regardless of camera resolution.
2. **Beauty (Facebetter)**: `GpuFrameCopier::readBGRA` copies the full
   processed frame into a 2-slot staging ring only while the beauty engine
   is actually processing; the result is uploaded back into a pooled BGRA8
   texture (`GpuFrameCopier::uploadBGRA`) and re-enters the normal GPU
   composite as the freshness-gated override. All buffers reused; the
   individual costs surface in the F3 overlay (readback/sdk/upload ms).

Neither path allocates per frame (spec section 35). If the beauty engine
stalls, the freshness gate simply keeps the un-beautified GPU frame - the
camera never waits for a readback.

## Diagnostics

D3D11 timestamp queries measure GPU time per frame (one frame of latency);
CPU wall time and process FPS are measured around `process()`. All values
surface in the F3 diagnostics overlay (FPS, frame time, GPU time, CPU time,
camera resolution, active stages, dropped frames, pool size).
