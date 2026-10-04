/**
 * HuanFace C ABI — Public API (Phase 2 Specification, no implementation)
 * Target: Windows 10+, x64, Native C/C++ DLL, NO OBS dependency
 * OBS is EXTERNAL REFERENCE only
 *
 * This header defines proposed C ABI, not implemented in Phase 2.
 * Implementation starts Phase 4+.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Opaque handles
typedef struct HFEngine_* HFEngine;
typedef struct HFBundle_* HFBundle;
typedef struct HFTexture_* HFTexture;

// Result codes
typedef enum HFResult {
    HF_RESULT_OK = 0,
    HF_RESULT_FAIL = 1,
    HF_RESULT_NOT_INITIALIZED = 2,
    HF_RESULT_INVALID_PARAM = 3,
    HF_RESULT_NOT_SUPPORTED = 4,
    HF_RESULT_OUT_OF_MEMORY = 5,
    HF_RESULT_FILE_NOT_FOUND = 6,
    HF_RESULT_BUNDLE_INVALID = 7,
    HF_RESULT_FACE_NOT_DETECTED = 8,
    HF_RESULT_MODEL_LOAD_FAILED = 9,
    HF_RESULT_MODEL_INTEGRITY_ERROR = 10,
    HF_RESULT_MODEL_NOT_FOUND = 11,
} HFResult;

// Inference backend type
typedef enum HFInferenceBackendType {
    HF_BACKEND_AUTO = 0,
    HF_BACKEND_ONNX = 1,
    HF_BACKEND_HEURISTIC = 2,
    HF_BACKEND_MEDIAPIPE = 3,
} HFInferenceBackendType;

// Camera intrinsics
typedef struct HFCameraIntrinsicsC {
    float fx;
    float fy;
    float cx;
    float cy;
} HFCameraIntrinsicsC;

// Render backend type
typedef enum HFRenderBackendType {
    HF_RENDER_BACKEND_AUTO = 0,
    HF_RENDER_BACKEND_D3D11 = 1,
    HF_RENDER_BACKEND_OPENGL = 2,
} HFRenderBackendType;

// Frame format
typedef enum HFFormat {
    HF_FORMAT_UNKNOWN = 0,
    HF_FORMAT_RGBA8 = 1,
    HF_FORMAT_BGRA8 = 2,
    HF_FORMAT_RGB8 = 3,
    HF_FORMAT_BGR8 = 4,
    HF_FORMAT_NV12 = 5,
    HF_FORMAT_YUV420P = 6,
    HF_FORMAT_R8 = 7,
    HF_FORMAT_R32F = 8,
} HFFormat;

// Param type
typedef enum HFParamType {
    HF_PARAM_TYPE_FLOAT = 0,
    HF_PARAM_TYPE_INT = 1,
    HF_PARAM_TYPE_BOOL = 2,
    HF_PARAM_TYPE_COLOR = 3,
    HF_PARAM_TYPE_VEC2 = 4,
    HF_PARAM_TYPE_VEC3 = 5,
    HF_PARAM_TYPE_VEC4 = 6,
    HF_PARAM_TYPE_TEXTURE = 7,
    HF_PARAM_TYPE_ENUM = 8,
} HFParamType;

// Frame (C version)
typedef struct HFFrameC {
    int width;
    int height;
    HFFormat format;
    int64_t timestampNanos;
    uint8_t* data;
    uint8_t* dataU;
    uint8_t* dataV;
    int stride;
    int strideU;
    int strideV;
    void* gpuTexture; // ID3D11Texture2D* or GLuint as void*
    void* nativeHandle;
    int ownsData;
    int ownsGpuTexture;
    int rotation; // 0,90,180,270
    int isMirrored;
} HFFrameC;

// Face data (C version simplified)
typedef struct HFFaceDataC {
    int id;
    float bboxX, bboxY, bboxW, bboxH;
    float confidence;
    int landmarkCount;
    float* landmarks; // [x0,y0,x1,y1,...] count*2
    float* landmarks3D; // [x0,y0,z0,...] count*3
    float rotationPitch, rotationYaw, rotationRoll;
    float translationX, translationY, translationZ;
} HFFaceDataC;

typedef struct HFTrackingDataC {
    int faceCount;
    HFFaceDataC* faces;
    int64_t timestampNanos;
} HFTrackingDataC;

// Engine config
typedef struct HFEngineConfigC {
    HFRenderBackendType backendType;
    void* windowHandle; // HWND
    int width;
    int height;
    int enableDebug;
    const char* faceTrackerType; // "mediapipe", "onnx", "dlib"
    int maxFaces;
    int detectSmallFace;
    float minFaceRatio;
    int faceLandmarkQuality;
    int faceDetectMode;
    int useAsyncAIInference;
    int enableFaceMeshV2;
} HFEngineConfigC;

// Color
typedef struct HFColorC {
    float r, g, b, a;
} HFColorC;

// Lifecycle
HFResult HF_Init();
HFResult HF_Shutdown();
HFResult HF_CreateEngine(const HFEngineConfigC* config, HFEngine* outEngine);
HFResult HF_DestroyEngine(HFEngine engine);

// Bundle
HFResult HF_LoadBundle(HFEngine engine, const char* bundlePath, HFBundle* outBundle);
HFResult HF_LoadBundleFromMemory(HFEngine engine, const uint8_t* data, int dataSize, HFBundle* outBundle);
HFResult HF_UnloadBundle(HFEngine engine, HFBundle bundle);

// Parameters generic
HFResult HF_SetParameterFloat(HFEngine engine, const char* name, float value);
HFResult HF_SetParameterInt(HFEngine engine, const char* name, int value);
HFResult HF_SetParameterBool(HFEngine engine, const char* name, int value);
HFResult HF_SetParameterColor(HFEngine engine, const char* name, HFColorC color);
HFResult HF_SetParameterVec2(HFEngine engine, const char* name, float x, float y);
HFResult HF_SetParameterVec3(HFEngine engine, const char* name, float x, float y, float z);
HFResult HF_SetParameterVec4(HFEngine engine, const char* name, float x, float y, float z, float w);
HFResult HF_SetParameterTexture(HFEngine engine, const char* name, HFTexture texture);
HFResult HF_SetParameterEnum(HFEngine engine, const char* name, const char* enumValue);

HFResult HF_GetParameterFloat(HFEngine engine, const char* name, float* outValue);
HFResult HF_GetParameterInt(HFEngine engine, const char* name, int* outValue);
HFResult HF_GetParameterBool(HFEngine engine, const char* name, int* outValue);
HFResult HF_GetParameterColor(HFEngine engine, const char* name, HFColorC* outColor);

// Process
HFResult HF_ProcessFrame(HFEngine engine, const HFFrameC* inputFrame, HFFrameC* outFrame);
HFResult HF_ProcessFrameWithBundle(HFEngine engine, const HFFrameC* inputFrame, HFBundle bundle, HFFrameC* outFrame);

// Face data
HFResult HF_GetFaceData(HFEngine engine, HFTrackingDataC* outTrackingData);
HFResult HF_FreeFaceData(HFTrackingDataC* trackingData);
HFResult HF_FreeFrame(HFFrameC* frame);

// Inference backend selection
HFResult HF_SetInferenceBackend(HFEngine engine, HFInferenceBackendType backendType);
HFResult HF_GetInferenceBackend(HFEngine engine, HFInferenceBackendType* outBackendType);
HFResult HF_LoadFaceModel(HFEngine engine, const char* modelPath, const char* expectedSha256);
HFResult HF_GetModelInfo(HFEngine engine, const char* modelPath, char* outInfo, int infoSize);

// Utility
const char* HF_GetVersion();
const char* HF_GetResultString(HFResult result);

#ifdef __cplusplus
}
#endif
