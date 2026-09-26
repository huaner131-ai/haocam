# SDK Integration Research

Findings as of **2026-09-26**, gathered from official documentation before
any integration work (spec section 37). Re-verify at integration time.

## Facebetter (Beauty - Phase 2)

| Question | Finding | Source |
|---|---|---|
| Supported platforms | iOS, Android, Web, Windows, macOS, Linux (desktop C/C++ API) | facebetter.net, github.com/pixpark/facebetter-sdk |
| Native C++ on Windows | Yes (desktop C++ sample: unzip SDK into `demo/cpp/sdk/`, CMake build) | github.com/pixpark/facebetter-sdk |
| License/auth | Developer account; `app_id` + `app_key` configured per app | facebetter.net docs (EngineConfig) |
| GPU texture sharing | SDK advertises GPU rendering pipeline & realtime video processing; exact interop formats to verify at integration (D3D11 texture input vs internal upload) | facebetter.net product overview |
| WebView integration | Not required on desktop | - |
| Key APIs (verified SDK 2.0) | `SetLogConfig` -> `BeautyEffectEngine::Create(EngineConfig{app_id, app_key, license_token?, resource_path=resource.fbd, external_context=false})` -> `SetCallbacks(EngineCallbacks{on_engine_event: 0=license OK / 1=license failed / 100=init complete / 101=init failed, on_face_landmarks})` -> `SetSmoothing/SetWhitening/SetRosiness/SetSharpening`, `SetReshape(Reshape::FaceThin/EyeSize/NoseSlim/Jawbone, v)` ([-1,1]) -> `ImageFrame::CreateWithRGBA(data,w,h,stride)`, `frame->type=Video`, `ProcessImage` -> output `{Data,Width,Height,Stride}`. NO type-enable step in 2.0 (intensity>0 enables). | docs.facebetter.net/windows/quick-start + implement-beauty; demo/cpp in github.com/pixpark/facebetter-sdk |
| Redistribution | SDK download behind the vendor site; license terms to review before shipping binaries | facebetter.net/download |
| Action for HaoCam | Config mechanism for app_id/key (Settings, local-only storage). No credentials in repo. | spec section 5 |

## MediaPipe Face Landmarker (Tracking - Phase 2)

| Question | Finding |
|---|---|
| Integration | MediaPipe Tasks C++ (`FaceLandmarker`, LIVE_STREAM/VIDEO mode). Adapter: `src/face/MediaPipeFaceTracker.cpp` - the ONLY MediaPipe-aware TU. |
| Options | `model_asset_path` (.task bundle), `num_faces`, `min_face_detection/presence/tracking_confidence`, `output_face_blendshapes`, `output_facial_transformation_matrixes` (verified against the real header, github.com/google-ai-edge/mediapipe). |
| Per-frame call | `DetectForVideo(Image, timestamp_ms)` (VIDEO mode, monotonic ms) or the LIVE_STREAM callback; result: normalized landmarks (468), optional blendshapes + 4x4 pose matrix. |
| Model | `face_landmarker.task`, manual download into `assets/models/` - never downloaded at runtime, never committed (spec section 38). |
| Drop-in | `sdk/mediapipe/include` + `sdk/mediapipe/lib`; `HAOCAM_ENABLE_MEDIAPIPE=ON` detects it, otherwise the build stays green with `NullFaceTracker` (Unavailable). |
| Status in HaoCam | Adapter + GPU downscale/readback source implemented; compiled only with the SDK present. NOT compiled in the sandbox (no drop-in). |

## OpenMakeupSDK (Makeup - Phase 3)

| Question | Finding |
|---|---|
| Official distribution | Not found as of 2026-09 (only third-party references). Treat as web/WebGL per the product brief. |
| Native C++ | No evidence - do NOT pretend it is native (spec section 3). |
| Integration path | Web Runtime (WebView2) bridge once official access exists. |
| Status in HaoCam | Adapter interface shipped; provider reports Unavailable; no fake integration. |

## Snap Camera Kit (AR - Phase 4)

| Question | Finding | Source |
|---|---|---|
| Supported platforms | iOS, Android, **Web**. No native Windows/macOS SDK. | developers.snap.com/camera-kit/home |
| Desktop story | Snap Camera (desktop app) shut down 2023-01; no official replacement. | ar.snap.com blog + press |
| Access/licensing | Application/approval process (ar.snap.com/camera-kit); API token per app; usage limits and ToS apply. | official Camera Kit docs |
| Web SDK | Camera Kit Web SDK (JS) intended for web apps - the sanctioned path for HaoCam's WebView2 runtime. | developers.snap.com/camera-kit/web |
| Input/Output | Frames into the SDK session, AR output composited by the SDK; lens groups configured in the developer portal. | official docs |
| Key risks | Token security (local storage only), lens group curation, ToS acceptance by the app publisher. | - |
| Action for HaoCam | Configuration mechanism for the API token; honest Unavailable state without it; never bundle or extract Snap assets. | spec sections 4/5/36 |

## Cross-cutting rules (spec section 5)

* Official SDK distributions and documented APIs only.
* No license/DRM/watermark/limit bypassing. No asset extraction from
  third-party apps. No reverse engineering of protections.
* User-provided credentials live in the local settings file
  (`%LOCALAPPDATA%/HaoCam/settings.json`); `sdk/` contents are gitignored.
* Every SDK slot has a functional placeholder that reports Unavailable so
  the rest of HaoCam keeps working.
