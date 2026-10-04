/**
 * HuanFace Engine Internal — Phase 7 Full Beauty & Face Retouching Engine
 * Defines HFEngine_ and HFBundle_ in global namespace for C ABI compatibility
 * Phase 7: ProductionFaceTracker REAL ML + FullMakeupEngine + FullBeautyEngine with real masks from ML landmarks+mesh, CPU ref + D3D11 GPU real HLSL
 */

#pragma once
#include "../../include/huanface_c_api.h"
#include "../bundle/bundle_reader.h"
#include "../bundle/resource_manager.h"
#include "../rendering/render_backend.h"
#include "../face/face_data.h"
#include "../face/simple_face_tracker.h"
#include "../face/production_face_tracker.h"
#include "../makeup/face_mask.h"
#include "../makeup/makeup_engine.h"
#include "../makeup/makeup_mask.h"
#include "../makeup/makeup_renderer.h"
#include "../beauty/beauty_mask.h"
#include "../beauty/beauty_params.h"
#include "../beauty/beauty_renderer.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>

namespace huanface {

class FaceEngine {
public:
    FaceEngine() {
        // Default to ProductionFaceTracker (real), SimpleFaceTracker as fallback prototype
        productionTracker = std::make_unique<ProductionFaceTracker>();
        simpleTracker = std::make_unique<SimpleFaceTracker>();
        // Use production by default
        tracker = productionTracker.get();
        useProduction = true;
    }

    HFResult Init(const HFEngineConfigC& config) {
        // Choose tracker based on config.faceTrackerType
        std::string type = config.faceTrackerType ? config.faceTrackerType : "production";
        if (type == "simple" || type == "prototype") {
            // Use simple as explicit fallback
            tracker = simpleTracker.get();
            useProduction = false;
            if (!tracker) return HF_RESULT_FAIL;
            return tracker->Init(config);
        } else {
            // Default production
            tracker = productionTracker.get();
            useProduction = true;
            if (!tracker) return HF_RESULT_FAIL;
            HFResult r = tracker->Init(config);
            if (r != HF_RESULT_OK) {
                // Fallback to simple if production fails to init
                tracker = simpleTracker.get();
                useProduction = false;
                return tracker->Init(config);
            }
            return r;
        }
    }

    void Shutdown() {
        if (productionTracker) productionTracker->Shutdown();
        if (simpleTracker) simpleTracker->Shutdown();
    }

    HFResult Process(const HFFrameC* input, HFTrackingData& outTracking) {
        if (!tracker) return HF_RESULT_FAIL;
        return tracker->Process(input, outTracking);
    }

    IFaceTracker* GetTracker() { return tracker; }
    ProductionFaceTracker* GetProductionTracker() { return productionTracker.get(); }
    SimpleFaceTracker* GetSimpleTracker() { return simpleTracker.get(); }
    bool IsUsingProduction() const { return useProduction; }

private:
    std::unique_ptr<ProductionFaceTracker> productionTracker;
    std::unique_ptr<SimpleFaceTracker> simpleTracker;
    IFaceTracker* tracker = nullptr; // points to one of above, not owned
    bool useProduction = true;
};

class MakeupEngine {
public:
    MakeupEngine() : proto(std::make_unique<MakeupEnginePrototype>()), fullEngine(std::make_unique<FullMakeupEngine>()) {}
    HFResult Init() {
        HFResult r1 = proto ? proto->Init() : HF_RESULT_FAIL;
        HFEngineConfigC config{}; config.maxFaces=5;
        HFResult r2 = fullEngine ? fullEngine->Init(config) : HF_RESULT_FAIL;
        return (r1==HF_RESULT_OK && r2==HF_RESULT_OK) ? HF_RESULT_OK : r1;
    }
    HFResult Init(const HFEngineConfigC& config) {
        HFResult r1 = proto ? proto->Init() : HF_RESULT_FAIL;
        HFResult r2 = fullEngine ? fullEngine->Init(config) : HF_RESULT_FAIL;
        return (r1==HF_RESULT_OK && r2==HF_RESULT_OK) ? HF_RESULT_OK : r1;
    }
    void Shutdown() {
        if (proto) proto->Shutdown();
        if (fullEngine) fullEngine->Shutdown();
    }
    MakeupEnginePrototype* GetProto() { return proto.get(); }
    FullMakeupEngine* GetFullEngine() { return fullEngine.get(); }
private:
    std::unique_ptr<MakeupEnginePrototype> proto;
    std::unique_ptr<FullMakeupEngine> fullEngine;
};

class BeautyEngine {
public:
    BeautyEngine() : fullEngine(std::make_unique<FullBeautyEngine>()) {}
    HFResult Init() {
        HFEngineConfigC config{}; config.maxFaces=5;
        return fullEngine ? fullEngine->Init(config) : HF_RESULT_FAIL;
    }
    HFResult Init(const HFEngineConfigC& config) {
        return fullEngine ? fullEngine->Init(config) : HF_RESULT_FAIL;
    }
    void Shutdown() { if (fullEngine) fullEngine->Shutdown(); }
    FullBeautyEngine* GetFullEngine() { return fullEngine.get(); }
private:
    std::unique_ptr<FullBeautyEngine> fullEngine;
};

class BeautyEngineStub {
public:
    HFResult Init() { return HF_RESULT_OK; }
    void Shutdown() {}
    HFResult Process(const HFFrameC* input, HFFrameC* output) {
        if (!input || !output) return HF_RESULT_INVALID_PARAM;
        *output = *input;
        output->ownsData = 0;
        output->ownsGpuTexture = 0;
        return HF_RESULT_OK;
    }
};

} // namespace huanface

// Global opaque structs for C ABI (must be in global namespace to match typedef struct HFEngine_* HFEngine)
struct HFBundle_ {
    std::string path;
    std::unique_ptr<huanface::BundleReader> reader;
    huanface::HFManifest manifest;
    std::unique_ptr<huanface::ResourceManager> resourceManager;
    bool loaded = false;
};

struct HFEngine_ {
    bool initialized = false;
    HFEngineConfigC config;
    std::unique_ptr<huanface::IRenderBackend> renderBackend;
    std::unique_ptr<huanface::ResourceManager> globalResourceManager;
    std::unique_ptr<huanface::FaceEngine> faceEngine;
    std::unique_ptr<huanface::MakeupEngine> makeupEngine;
    std::unique_ptr<huanface::BeautyEngine> beautyEngine;
    std::unique_ptr<huanface::BeautyEngineStub> beautyEngineStub; // legacy
    // Phase 5 tracking data (real production) + Phase 6 makeup + Phase 7 beauty
    std::unique_ptr<huanface::HFTrackingData> trackingData;
    std::unique_ptr<huanface::FaceMaskGenerator> maskGenerator;
    std::unique_ptr<huanface::MakeupMaskGenerator> makeupMaskGenerator;
    std::unique_ptr<huanface::HFMakeupParameters> makeupParams;
    std::unique_ptr<huanface::HFBeautyMaskGenerator> beautyMaskGenerator;
    std::unique_ptr<huanface::HFBeautyParameters> beautyParams;

    std::map<std::string, std::shared_ptr<HFBundle_>> loadedBundles;
    std::map<std::string, float> floatParams;
    std::map<std::string, int> intParams;
    std::map<std::string, int> boolParams;
    std::map<std::string, HFColorC> colorParams;
    std::map<std::string, std::string> enumParams;

    HFTrackingDataC lastTrackingData;
    bool hasLastTracking = false;

    std::mutex mutex;

    HFEngine_() {
        lastTrackingData.faceCount = 0;
        lastTrackingData.faces = nullptr;
        lastTrackingData.timestampNanos = 0;
    }
    ~HFEngine_() {
        if (lastTrackingData.faces) {
            for (int i=0;i<lastTrackingData.faceCount;++i) {
                if (lastTrackingData.faces[i].landmarks) delete[] lastTrackingData.faces[i].landmarks;
                if (lastTrackingData.faces[i].landmarks3D) delete[] lastTrackingData.faces[i].landmarks3D;
            }
            delete[] lastTrackingData.faces;
        }
    }
};

// HFTexture_ stub for Phase 3
struct HFTexture_ {
    int width = 0;
    int height = 0;
    HFFormat format = HF_FORMAT_UNKNOWN;
    std::vector<uint8_t> data;
};
