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

The executable lands in `build/windows/Release/haocam.exe`.

IMPORTANT: close any running haocam.exe BEFORE rebuilding, otherwise the
linker fails with `LNK1104: cannot open file ...haocam.exe`:

    taskkill /IM haocam.exe /F 2>$null

If the old instance is wedged in a driver-level deadlock, it may not die even
with /F (threads stuck in kernel mode keep the image file locked). In that
case the only reliable way out is a reboot - which also resets a possibly
wedged USB camera stack. Validate the build actually succeeded
(`haocam.vcxproj -> ...Release\haocam.exe`, no LNK1104) before launching.

The QML module
itself is compiled into the binary, but the Qt-provided QML modules
(QtQuick.Layouts, QtQuick.Controls, ...) must be deployed - and windeployqt's
binary scan alone misses QML-only modules. Always pass --qmldir:

    C:/Qt/<ver>/msvc2022_64/bin/windeployqt --qmldir <source>/ui/qml ^
        build/windows/Release/haocam.exe

Without it the app starts and exits with "module QtQuick.Layouts is not
installed" in %LOCALAPPDATA%\HaoCam\logs\haocam.log.

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
| `HAOCAM_ENABLE_FACEBETTER`        | OFF     | Beauty provider (Phase 2, needs sdk/facebetter drop-in) |
| `HAOCAM_ENABLE_MEDIAPIPE`         | OFF     | Face tracker (Phase 2, needs sdk/mediapipe drop-in) |
| `HAOCAM_ENABLE_OPENMAKEUP`        | OFF     | Makeup provider via web runtime (Phase 3) |
| `HAOCAM_ENABLE_SNAP`              | OFF     | AR provider via web runtime (Phase 4)     |
| `HAOCAM_ENABLE_FFMPEG`            | OFF     | Recording backend (Phase 5)               |
| `HAOCAM_ENABLE_VIRTUAL_CAMERA`    | OFF     | Virtual camera output (Phase 6)           |

Optional SDKs must never block the core build: every `HAOCAM_ENABLE_*` flag
maps to an honest provider status in the UI when the SDK is absent.

Drop-in layouts (never committed):

* `sdk/facebetter/` - `include/facebetter/*.h`, `lib/facebetter.lib` +
  `facebetter.dll`, `resource/resource.fbd`. Credentials go to
  `%LOCALAPPDATA%\HaoCam\config.json` (copy `config.example.json`).
* `sdk/mediapipe/` - Tasks C++ headers + prebuilt libs; the
  `face_landmarker.task` model goes to `assets/models/` (see
  `assets/models/README.md`).

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
