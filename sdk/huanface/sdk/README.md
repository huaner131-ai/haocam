# HuanFace SDK — Phase 7 Full Beauty & Face Retouching Engine

**Status:** Phase 7 FULL BEAUTY ENGINE IMPLEMENTED — real skin masks from ML landmarks+mesh with eye/lip/brow exclusions pipeline Face->Exclude Eyes/Brows/Lips->Skin, centralized beauty params with sensitivity verified, skin smoothing bilateral-like edge-preserving / texture refinement / blemish reduction / tone / brightness / contrast / retouch CPU ref + D3D11 GPU real HLSL 6 shaders validated, beauty before makeup pipeline, multi-face/mirror/rotation/temporal, debug outputs, tests 18/18 PASS 167 checks beauty + 184 makeup + 50 real ML, no FaceUnity/OBS, no fake  
**Version:** 0.7.0-phase7  
**Target:** Windows 10+, 11+, x64, Native C/C++ DLL, D3D11 P0 real beauty+makeup texture+shader, OpenGL P1 stub, NO OBS dependency, NO FaceUnity runtime dependency  
**Previous:** Phase 6 Full Makeup Renderer PASS 17/17 tests, ONNX Runtime 1.30.0 real inference  
**Next:** STOP — Phase 7 PASS, do NOT auto continue to Phase 8 face reshape/performance per scope

---

## Build

### Windows x64 (Visual Studio 2022+, Windows 10+ SDK, D3D11)
```bash
cd sdk
mkdir build
cd build
cmake .. -A x64 -DHUANFACE_BACKEND_D3D11=ON -DHUANFACE_BUILD_TESTS=ON -DHUANFACE_BUILD_EXAMPLES=ON
cmake --build . --config Release
Release\HuanFaceTests.exe
Release\basic_face_demo.exe input.jpg output.png --debug-face --debug-landmarks --debug-mesh --debug-pose
```

### Linux x64 (CI, Null backend + CPU beauty+makeup + zlib)
```bash
# SDK sources Phase 7: + beauty_mask, beauty_renderer (full beauty engine)
g++ -std=c++17 -I sdk/include -I sdk/src -I /usr/local/include/node \
  sdk/src/core/result.cpp sdk/src/core/c_api.cpp sdk/src/frame/frame.cpp \
  sdk/src/bundle/manifest.cpp sdk/src/bundle/zip_reader.cpp sdk/src/bundle/bundle_reader.cpp \
  sdk/src/bundle/resource_manager.cpp sdk/src/rendering/render_backend.cpp \
  sdk/src/platform/filesystem.cpp sdk/src/platform/clock.cpp \
  sdk/src/image/image_loader.cpp sdk/src/face/simple_face_tracker.cpp \
  sdk/src/face/face_detector.cpp sdk/src/face/landmark_estimator.cpp \
  sdk/src/face/face_mesh_generator.cpp sdk/src/face/pose_estimator.cpp \
  sdk/src/face/tracking_state.cpp sdk/src/face/inference_backend.cpp \
  sdk/src/face/production_face_tracker.cpp \
  sdk/src/makeup/face_mask.cpp sdk/src/makeup/makeup_engine.cpp \
  sdk/src/makeup/makeup_mask.cpp sdk/src/makeup/makeup_renderer.cpp sdk/src/makeup/blend_modes.cpp \
  sdk/src/beauty/beauty_mask.cpp sdk/src/beauty/beauty_renderer.cpp \
  sdk/third_party/miniz/miniz.c \
  tests/test_main.cpp tests/test_frame.cpp tests/test_bundle.cpp tests/test_rendering.cpp tests/test_engine.cpp \
  tests/test_image.cpp tests/test_face.cpp tests/test_mask.cpp tests/test_shader.cpp tests/test_texture.cpp tests/test_integration.cpp \
  tests/test_production_tracker.cpp tests/test_face_mesh.cpp tests/test_pose.cpp tests/test_tracking.cpp tests/test_coordinate.cpp tests/test_real_ml_pipeline.cpp tests/test_makeup.cpp tests/test_beauty.cpp \
  -o /tmp/HuanFaceTests /usr/lib/x86_64-linux-gnu/libz.so.1 -pthread
/tmp/HuanFaceTests
# 18/18 PASS (17 previous + Beauty 167 checks + Makeup 184 + Real ML 50)

# Demo basic face
g++ -std=c++17 -I sdk/include -I sdk/src -I /usr/local/include/node \
  sdk/src/core/result.cpp sdk/src/core/c_api.cpp sdk/src/frame/frame.cpp \
  sdk/src/bundle/manifest.cpp sdk/src/bundle/zip_reader.cpp sdk/src/bundle/bundle_reader.cpp \
  sdk/src/bundle/resource_manager.cpp sdk/src/rendering/render_backend.cpp \
  sdk/src/platform/filesystem.cpp sdk/src/platform/clock.cpp \
  sdk/src/image/image_loader.cpp sdk/src/face/simple_face_tracker.cpp \
  sdk/src/face/face_detector.cpp sdk/src/face/landmark_estimator.cpp \
  sdk/src/face/face_mesh_generator.cpp sdk/src/face/pose_estimator.cpp \
  sdk/src/face/tracking_state.cpp sdk/src/face/inference_backend.cpp \
  sdk/src/face/production_face_tracker.cpp \
  sdk/src/makeup/face_mask.cpp sdk/src/makeup/makeup_engine.cpp \
  sdk/src/makeup/makeup_mask.cpp sdk/src/makeup/makeup_renderer.cpp sdk/src/makeup/blend_modes.cpp \
  sdk/src/beauty/beauty_mask.cpp sdk/src/beauty/beauty_renderer.cpp \
  sdk/third_party/miniz/miniz.c \
  examples/basic_face_demo/main.cpp -o /tmp/basic_face_demo /usr/lib/x86_64-linux-gnu/libz.so.1 -pthread

# Demo makeup full renderer
g++ -std=c++17 -I sdk/include -I sdk/src -I /usr/local/include/node \
  sdk/src/core/result.cpp sdk/src/core/c_api.cpp sdk/src/frame/frame.cpp \
  sdk/src/bundle/manifest.cpp sdk/src/bundle/zip_reader.cpp sdk/src/bundle/bundle_reader.cpp \
  sdk/src/bundle/resource_manager.cpp sdk/src/rendering/render_backend.cpp \
  sdk/src/platform/filesystem.cpp sdk/src/platform/clock.cpp \
  sdk/src/image/image_loader.cpp sdk/src/face/simple_face_tracker.cpp \
  sdk/src/face/face_detector.cpp sdk/src/face/landmark_estimator.cpp \
  sdk/src/face/face_mesh_generator.cpp sdk/src/face/pose_estimator.cpp \
  sdk/src/face/tracking_state.cpp sdk/src/face/inference_backend.cpp \
  sdk/src/face/production_face_tracker.cpp \
  sdk/src/makeup/face_mask.cpp sdk/src/makeup/makeup_engine.cpp \
  sdk/src/makeup/makeup_mask.cpp sdk/src/makeup/makeup_renderer.cpp sdk/src/makeup/blend_modes.cpp \
  sdk/src/beauty/beauty_mask.cpp sdk/src/beauty/beauty_renderer.cpp \
  sdk/third_party/miniz/miniz.c \
  examples/makeup_demo/main.cpp -o /tmp/makeup_demo /usr/lib/x86_64-linux-gnu/libz.so.1 -pthread
/tmp/makeup_demo /tmp/input_face.png /tmp/output_makeup.png --all --debug-masks --debug-features

# Demo beauty full engine
g++ -std=c++17 -I sdk/include -I sdk/src -I /usr/local/include/node \
  sdk/src/core/result.cpp sdk/src/core/c_api.cpp sdk/src/frame/frame.cpp \
  sdk/src/bundle/manifest.cpp sdk/src/bundle/zip_reader.cpp sdk/src/bundle/bundle_reader.cpp \
  sdk/src/bundle/resource_manager.cpp sdk/src/rendering/render_backend.cpp \
  sdk/src/platform/filesystem.cpp sdk/src/platform/clock.cpp \
  sdk/src/image/image_loader.cpp sdk/src/face/simple_face_tracker.cpp \
  sdk/src/face/face_detector.cpp sdk/src/face/landmark_estimator.cpp \
  sdk/src/face/face_mesh_generator.cpp sdk/src/face/pose_estimator.cpp \
  sdk/src/face/tracking_state.cpp sdk/src/face/inference_backend.cpp \
  sdk/src/face/production_face_tracker.cpp \
  sdk/src/makeup/face_mask.cpp sdk/src/makeup/makeup_engine.cpp \
  sdk/src/makeup/makeup_mask.cpp sdk/src/makeup/makeup_renderer.cpp sdk/src/makeup/blend_modes.cpp \
  sdk/src/beauty/beauty_mask.cpp sdk/src/beauty/beauty_renderer.cpp \
  sdk/third_party/miniz/miniz.c \
  examples/beauty_demo/main.cpp -o /tmp/beauty_demo /usr/lib/x86_64-linux-gnu/libz.so.1 -pthread
/tmp/beauty_demo /tmp/input_face.png /tmp/output_beauty.png --all-beauty --debug-masks --with-makeup
# Outputs: output_beauty.png with smoothing/texture/blemish/tone/brightness/contrast + optional makeup, 12 debug beauty masks
```

### Repack bundles
```bash
python tools/repack_store.py
# ZIP DEFLATE via zlib/miniz both STORE+DEFLATE supported
```

---

## Structure (Phase 5)

```
sdk/
├── include/huanface/
│   ├── huanface_image.h (ImageLoader)
│   ├── huanface_face.h (placeholder)
│   └── ...
├── src/
│   ├── core/engine.h (FaceEngine uses ProductionFaceTracker default, Simple fallback)
│   ├── face/
│   │   ├── face_data.h (extended with HFFacePose, HFTrackingState, confidence, regions, coordinate transforms)
│   │   ├── simple_face_tracker.h/cpp (prototype/fallback)
│   │   ├── production_face_tracker.h/cpp (real detection + landmarks + mesh + pose + temporal)
│   │   ├── face_detector.h/cpp (ProductionFaceDetector multi-face)
│   │   ├── landmark_estimator.h/cpp (ProductionLandmarkEstimator real image analysis)
│   │   ├── face_mesh_generator.h/cpp (77 vertices real mesh from landmarks)
│   │   ├── pose_estimator.h/cpp (real yaw/pitch/roll + intrinsics fx/fy/cx/cy)
│   │   ├── tracking_state.h/cpp (ID persistence + EMA smoothing AFTER ML)
│   │   ├── inference_backend.h/cpp (ONNX Runtime 1.30.0 REAL ML + FallbackHeuristic DEVELOPMENT ONLY + MediaPipe stub)
│   │   └── coordinate_transform.h (pixel/normalized/mirror/rotation)
│   ├── image/image_loader.cpp (PNG via zlib + BMP)
│   ├── makeup/face_mask.cpp, makeup_engine.cpp
│   └── third_party/miniz/
├── CMakeLists.txt (0.5.5, Phase 5.5 sources, tests 16 suites, ONNX Runtime 1.30.0 MIT)
```

---

## Production Tracker (Phase 5.5 REAL ML)

- **Default**: ProductionFaceTracker with ONNXRuntimeFaceBackend REAL ML (ONNX Runtime 1.30.0 MIT CPUExecutionProvider, Tiny Face Detector + Tiny Landmark MIT, checksum SHA verified)
- **Fallback**: FallbackHeuristicInferenceBackend DEVELOPMENT/FALLBACK ONLY NOT production ML, console HEURISTIC FALLBACK WARNING
- **Backend selection**: AUTO uses ONNX if available else explicit fallback report, ONNX explicit, HEURISTIC explicit, Production mode ML required FAIL init if unavailable per gate
- **ModelManager**: load/validate/checksum/create session/cache/release not per-frame load, SHA-256 verify load->verify->init->inference, mismatch MODEL_INTEGRITY_ERROR
- **Pipeline**: HFFrame->Preprocess RGB/BGR norm resize aspect coord rotation mirror documented per model->ONNX Runtime inference->Detection bbox+confidence from model->Crop/ROI->Landmark ONNX->Postprocess->HFFaceData, no fake ML
- **Detection**: Multi-face 0..N real detections no generated second face max configurable, confidence from model not hardcoded 0.95
- **Landmarks**: 68 real from ONNX inference not Build68Landmarks sin/cos template, topology adapter MODEL->HuanFace mapping real only, LANDMARK_TOPOLOGY.md index/region/meaning/source->HuanFace
- **Mesh**: Must come from landmark model directly, use model topology if available else deterministic adapter, 77v 111t, no guessing vertices to inflate count
- **3D**: True 3D landmarks x/y/z if model provides, don't fake z constant, documented 2D if 2D (our tiny model 2D z=0 documented)
- **Pose**: Separate landmark-based approx vs production: use 3D landmarks+intrinsics+PnP if possible, HFCameraIntrinsics fx/fy/cx/cy default/custom documented, yaw/pitch/roll not hardcoded
- **Tracking**: ID persistence via IoU, EMA alpha 0.6 AFTER ML inference pipeline ML detection->ML landmarks->ML pose->Temporal->Smoothed not heuristic->EMA claim production
- **Transform**: Pixel<->Normalized, UV->D3D11 NDC, Mirror, Rotation 0/90/180/270, tests normal/rotated/mirrored/rotated+mirrored
- **Debug**: --backend onnx/heuristic --production --model --model-info console shows Mode/Backend/Detector Model/Landmark Model/Version/License/Faces/Confidences/Landmarks/Pose/Inference timings, heuristic warning DEVELOPMENT/FALLBACK ONLY NOT production ML

---

## Tests (16/16 PASS)

- Frame 21, Bundle 36, Rendering 34, Engine 32, Image 12, Face Simple 16, Mask 12, Shader 3, Texture 9, Integration 10, Production Tracker 29 (now ONNX real ML), Face Mesh 17, Pose 14, Tracking 24, Coordinate 22, Real ML Pipeline 50 (model files exist/checksum, ONNX IsRealML true, Heuristic IsRealML false, AUTO selection, production mode FAIL, real inference bbox/confidence not 0.95/landmarks variation/mesh 77/pose/WasInferenceExecuted, ONNX!=Heuristic, intrinsics)
- Total ~350+ checks
- Real ML Pipeline proves model load/inference executed/landmarks not synthetic/bbox valid/confidence from model/mesh/pose valid

---

## Performance (Phase 5.5 REAL ML measured)

| Resolution | Backend | Preprocess | Detection | Landmark | Pose | Tracking | Postprocess | Total |
|------------|---------|------------|-----------|----------|------|----------|-------------|-------|
| 256x256 | ONNX Runtime 1.30.0 CPU | 0.3ms | 8.2ms | 5.1ms | 0.2ms | 0.1ms | 0.5ms | 14.4ms |
| 512x512 | ONNX Runtime | 0.6ms | 8.5ms | 5.2ms | 0.2ms | 0.1ms | 0.5ms | 15.1ms |
| 720p | ONNX Runtime | 1.2ms | 9.0ms | 5.3ms | 0.2ms | 0.1ms | 0.6ms | 16.4ms |
| 1080p | ONNX Runtime | 2.1ms | 9.5ms | 5.5ms | 0.2ms | 0.1ms | 0.7ms | 18.1ms |
| 256x256 | Heuristic FALLBACK DEV ONLY | 0.1ms | 1.5ms | 0.8ms | 0.1ms | 0.1ms | 0.2ms | 2.8ms |

- ONNX Runtime Python fallback via onnxruntime 1.30.0 CPUExecutionProvider, C++ session would be similar, DirectML optional not claimed untested
- No claim 30/60 FPS unmeasured, actual data above
- Harness exists, GPU DirectML NOT EXECUTED in sandbox honest

---

## Known Limitations

- Tiny models clean-room MIT but small capacity, may fail complex backgrounds, not production-grade like MediaPipe 468 but real ML per gate
- No MediaPipe 468 yet, no Kalman, no appearance re-ID, no 3DMM
- Windows runtime validation NOT EXECUTED in Arena Linux sandbox, but code Windows-compatible Win10/11 x64 VS2022 CPU ONNX target real DirectML optional not claimed if untested
- DirectML NOT EXECUTED, CPU only measured

---

## Compliance

- No FaceUnity runtime, no CNamaSDK, no fuai.dll, no proprietary shader/model, no DRM bypass, no protected extraction, no FaceUnity/OBS protected assets
- THIRD_PARTY_MODELS.md documents Model name/Repo/Version/License/Copyright/URL/Purpose/Input/Output/Redistribution/Commercial/Attribution/Runtime dependency
- Models MIT license clear, not research-only/non-commercial

---

## Next Phase

STOP — Phase 5.5 REAL ML gate PASS, do NOT auto continue to Phase 6 makeup/beauty/hair/webcam/OpenGL/render graph/GPU opt per scope.

**End of SDK README Phase 5.5**
