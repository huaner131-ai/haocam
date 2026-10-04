/**
 * HuanFace C++ RAII wrapper — header-only (Phase 2 Specification, no implementation)
 * Wraps huanface_c_api.h C ABI
 * Target: Windows 10+, x64, NO OBS dependency
 */

#pragma once
#include "huanface_c_api.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace huanface {

class Exception : public std::runtime_error {
public:
    HFResult result;
    Exception(HFResult r, const std::string& msg) : std::runtime_error(msg), result(r) {}
};

inline void CheckResult(HFResult r) {
    if (r != HF_RESULT_OK) {
        throw Exception(r, HF_GetResultString(r));
    }
}

struct Frame {
    int width = 0;
    int height = 0;
    HFFormat format = HF_FORMAT_UNKNOWN;
    int64_t timestampNanos = 0;
    std::vector<uint8_t> data;
    std::vector<uint8_t> dataU;
    std::vector<uint8_t> dataV;
    int stride = 0;
    int strideU = 0;
    int strideV = 0;
    void* gpuTexture = nullptr;
    void* nativeHandle = nullptr;
    int rotation = 0;
    bool isMirrored = false;
    
    HFFrameC ToC() const {
        HFFrameC c{};
        c.width = width;
        c.height = height;
        c.format = format;
        c.timestampNanos = timestampNanos;
        c.data = const_cast<uint8_t*>(data.data());
        c.dataU = const_cast<uint8_t*>(dataU.data());
        c.dataV = const_cast<uint8_t*>(dataV.data());
        c.stride = stride;
        c.strideU = strideU;
        c.strideV = strideV;
        c.gpuTexture = gpuTexture;
        c.nativeHandle = nativeHandle;
        c.ownsData = 0;
        c.ownsGpuTexture = 0;
        c.rotation = rotation;
        c.isMirrored = isMirrored ? 1 : 0;
        return c;
    }
};

struct FaceData {
    int id = 0;
    struct Rect { float x,y,w,h; } bbox;
    float confidence = 0.0f;
    std::vector<std::pair<float,float>> landmarks;
};

struct TrackingData {
    std::vector<FaceData> faces;
    int64_t timestampNanos = 0;
};

struct EngineConfig {
    HFRenderBackendType backendType = HF_RENDER_BACKEND_AUTO;
    void* windowHandle = nullptr;
    int width = 1280;
    int height = 720;
    bool enableDebug = false;
    std::string faceTrackerType = "mediapipe";
    int maxFaces = 4;
    
    HFEngineConfigC ToC() const {
        HFEngineConfigC c{};
        c.backendType = backendType;
        c.windowHandle = windowHandle;
        c.width = width;
        c.height = height;
        c.enableDebug = enableDebug ? 1 : 0;
        c.faceTrackerType = faceTrackerType.c_str();
        c.maxFaces = maxFaces;
        return c;
    }
};

class Bundle {
public:
    Bundle() : handle(nullptr) {}
    ~Bundle() { /* HF_UnloadBundle requires engine, store engine in real impl */ }
    HFBundle handle;
};

class Engine {
public:
    Engine(const EngineConfig& config) : engine(nullptr) {
        HFEngineConfigC c = config.ToC();
        HFResult r = HF_CreateEngine(&c, &engine);
        CheckResult(r);
    }
    ~Engine() {
        if (engine) HF_DestroyEngine(engine);
    }
    
    Bundle LoadBundle(const std::string& path) {
        Bundle bundle;
        HFResult r = HF_LoadBundle(engine, path.c_str(), &bundle.handle);
        CheckResult(r);
        return bundle;
    }
    
    void SetParameter(const std::string& name, float value) {
        CheckResult(HF_SetParameterFloat(engine, name.c_str(), value));
    }
    void SetParameter(const std::string& name, const HFColorC& color) {
        CheckResult(HF_SetParameterColor(engine, name.c_str(), color));
    }
    
    Frame ProcessFrame(const Frame& input) {
        HFFrameC inC = input.ToC();
        HFFrameC outC{};
        CheckResult(HF_ProcessFrame(engine, &inC, &outC));
        Frame out;
        out.width = outC.width;
        out.height = outC.height;
        // In real impl copy data
        HF_FreeFrame(&outC);
        return out;
    }
    
    TrackingData GetFaceData() {
        HFTrackingDataC c{};
        CheckResult(HF_GetFaceData(engine, &c));
        TrackingData data;
        for (int i=0;i<c.faceCount;++i) {
            FaceData fd;
            fd.id = c.faces[i].id;
            fd.bbox = {c.faces[i].bboxX, c.faces[i].bboxY, c.faces[i].bboxW, c.faces[i].bboxH};
            fd.confidence = c.faces[i].confidence;
            data.faces.push_back(std::move(fd));
        }
        data.timestampNanos = c.timestampNanos;
        HF_FreeFaceData(&c);
        return data;
    }
    
private:
    HFEngine engine;
};

inline void Init() { CheckResult(HF_Init()); }
inline void Shutdown() { CheckResult(HF_Shutdown()); }
inline std::string GetVersion() { return HF_GetVersion(); }

} // namespace huanface
