# Recording (FFmpeg) - Phase 5

Phase 1 ships no recording. This document fixes the design so the Phase-5
implementation cannot drift from the architecture:

* Recording consumes the **final processed texture** from the compositor
  output ring - never a second effect pipeline (spec sections 23/18).
* GPU->encoder: map the final texture through
  `ID3D11VideoProcessor`/NV12 copy into an FFmpeg D3D11VA input frame
  (H.264/H.265 via `libx264`/`libx265` or hardware encoders where
  available). MP4 container.
* Start/stop/pause/resume controls live in `RecordingPanel`; settings
  (codec, bitrate, container, output folder) in `RecordingConfig`.
* Audio capture (WASAPI loopback or mic) is added only if the threading
  budget allows it without preview stalls; otherwise it is explicitly
  listed as unavailable.
* `HAOCAM_ENABLE_FFMPEG=ON` requires FFmpeg dev libraries; without it the
  Record tab reports "Recording unavailable (Phase 5 backend not compiled)".

## Status reporting

Recorder state changes flow through the EventBus (`recording` category) so
the UI, logs and future automation share one truth source. Failures (disk
full, encoder init) disable recording for the session and are logged - the
preview keeps running.
