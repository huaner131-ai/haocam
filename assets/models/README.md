# Face tracking models (Phase 2)

HaoCam NEVER downloads models at runtime (spec section 38). Place the
MediaPipe face landmarker bundle here manually:

    assets/models/face_landmarker.task

Official download (MediaPipe, Apache-2.0 model bundle):

https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task

Notes:

* `.task` files are model bundles and are NOT committed to this repository.
* A custom path can be set in `config.json` -> `tracking.modelPath`.
* If the file is missing, the tracker reports **Unavailable** and the rest
  of the pipeline (preview, beauty engine self-tracking) keeps working.
