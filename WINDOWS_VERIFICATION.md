# Windows Runtime Verification

Status: **preview LIVE** (first fully-running app on the user's Windows machine).

All runtime verification below was performed on the **user's machine** (remote
iteration loop: user builds/runs, uploads logs/screenshots to `origin/main`,
fixes land on `arena/01a0de8b-haocam`). Nothing here is claimed from the
development sandbox alone.

## Environment (user machine, 2026-09-28)

* Windows 11 (build 26200), **debloated** — Windows Camera app removed;
  `FrameServer` service had to be re-enabled manually (was policy-disabled)
* GPU: **NVIDIA dGPU** (no iGPU), driver `nvwgf2umx.dll`
* Qt 6.11.2 (msvc2022_64), VS Build Tools 17.14, Windows SDK 10.0.26100
* Cameras: 4 devices — **3 virtual** (Snap Camera, DroidCam, SplitCam) +
  1 physical USB "WebCamera"
* Deploy: `windeployqt --qmldir D:\cam\haocam\ui\qml build\windows\Release\haocam.exe`
  (`--qmldir` is REQUIRED; see docs/BUILD.md)

## Verified working

| Item | Evidence |
|---|---|
| Configure + build + link (MSVC, Release) | user build logs (`origin/main`) |
| Window opens; UI renders; GUI stays responsive | screenshot 205/207, `ui-heartbeat` lines |
| QML module embedded at `:/qt/qml/HaoCam/ui/qml/` | `QML module probe` lines |
| Camera enumeration with physical/virtual classification | `Device N: '...' (virtual|physical)` |
| **Auto-select prefers PHYSICAL camera** | `Auto-selected camera 'WebCamera' (physical)` |
| Explicit device selection works | `Starting camera 'WebCamera' [physical]` |
| Camera opens native mode (1080p30 NV12) | `Camera opened: 1920x1080 @ 30 fps (NV12)` |
| Frames reach the engine | `First camera frame reached the engine queue (1920x1080 gpu=0)` |
| Engine pipeline runs on its own D3D11 device | `Engine owns its own D3D11 device; output shared to Qt via D3D11 shared resources` |
| Cross-device frame handoff | `Importing engine frames via cross-device shared resource` + `First frame CONSUMED by the UI (id=1, 1920x1080)` |
| **Preview displays the camera image** | user confirmation after commit `8a02014` |
| F3 diagnostics overlay | screenshots (fps/frame/gpu/cpu, stages, pool, track/face/beauty rows) |
| No-SDK degradation | Beauty controls disabled, `track Unavailable`, `beauty OFF` — correct by design |
| Core unit tests (Linux sandbox) | 63/63 green at every pushed commit |
| GUI survives camera state churn | watchdog reconnect/backoff exercised during earlier runs |

## Bugs found on real Windows hardware (and their fixes)

The Linux CI (Phase 1) never executes the D3D11 paths; the user's NVIDIA GPU
was the first hardware to run them. Every one of these was invisible until
real Windows runs:

1. **Thread-name exception killed the process** (`0x406D1388`) —
   `RaiseException`-based naming terminates without a debugger → replaced with
   `SetThreadDescription` (`3feaafa`).
2. **Render-thread sync-attach freeze** — the whole engine/camera bootstrap
   ran on the QSG render thread → async attach with stage logs (`a18cde2`).
3. **Media Foundation vs Qt device deadlock** — MF was handed the render
   D3D11 device; `haocam-capture` wedged in `mfplat.dll` while
   `QSGRenderThread` blocked in `d3d11.dll` (0% CPU, "Not Responding");
   stacks captured on the user machine → MF runs device-less, CPU NV12
   samples, one upload per frame (`fffd4f2`).
4. **Engine thread also wedged the shared device** — same failure class as
   (3) once MF was out of the picture → **engine owns its own D3D11 device**;
   frames cross to Qt as `MISC_SHARED` resources (`8648036`).
5. **Virtual-camera auto-pick** — 3 of 4 cameras are virtual; idle virtual
   drivers never deliver frames (E_POINTER loop, "unsustainable" 1440p60
   modes) → devices classified from the symlink (`usb#` vs `root#`),
   physical first (`dcd5181`).
6. **Explicit device-id regression** — the auto-pick refactor dropped the
   else-branch; explicit selections failed with "No device id" → restored
   assignment (`2bfc37e`).
7. **Import failure froze the preview black forever** — the render loop's
   `update()` was skipped after a failed import and the frame id was
   committed too early → retry every sync + per-step failure logs (`231ec0c`).
8. **THE black preview: D3D11 default `CULL_BACK` culled the fullscreen
   triangle.** Probe evidence: NV12 upload held real pixels (Y=108), pass-1
   output had alpha 0 (nothing written), final had alpha 255 (clear works,
   culling kills the draw); the PS writes alpha=1 unconditionally → zero
   coverage → the triangle never rasterized. This code had *never* run on any
   GPU before. → explicit `CULL_NONE` rasterizer state (`8a02014`).
9. **Mirror did nothing: double flip** — pass 2 never bound its own VS
   constant buffer, inherited pass 1's (mirrorX=1) and mirrored a second time,
   cancelling the mirror → pass 2 binds its own constants and copies straight.
10. **Resolution re-select flicker** — re-clicking the active mode restarted
    the capture for nothing → skip restart when the requested mode already
    streams.

## Diagnostics added (kept intentionally)

* `HaoCam 0.1.0 starting (build <sha>)` — build stamp refreshed every build
* `ui-heartbeat: main thread alive` every 5 s
* `VideoView sync #N (compositor=… latestFrame=…)` on change (render health)
* `First camera frame …` / `First frame CONSUMED by the UI …`
* `Output probe` / `Processed probe` / `NV12 probe` — one-shot engine-side
  staging readbacks (frame 1 + every 300th frame) — to be quieted after the
  current verification round
* Per-step import failure logs (`Import step FAILED: …`, once per kind)

## Pending checklist (not yet verified on the user machine)

* [x] Camera auto-select fixes + resolution/mirror polish pushed (310b313)
* [x] HuanFace SDK (user's own) integrated behind HAOCAM_ENABLE_HUANFACE:
      provider engine lifecycle verified in CI (64/64 with SDK drop,
      63/63 without); needs a user-side run with the SDK drop + wiring patch
      (patches/huanface/0001) for the visual check
* [ ] Mirror toggle visibly flips the preview (fix pushed, needs a run)
* [ ] "Auto" resolution = device native best; resolution switch without
      flicker on re-select (fix pushed, needs a run)
* [ ] Camera unplug → watchdog reconnect, no crash
* [ ] Wrong Facebetter credentials → "Invalid credentials" message, no crash
      (requires the Facebetter SDK build, see docs/SDK_INTEGRATION.md)
* [ ] Facebetter SDK build with real credentials (`HAOCAM_ENABLE_FACEBETTER=ON`)
* [ ] MediaPipe tracker build (`HAOCAM_ENABLE_MEDIAPIPE=ON`) — optional
* [ ] Performance measurement at native mode (GPU ms, engine fps) — current
      observation: `gpu 1.1 ms`, camera 15 fps in low light (sensor, not app)

## Known environment caveats

* A crashed/wedged haocam.exe can lock `haocam.exe` against relink
  (LNK1104) — `taskkill /IM haocam.exe /F` first; if the process is stuck in
  a kernel-mode wait, only a reboot clears it (see docs/BUILD.md).
* The debloat disabled `FrameServer` — Media Foundation falls back to direct
  driver access, but re-enabling it is recommended
  (`Set-Service FrameServer -StartupType Manual; Start-Service FrameServer`
  from an ADMIN PowerShell).
* Virtual cameras (DroidCam/SplitCam/Snap) enumerate but deliver no frames
  until their sources are active; HaoCam now avoids them by default.
