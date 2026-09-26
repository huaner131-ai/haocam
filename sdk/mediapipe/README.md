# MediaPipe drop-in (Phase 2)

Optional. HAOCAM_ENABLE_MEDIAPIPE=ON expects prebuilt MediaPipe Tasks
libraries + headers here:

    sdk/mediapipe/
    ├── include/mediapipe/tasks/cc/vision/face_landmarker/face_landmarker.h
    │   ... (full tasks CC headers)
    └── lib/*.lib

Model bundle goes to assets/models/face_landmarker.task
(see assets/models/README.md - never downloaded at runtime).

Without the drop-in, HaoCam builds fine and Tracking reports Unavailable.
