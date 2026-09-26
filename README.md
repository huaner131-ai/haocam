# HaoCam

A modern, Windows-first **beauty camera** for VTubers, streamers, content
creators and everyday webcam users - built around a GPU-first Direct3D 11
pipeline with replaceable effect providers (beauty / makeup / AR).

> **Status: Phase 1 complete** (see `docs/` for the full roadmap).
> Phase 1 delivers the application shell, Media Foundation camera capture,
> the D3D11 GPU pipeline, live preview, camera controls and diagnostics -
> with honest provider plumbing for the beauty/makeup/AR SDKs that arrive in
> Phases 2-4.

## Highlights

* **Camera**: Media Foundation Source Reader, GPU-resident NV12 frames,
  720p/1080p/1440p modes, `1920x1080 @ 60` preferred, device hotplug
  recovery with backoff.
* **GPU pipeline**: single D3D11 device shared with Qt Quick; NV12 -> BGRA8
  color pass (BT.709) + composite pass; texture pooling; timestamp-query GPU
  timing; one final texture feeds every output.
* **Effect architecture**: `IEffectProvider` + `EffectManager` +
  `EffectGraph` implementing the canonical 10-stage order. Facebetter
  (beauty), OpenMakeupSDK (makeup) and Snap Camera Kit (AR) plug in during
  Phases 2-4; until then each slot reports `Unavailable` and the app keeps
  working - never a fake integration.
* **UI**: dark, modern camera layout (preview-dominant, bottom tab bar:
  Camera / Beauty / Makeup / AR / Filters / Record), settings overlay,
  developer diagnostics overlay (F3).
* **Tests**: platform-neutral core (JSON, frame queue, pools, camera format
  selection, effect graph, event bus, settings) - 27 unit tests, buildable
  on any OS.

## Build & run

Windows (VS 2022 + Qt 6.6+/6.8 LTS):

```powershell
$env:QT_CMAKE_PREFIX_PATH = "C:/Qt/6.8.2/msvc2022_64"
cmake --preset windows-base
cmake --build --preset windows --config Release
.\build\windows\app\Release\haocam.exe
```

Core + tests (any OS, no Qt required):

```bash
cmake --preset linux-core
cmake --build --preset linux-core
ctest --preset linux-core
```

See `docs/BUILD.md` for details and `docs/ARCHITECTURE.md` for the design.

## Repository layout

```
app/       Qt application bootstrap            src/        engine core (camera, frame, effects, graphics, face, core)
ui/        QML + controllers                   tests/      core unit tests
docs/      architecture & guides               web/        Web Runtime (Phases 3-4)
sdk/       SDK drop-in location (gitignored)   assets/     presets and media
tools/     build-time utilities                cmake/      CMake helpers
```

## License

MIT - see [LICENSE](LICENSE). Third-party SDKs are never committed; each
keeps its own license and is opt-in via CMake flags and user configuration.
