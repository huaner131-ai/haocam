# Building HaoCam

## Requirements (full desktop app)

* Windows 10/11
* Visual Studio 2022 (Desktop development with C++), or the VS Build Tools
* CMake 3.21+ (bundled with VS)
* Qt 6.6+ (6.8 LTS recommended) with the **Qt Base**, **Qtdeclarative**
  (Quick/QuickControls2), **QtShaderTools** modules - an msvc2022_64 install

Optional (later phases): FFmpeg dev files, SDK drop-ins under `sdk/`.

## Configure + build (Windows)

Set `QT_CMAKE_PREFIX_PATH` to your Qt installation's CMake prefix, then:

```powershell
cmake --preset windows-base
cmake --build --preset windows --config Release
```

The executable lands in `build/windows/app/Release/haocam.exe` (QML modules
and resources are compiled in; no deploy step is required beyond the standard
Qt runtime DLLs - run `windeployqt` if you plan to ship it).

Equivalent Ninja flow:

```powershell
cmake --preset windows-ninja
cmake --build --preset windows-ninja
```

## CMake options

| Option                            | Default | Meaning                                   |
|-----------------------------------|---------|-------------------------------------------|
| `HAOCAM_BUILD_APP`                | ON      | Qt/QML desktop application                |
| `HAOCAM_BUILD_TESTS`              | ON      | Platform-neutral core unit tests          |
| `HAOCAM_ENABLE_FACEBETTER`        | OFF     | Beauty provider (Phase 2)                 |
| `HAOCAM_ENABLE_OPENMAKEUP`        | OFF     | Makeup provider via web runtime (Phase 3) |
| `HAOCAM_ENABLE_SNAP`              | OFF     | AR provider via web runtime (Phase 4)     |
| `HAOCAM_ENABLE_FFMPEG`            | OFF     | Recording backend (Phase 5)               |
| `HAOCAM_ENABLE_VIRTUAL_CAMERA`    | OFF     | Virtual camera output (Phase 6)           |

Optional SDKs must never block the core build: every `HAOCAM_ENABLE_*` flag
currently maps to a status report in the UI (the providers arrive in their
phases).

## Core-only build (Linux/macOS/CI)

The engine core is platform-neutral and testable without Windows or Qt:

```bash
cmake --preset linux-core
cmake --build --preset linux-core
ctest --preset linux-core        # or run build/linux-core/tests/haocam_tests
```

## Troubleshooting the build

* `Qt6 not found` - check `QT_CMAKE_PREFIX_PATH`
  (e.g. `C:/Qt/6.8.2/msvc2022_64/lib/cmake`).
* HLSL embedding errors - `cmake/EmbedFile.cmake` runs at build time; make
  sure `src/graphics/shaders/*.hlsl` exist and are UTF-8.
* mingw/clang-cl builds work for syntax checking, but the supported desktop
  toolchain is MSVC.
