# Virtual Camera - Phase 6

Phase 1 ships no virtual camera. Design contract for Phase 6:

* `VirtualCamera`/`VirtualCameraManager` receive the **final HaoCam frame**
  (spec section 24) - the same GPU texture the preview shows, not the raw
  webcam frame.
* Windows architecture: a user-mode virtual camera driver stack
  (Media Foundation virtual camera on Windows 11 22H2+, or the
  well-established filter-driver approach for Windows 10 compatibility).
  HaoCam will document which approach ships and its installation story;
  no kernel code is written in-tree without explicit opt-in.
* Compatibility targets: OBS, Discord, Zoom, Google Meet, browsers - any
  app enumerating standard DirectShow/MF camera devices.
* `HAOCAM_ENABLE_VIRTUAL_CAMERA=ON` gates the output; the Settings panel
  reports "Virtual camera unavailable" when the driver stack is not
  installed, and every other feature keeps working.

## Frame flow

```
Compositor final ring -> VirtualCamera::pushFrame(Frame) -> driver queue
```

Pushes are best-effort and drop-oldest: a slow consumer must never stall
the camera or preview threads.
