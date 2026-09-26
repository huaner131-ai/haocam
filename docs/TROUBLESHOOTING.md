# Troubleshooting (Phase 1)

## Logs

Everything is logged to `%LOCALAPPDATA%\HaoCam\logs\haocam.log` (and the
debugger console). Categories: `app`, `camera`, `gpu`, `effects`, `preview`,
`engine`, `settings`, `filters`, `log`. File level is DEBUG; console level is
INFO (change via `general.logLevel` in settings.json: trace/debug/info/
warning/error/off).

## Preview problems

| Symptom | Cause / fix |
|---|---|
| Black preview, banner "Qt Quick is not using Direct3D 11" | Force the backend: set environment variable `QSG_RHI_BACKEND=d3d11` and restart. HaoCam's Phase-1 pipeline requires D3D11. |
| Black preview, banner "Engine failed to initialize" | Check the log for the compositor error (shader compile / device create). Hybrid-GPU laptops: prefer the discrete GPU (`SHIM_MCCOMPAT` is set automatically; also check Windows Graphics settings). |
| Preview works, camera banner "Camera reconnecting..." | Device unplugged/busy (another app holds it). HaoCam retries with backoff; replug and wait, or switch devices in the Camera tab. |
| "No camera found" | Plug in a webcam and press refresh/reselect in the Camera tab. |
| Low FPS on 4K webcams | Use the 1080p60 mode in the Camera tab; measured FPS shows in the F3 overlay. HaoCam never exceeds what the hardware provides. |
| Preview stutter when dragging the window | Expected: the compositor waits (bounded) for the UI to consume frames. |

## Camera controls

Resolution/fps changes apply on capture restart (the reliable UVC path) -
the active mode is always shown in the Camera tab. Exposure/white
balance/focus/zoom hooks report unsupported in Phase 1.

## Settings file

`%LOCALAPPDATA%\HaoCam\settings.json` - safe to delete while HaoCam is
closed; defaults are recreated. A corrupt file is reported in the log and
replaced.

## Diagnostics overlay

Toggle with `F3` or the title-bar button. Shows: preview FPS, camera FPS,
frame time (CPU), GPU time (timestamp query), CPU time, camera mode, active
pipeline stages, dropped frames, texture pool size. Developer-facing by
design.

## Building

See docs/BUILD.md. The core unit tests build everywhere:
`cmake --preset linux-core && ctest --preset linux-core`.
