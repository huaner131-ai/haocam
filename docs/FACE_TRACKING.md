# Face Tracking

## Phase 1 status

The `IFaceTracker` interface and shared `FaceData` model exist
(`src/face/`). The default tracker is `NullFaceTracker`, which reports
"not detected" and logs honestly:

```
[INFO] [tracking  ] Face tracking unavailable in this build (Phase 2); providers will receive empty FaceData
```

No fake landmarks are produced. The pipeline runs fine without a face: the
color pass ignores `FaceData`, and future providers will degrade to
passthrough when `detected == false`.

## Shared model (spec section 13)

```cpp
struct FaceData {
    bool detected;  float confidence;
    std::vector<Point2D> landmarks;  // normalized [0,1]
    HeadPose headPose;  FaceBounds bounds;
    std::vector<float> blendShapes;
    FaceTransform transform;
};
```

One tracking pass per frame feeds **all** providers (beauty reshape, makeup
layer placement, AR lens anchoring). Providers receive normalized data and
never run their own detector unless the SDK contractually requires it
(e.g. Snap Camera Kit performs its own tracking internally - the bridge
feeds camera frames, and `FaceData` from HaoCam's tracker is used only by
native stages).

## Phase 2 plan

MediaPipe Tasks (FaceLandmarker, GPU delegate) as the first-class
`MediaPipeFaceTracker`, normalized to the canonical 68-point superset
documented in `src/face/FaceLandmarks.h`. Head pose from the transform
matrix; blend shapes from the FaceBlendShapes model (52 ARKit-style).
Confidence-gated handoff to reshape passes.
