# Face Tracking (Phase 2)

## What exists now

`src/face/` contains the provider-independent tracking stack:

| File | Role |
|---|---|
| `FaceData.h` | Shared per-face model: landmarks, pose, bounds, confidence |
| `FaceLandmarks.h/.cpp` | Semantic landmark access (`FaceLandmark` enum + `getLandmark`) + geometric pose fallback |
| `FaceTracker.h` | `IFaceTracker` contract, `FaceTrackerConfig`, `FaceTrackingResult` |
| `FaceTracking.cpp` | Factory: `createDefaultTracker()` (MediaPipe adapter when compiled in, else `NullFaceTracker`) |
| `LandmarkSmoother.h` | Adaptive (One Euro style) temporal smoothing |
| `TrackingWorker.h/.cpp` | Engine-side worker thread: latest-slot queue, pixel source, smoothing, stats |
| `MediaPipeFaceTracker.h/.cpp` | MediaPipe Tasks adapter (the ONLY file allowed to know MediaPipe) |
| `MediaPipePixelSource.h/.cpp` | Windows GPU downscale + readback path that feeds the tracker |

No MediaPipe types or landmark indices appear outside `MediaPipeFaceTracker.*`
/ `MediaPipePixelSource.*`; semantic access goes through the `FaceLandmark`
enum whose per-layout index maps live only in `FaceLandmarks.h`.

## Data conventions (spec sections 9-12)

* **Landmarks** are normalized `[0,1]` in frame space, origin top-left,
  x right, y down. MediaPipe landmarks are already normalized - they are
  copied as-is.
* **Bounds** (`FaceBounds{x,y,width,height}`) are the tight box of the
  landmarks, same normalized space, clamped to `[0,1]`.
* **Confidence** is the mean landmark presence score in `[0,1]`.
* **Head pose** (degrees, `[-180,180]`), documented in `FaceData.h`:
  * `yaw`   : positive = face turned to **their** left (viewer's right)
  * `pitch` : positive = looking **up**
  * `roll`  : positive = top of head tilted toward the **viewer's right**
* When the MediaPipe facial-transformation matrix is available, pose is
  extracted from it (row-major 4x4: `pitch=atan2(r21,r22)`,
  `yaw=-atan2(-r20,sy)`, `roll=-atan2(r10,r00)`). Otherwise the geometric
  fallback in `FaceLandmarks.cpp` runs (roll from the eye-line angle,
  yaw from nose offset vs. eye midpoint, pitch from nose position between
  forehead and chin). Both paths follow the same sign conventions above.

## Threading and freshness (spec sections 13/19)

```
camera thread -> FrameQueue(3) -> engine thread (EffectManager::process)
                                   |-> TrackingWorker (own thread, latest-slot)
                                   `-> beauty worker (Facebetter engine)
```

* The tracking worker keeps **only the latest** submitted frame; stale
  frames are dropped (counter exposed as `dropped` in the stats).
* Tracking never runs on the UI thread and never blocks the camera or
  engine thread.
* `maxTrackingFps` (default 30) throttles how often the tracker actually
  processes; intermediate frames are skipped.
* Results are smoothed with the adaptive filter below and published as an
  atomic snapshot; `EffectManager::latestTracking()` reads the newest one.

## Pixel path (GPU-preferred, spec section 20)

On Windows the tracker consumes GPU textures through `D3D11PixelSource`:
the last composited D3D11 texture is downscaled to <=480 px wide (aspect
preserved) with a GPU pass (`tracker_ps.hlsl` also swizzles to RGBA byte
order in the BGRA8 target), then read back once into a **reused** staging
buffer (2-slot ring, no per-frame allocation). The full-resolution texture
is never downloaded; the readback moves at most ~0.9 MB per processed
frame at 480x270 RGBA (~0.7 MB at 16:9). Cost is measured
(`TrackingStats.lastProcessMs` includes read + process). Without a D3D11
device (tests, non-Windows) a `CpuNv12PixelSource` converts NV12 -> RGBA
with reused buffers (BT.709 limited-range).

## Temporal smoothing (documented, spec section 19)

`LandmarkSmoother` implements the One Euro filter:

```
speed  = (x - prev) / dt
cutoff = minCutoff + beta * |speed|          (defaults 1.2, 0.007)
alpha  = 1 / (1 + 1 / (2*pi * cutoff * dt))
smooth = prev + alpha * (sample - smooth)
```

* Fast motion raises the cutoff -> larger alpha -> less lag.
* The first sample after start/reset passes through unchanged.
* A gap longer than 0.5 s passes through (no motion smear across stalls).
* Caches are resized only when the landmark count changes (no per-frame
  allocation, spec section 35).

## Failure behavior (spec sections 14/16/33)

| Situation | Behavior |
|---|---|
| No MediaPipe SDK compiled in | `NullFaceTracker`: `Unavailable`, empty results, zero crash |
| Model file missing (`assets/models/face_landmarker.task`) | tracker stays `Unavailable: model file not found ...`; camera + preview unaffected |
| No face in frame | `detected=false`; providers bypass; tracking FPS stays live |
| Tracker error at runtime | result flagged undetected, worker keeps running, error logged once |
| Tracking disabled in config | graph stage off; diagnostics show `Disabled in config.json` |

## Model policy (spec section 38)

The `.task` model is **never downloaded at runtime** and **never
committed**. Place it manually (see `assets/models/README.md`).
