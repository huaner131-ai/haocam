# Windows Verification Status

**Status: NOT runtime-verified in Phase 2.** (Last updated: 2026-09-27)

## Why

The Phase 2 development environment is a Linux sandbox with:

* no Windows hardware, no Windows VM, no GPU device;
* no Qt 6 runtime (the Qt/QML application is therefore **never compiled
  here**, in Phase 1 or Phase 2 - see below);
* no Facebetter SDK drop-in and no Facebetter credentials;
* no MediaPipe Tasks C++ drop-in and no `.task` model.

Per the project rule "never claim untested functionality", nothing in this
file is a runtime claim. The table below states exactly what was and was
not verified for the Phase 2 changes.

## What WAS verified (Linux sandbox)

| Check | Result |
|---|---|
| `linux-core` configure + build (GNU 12.2, CMake preset) | clean, no warnings from new code |
| Unit tests (platform-neutral core) | **62/62 pass**, including 27 Phase-1 tests + 35 new Phase-2 tests |
| New test coverage | FaceData/landmarks/pose conventions, LandmarkSmoother (adaptive, gap, layout change), NullFaceTracker, FaceTrackingResult selection, BeautyConfig defaults/clamp/JSON, NullBeautyProvider honesty, AppConfig credentials parsing, graph order tracking->beauty->composite |
| Windows cross-compile of changed native TUs | `zig c++ -target x86_64-windows-gnu` object-compile of `MediaPipePixelSource.cpp`, `GpuFrameCopier.cpp`, `Compositor.cpp`, `EffectManager.cpp` (incl. `tracker_ps.hlsl` embed headers) - all clean |

Cross-compiling a TU is **not** linking, running, or QML validation.

## What was NOT verified (requires Windows hardware + SDKs)

* Qt/QML application build (needs Qt 6.6 + MSVC) and moc of the new
  `BeautyController` / rewritten `EngineController` /
  `DiagnosticsController` - first compile happens on Windows.
* The full preview pipeline (camera -> D3D11 -> composite -> QRhi import).
* `FacebetterProvider` beyond API-accuracy against official docs: the
  engine was never created (no SDK/credentials). Mapping table and
  threading design are verified against docs; runtime behavior is not.
* `MediaPipeFaceTracker` beyond API-accuracy against the real Tasks
  headers: never compiled (no SDK), never run (no model).
* F3 diagnostics overlay extension, Beauty panel sliders, provider status
  pills, config.json credential loading at runtime.
* Performance numbers on Windows (nothing measured; no claims made).

## First-run checklist for a Windows machine

1. Install Qt 6.6+ (MSVC), CMake 3.21+, Ninja; `cmake --preset windows`
   and build (see docs/BUILD.md).
2. Facebetter (optional): copy the SDK into `sdk/facebetter/`
   (include/lib/resource), configure `%LOCALAPPDATA%\HaoCam\config.json`
   from `config.example.json`, enable `HAOCAM_ENABLE_FACEBETTER=ON`.
3. MediaPipe (optional): prebuilt Tasks SDK into `sdk/mediapipe/`, the
   `face_landmarker.task` model into `assets/models/`, enable
   `HAOCAM_ENABLE_MEDIAPIPE=ON`.
4. Run, press F3, and confirm: engine thread starts, tracking/beauty rows
   report honest status, sliders function, camera recovery still works.
5. Report measured numbers (tracking ms incl. readback, beauty
   readback/sdk/upload ms, preview FPS) back into this file.
