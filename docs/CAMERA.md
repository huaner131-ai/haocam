# Camera System (Phase 1)

## Implementation

* **Backend**: Media Foundation Source Reader (`src/camera/MediaFoundationCapture.cpp`).
* **GPU residency**: the reader is configured with `MF_SOURCE_READER_D3D_MANAGER`
  (`IMFDXGIDeviceManager`) wrapping the pipeline D3D11 device, so NV12 samples
  stay in GPU memory. Samples that arrive as CPU buffers (software converter
  paths) are uploaded once in the compositor and then processed on the GPU.
* **Format selection**: `pickBestFormat` prefers `1920x1080 @ 60 FPS`, then
  1440p/720p, rewards NV12 over MJPG, and degrades deterministically
  (unit-tested in `tests/core/test_CameraFormat.cpp`).
* **Controls (Phase 1)**: enumeration, device selection, resolution/fps
  selection (applied on capture restart - the reliable UVC path), mirror
  preview toggle. Exposure/WB/focus/zoom hooks exist on `ICameraSource` and
  return "unsupported" for now (to be wired through `IAMCameraControl` /
  `IAMVideoProcAmp` when the camera control UI lands).

## Device loss & recovery

1. `ReadSample` failure, `MF_SOURCE_READERF_ERROR`, device-removal or an
   activation failure transitions the source to `Reconnecting` and releases
   all COM objects on the camera thread.
2. `CameraManager`'s watchdog retries with exponential backoff
   (1s -> 2s -> 4s -> 8s max). Successful frame flow resets the backoff.
3. If no camera is present the app enters `NoDevice`, keeps running, and the
   UI shows a non-blocking banner. Enumeration is re-run from the Camera tab.
4. HaoCam never crashes when a camera disappears (spec section 22).

## Null camera

`NullCameraSource` produces animated NV12 test-pattern frames (GPU-uploaded
by the compositor) so the pipeline, UI and diagnostics remain demonstrable on
machines without a camera. It is also the development source on non-Windows
builds.

## Supported modes

720p30/60, 1080p24/30/60, 1440p where hardware offers them. The preference
target is `1920x1080 @ 60`; HaoCam never promises a frame rate the hardware
does not deliver (measured FPS is shown in the diagnostics overlay).
