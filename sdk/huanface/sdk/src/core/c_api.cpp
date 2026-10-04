/**
 * HuanFace C API Implementation — Phase 3
 * Implements functions from huanface_c_api.h
 * No OBS dependency, no FaceUnity runtime dependency
 */

#include "../../include/huanface_c_api.h"
#include "engine.h"
#include "result.h"
#include "../frame/frame.h"
#include "../bundle/bundle_reader.h"
#include "../bundle/manifest.h"
#include "../bundle/resource_manager.h"
#include "../rendering/render_backend.h"
#include <mutex>
#include <atomic>
#include <cstring>

namespace {
    std::atomic<bool> g_initialized(false);
    std::mutex g_initMutex;
}

extern "C" {

HFResult HF_Init() {
    std::lock_guard<std::mutex> lock(g_initMutex);
    if (g_initialized.load()) {
        return HF_RESULT_OK; // already initialized
    }
    // Global init: could init COM for Media Foundation on Windows, etc.
    // For Phase 3 minimal, just mark initialized
    g_initialized.store(true);
    return HF_RESULT_OK;
}

HFResult HF_Shutdown() {
    std::lock_guard<std::mutex> lock(g_initMutex);
    if (!g_initialized.load()) {
        return HF_RESULT_NOT_INITIALIZED;
    }
    g_initialized.store(false);
    return HF_RESULT_OK;
}

HFResult HF_CreateEngine(const HFEngineConfigC* config, HFEngine* outEngine) {
    if (!g_initialized.load()) {
        return HF_RESULT_NOT_INITIALIZED;
    }
    if (!outEngine) {
        return HF_RESULT_INVALID_PARAM;
    }

    try {
        auto engine = new HFEngine_();

        // Copy config or use default
        if (config) {
            engine->config = *config;
        } else {
            // Default config
            memset(&engine->config, 0, sizeof(engine->config));
            engine->config.backendType = HF_RENDER_BACKEND_AUTO;
            engine->config.width = 1280;
            engine->config.height = 720;
            engine->config.maxFaces = 4;
            engine->config.faceTrackerType = "mediapipe";
        }

        // Init render backend
        engine->renderBackend = huanface::CreateRenderBackend(engine->config.backendType);
        if (!engine->renderBackend) {
            delete engine;
            return HF_RESULT_FAIL;
        }
        HFResult res = engine->renderBackend->Init(engine->config.windowHandle);
        if (res != HF_RESULT_OK) {
            // For Phase 3, allow fallback to null backend even if D3D11 fails
            // Try null backend
            engine->renderBackend = huanface::CreateNullBackend();
            if (!engine->renderBackend) {
                delete engine;
                return HF_RESULT_FAIL;
            }
            res = engine->renderBackend->Init(nullptr);
            if (res != HF_RESULT_OK) {
                delete engine;
                return HF_RESULT_FAIL;
            }
        }

        // Init resource manager
        engine->globalResourceManager = std::make_unique<huanface::ResourceManager>();

        // Init real engines Phase 4-7
        engine->faceEngine = std::make_unique<huanface::FaceEngine>();
        engine->faceEngine->Init(engine->config);

        engine->makeupEngine = std::make_unique<huanface::MakeupEngine>();
        engine->makeupEngine->Init(engine->config);

        engine->beautyEngine = std::make_unique<huanface::BeautyEngine>();
        engine->beautyEngine->Init(engine->config);

        engine->beautyEngineStub = std::make_unique<huanface::BeautyEngineStub>();
        engine->beautyEngineStub->Init();

        engine->trackingData = std::make_unique<huanface::HFTrackingData>();
        engine->maskGenerator = std::make_unique<huanface::FaceMaskGenerator>();
        engine->makeupMaskGenerator = std::make_unique<huanface::MakeupMaskGenerator>();
        engine->beautyMaskGenerator = std::make_unique<huanface::HFBeautyMaskGenerator>();
        engine->makeupParams = std::make_unique<huanface::HFMakeupParameters>();
        engine->beautyParams = std::make_unique<huanface::HFBeautyParameters>();

        engine->initialized = true;
        *outEngine = engine;
        return HF_RESULT_OK;
    } catch (const std::bad_alloc&) {
        return HF_RESULT_OUT_OF_MEMORY;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_DestroyEngine(HFEngine engine) {
    if (!engine) {
        return HF_RESULT_INVALID_PARAM;
    }
    try {
        // Shutdown engines
        if (engine->faceEngine) engine->faceEngine->Shutdown();
        if (engine->makeupEngine) engine->makeupEngine->Shutdown();
        if (engine->beautyEngine) engine->beautyEngine->Shutdown();
        if (engine->beautyEngineStub) engine->beautyEngineStub->Shutdown();
        if (engine->renderBackend) engine->renderBackend->Shutdown();

        // Clear bundles
        engine->loadedBundles.clear();

        delete engine;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_LoadBundle(HFEngine engine, const char* bundlePath, HFBundle* outBundle) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !bundlePath || !outBundle) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;

    try {
        std::lock_guard<std::mutex> lock(engine->mutex);

        // Check if already loaded
        auto it = engine->loadedBundles.find(bundlePath);
        if (it != engine->loadedBundles.end()) {
            *outBundle = it->second.get();
            return HF_RESULT_OK;
        }

        auto bundle = std::make_shared<HFBundle_>();
        bundle->path = bundlePath;
        bundle->reader = std::make_unique<huanface::BundleReader>();

        std::string error;
        if (!bundle->reader->Open(bundlePath, error)) {
            return HF_RESULT_FILE_NOT_FOUND;
        }

        bundle->manifest = bundle->reader->GetManifest();
        bundle->resourceManager = std::make_unique<huanface::ResourceManager>();

        // Load all resources from manifest
        if (!bundle->resourceManager->LoadAllFromManifest(*bundle->reader, bundle->manifest, error)) {
            if (error.find("not found") != std::string::npos) {
                return HF_RESULT_FILE_NOT_FOUND;
            }
            return HF_RESULT_BUNDLE_INVALID;
        }

        bundle->loaded = true;

        // Store in engine
        HFBundle rawPtr = bundle.get();
        engine->loadedBundles[bundlePath] = bundle;
        *outBundle = rawPtr;
        return HF_RESULT_OK;
    } catch (const std::bad_alloc&) {
        return HF_RESULT_OUT_OF_MEMORY;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_LoadBundleFromMemory(HFEngine engine, const uint8_t* data, int dataSize, HFBundle* outBundle) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !data || dataSize <=0 || !outBundle) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;

    try {
        std::lock_guard<std::mutex> lock(engine->mutex);

        auto bundle = std::make_shared<HFBundle_>();
        bundle->path = "<memory>";
        bundle->reader = std::make_unique<huanface::BundleReader>();

        std::string error;
        if (!bundle->reader->OpenFromMemory(data, (size_t)dataSize, error)) {
            return HF_RESULT_BUNDLE_INVALID;
        }

        bundle->manifest = bundle->reader->GetManifest();
        bundle->resourceManager = std::make_unique<huanface::ResourceManager>();

        if (!bundle->resourceManager->LoadAllFromManifest(*bundle->reader, bundle->manifest, error)) {
            return HF_RESULT_BUNDLE_INVALID;
        }

        bundle->loaded = true;
        HFBundle rawPtr = bundle.get();
        std::string key = "<memory>_" + std::to_string((uintptr_t)rawPtr);
        engine->loadedBundles[key] = bundle;
        *outBundle = rawPtr;
        return HF_RESULT_OK;
    } catch (const std::bad_alloc&) {
        return HF_RESULT_OUT_OF_MEMORY;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_UnloadBundle(HFEngine engine, HFBundle bundle) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !bundle) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;

    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        // Find and erase bundle that matches raw pointer
        for (auto it = engine->loadedBundles.begin(); it != engine->loadedBundles.end(); ++it) {
            if (it->second.get() == bundle) {
                engine->loadedBundles.erase(it);
                return HF_RESULT_OK;
            }
        }
        // If not found in map, maybe it was already unloaded, still OK? Return NOT_FOUND
        return HF_RESULT_FILE_NOT_FOUND;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_SetParameterFloat(HFEngine engine, const char* name, float value) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !name) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;
    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        engine->floatParams[name] = value;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_SetParameterInt(HFEngine engine, const char* name, int value) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !name) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;
    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        engine->intParams[name] = value;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_SetParameterBool(HFEngine engine, const char* name, int value) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !name) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;
    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        engine->boolParams[name] = value ? 1 : 0;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_SetParameterColor(HFEngine engine, const char* name, HFColorC color) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !name) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;
    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        engine->colorParams[name] = color;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_SetParameterVec2(HFEngine engine, const char* name, float x, float y) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !name) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;
    // For Phase 3 minimal, store as float params with suffix
    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        engine->floatParams[std::string(name) + ".x"] = x;
        engine->floatParams[std::string(name) + ".y"] = y;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_SetParameterVec3(HFEngine engine, const char* name, float x, float y, float z) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !name) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;
    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        engine->floatParams[std::string(name) + ".x"] = x;
        engine->floatParams[std::string(name) + ".y"] = y;
        engine->floatParams[std::string(name) + ".z"] = z;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_SetParameterVec4(HFEngine engine, const char* name, float x, float y, float z, float w) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !name) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;
    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        engine->floatParams[std::string(name) + ".x"] = x;
        engine->floatParams[std::string(name) + ".y"] = y;
        engine->floatParams[std::string(name) + ".z"] = z;
        engine->floatParams[std::string(name) + ".w"] = w;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_SetParameterTexture(HFEngine engine, const char* name, HFTexture texture) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !name) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;
    // For Phase 3 minimal, not implemented full texture param, return NOT_SUPPORTED
    (void)texture;
    return HF_RESULT_NOT_SUPPORTED;
}

HFResult HF_SetParameterEnum(HFEngine engine, const char* name, const char* enumValue) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !name || !enumValue) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;
    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        engine->enumParams[name] = enumValue;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_GetParameterFloat(HFEngine engine, const char* name, float* outValue) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !name || !outValue) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;
    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        auto it = engine->floatParams.find(name);
        if (it == engine->floatParams.end()) return HF_RESULT_FILE_NOT_FOUND;
        *outValue = it->second;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_GetParameterInt(HFEngine engine, const char* name, int* outValue) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !name || !outValue) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;
    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        auto it = engine->intParams.find(name);
        if (it == engine->intParams.end()) return HF_RESULT_FILE_NOT_FOUND;
        *outValue = it->second;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_GetParameterBool(HFEngine engine, const char* name, int* outValue) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !name || !outValue) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;
    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        auto it = engine->boolParams.find(name);
        if (it == engine->boolParams.end()) return HF_RESULT_FILE_NOT_FOUND;
        *outValue = it->second;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_GetParameterColor(HFEngine engine, const char* name, HFColorC* outColor) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !name || !outColor) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;
    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        auto it = engine->colorParams.find(name);
        if (it == engine->colorParams.end()) return HF_RESULT_FILE_NOT_FOUND;
        *outColor = it->second;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_ProcessFrame(HFEngine engine, const HFFrameC* inputFrame, HFFrameC* outFrame) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !inputFrame || !outFrame) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;

    std::string validationError;
    HFResult res = huanface::FrameValidator::Validate(inputFrame, &validationError);
    if (res != HF_RESULT_OK) return res;

    try {
        std::lock_guard<std::mutex> lock(engine->mutex);

        // Phase 4 pipeline: Validate -> FaceTracker -> FaceMaskGenerator -> Beauty pass-through -> Makeup -> RenderBackend -> Output
        // 1. Face tracking real
        huanface::HFTrackingData tracking;
        if (engine->faceEngine) {
            HFResult faceRes = engine->faceEngine->Process(inputFrame, tracking);
            if (faceRes != HF_RESULT_OK) {
                // If face tracker fails, continue with no faces (not fatal)
                tracking.Clear();
            }
        }
        // Store for GetFaceData (convert to C ABI)
        // Free previous C tracking
        if (engine->lastTrackingData.faces) {
            for (int i=0;i<engine->lastTrackingData.faceCount;++i) {
                if (engine->lastTrackingData.faces[i].landmarks) delete[] engine->lastTrackingData.faces[i].landmarks;
                if (engine->lastTrackingData.faces[i].landmarks3D) delete[] engine->lastTrackingData.faces[i].landmarks3D;
            }
            delete[] engine->lastTrackingData.faces;
            engine->lastTrackingData.faces = nullptr;
            engine->lastTrackingData.faceCount = 0;
        }
        // Convert tracking to C
        engine->lastTrackingData.faceCount = (int)tracking.faces.size();
        engine->lastTrackingData.timestampNanos = inputFrame->timestampNanos;
        if (!tracking.faces.empty()) {
            engine->lastTrackingData.faces = new HFFaceDataC[tracking.faces.size()];
            for (size_t fi=0; fi<tracking.faces.size(); ++fi) {
                const auto& src = tracking.faces[fi];
                auto& dst = engine->lastTrackingData.faces[fi];
                dst.id = src.id;
                dst.bboxX = src.bboxX;
                dst.bboxY = src.bboxY;
                dst.bboxW = src.bboxW;
                dst.bboxH = src.bboxH;
                dst.confidence = src.confidence;
                dst.landmarkCount = (int)src.landmarks.size();
                if (!src.landmarks.empty()) {
                    dst.landmarks = new float[src.landmarks.size()*2];
                    for (size_t li=0; li<src.landmarks.size(); ++li) {
                        dst.landmarks[li*2+0]=src.landmarks[li].x;
                        dst.landmarks[li*2+1]=src.landmarks[li].y;
                    }
                } else dst.landmarks=nullptr;
                if (!src.landmarks3D.empty()) {
                    dst.landmarks3D = new float[src.landmarks3D.size()*3];
                    for (size_t li=0; li<src.landmarks3D.size(); ++li) {
                        dst.landmarks3D[li*3+0]=src.landmarks3D[li].x;
                        dst.landmarks3D[li*3+1]=src.landmarks3D[li].y;
                        dst.landmarks3D[li*3+2]=src.landmarks3D[li].z;
                    }
                } else dst.landmarks3D=nullptr;
                dst.rotationPitch=src.rotationPitch;
                dst.rotationYaw=src.rotationYaw;
                dst.rotationRoll=src.rotationRoll;
                dst.translationX=src.translationX;
                dst.translationY=src.translationY;
                dst.translationZ=src.translationZ;
            }
        } else {
            engine->lastTrackingData.faces = nullptr;
        }
        engine->hasLastTracking = true;
        // Also store internal tracking
        if (engine->trackingData) *engine->trackingData = tracking;

        // 2. If no face, output = input (pass-through)
        if (tracking.faces.empty()) {
            // Beauty pass-through
            HFFrameC beautified = *inputFrame;
            beautified.ownsData = 0;
            beautified.ownsGpuTexture = 0;
            // Output: need to allocate new data if input has CPU data? For ownership, we will allocate new buffer and copy
            // For minimal, we copy input to output with owned data
            if (inputFrame->data) {
                int bpp = huanface::FrameValidator::GetBytesPerPixel(inputFrame->format);
                size_t dataSize = (size_t)inputFrame->stride * inputFrame->height;
                uint8_t* newData = new uint8_t[dataSize];
                memcpy(newData, inputFrame->data, dataSize);
                outFrame->width = inputFrame->width;
                outFrame->height = inputFrame->height;
                outFrame->format = inputFrame->format;
                outFrame->stride = inputFrame->stride;
                outFrame->data = newData;
                outFrame->ownsData = 1;
                outFrame->gpuTexture = nullptr;
                outFrame->ownsGpuTexture = 0;
                outFrame->timestampNanos = inputFrame->timestampNanos;
                outFrame->rotation = inputFrame->rotation;
                outFrame->isMirrored = inputFrame->isMirrored;
                outFrame->dataU = nullptr;
                outFrame->dataV = nullptr;
                outFrame->strideU = 0;
                outFrame->strideV = 0;
                outFrame->nativeHandle = nullptr;
            } else {
                *outFrame = *inputFrame;
                outFrame->ownsData = 0;
                outFrame->ownsGpuTexture = 0;
            }
            return HF_RESULT_OK;
        }

        // 3. For each face (Phase 4 minimal: only first face), generate masks and makeup
        // Convert input frame to HFImage (RGBA8)
        huanface::HFImage inputImage;
        inputImage.width = inputFrame->width;
        inputImage.height = inputFrame->height;
        inputImage.channels = 4;
        // Handle format
        if (inputFrame->format == HF_FORMAT_RGBA8) {
            inputImage.data.resize((size_t)inputFrame->width*inputFrame->height*4);
            for (int y=0;y<inputFrame->height;++y) {
                const uint8_t* srcRow = inputFrame->data + y*inputFrame->stride;
                uint8_t* dstRow = inputImage.data.data() + y*inputFrame->width*4;
                memcpy(dstRow, srcRow, (size_t)inputFrame->width*4);
            }
        } else if (inputFrame->format == HF_FORMAT_BGRA8) {
            inputImage.data.resize((size_t)inputFrame->width*inputFrame->height*4);
            for (int y=0;y<inputFrame->height;++y) {
                const uint8_t* srcRow = inputFrame->data + y*inputFrame->stride;
                uint8_t* dstRow = inputImage.data.data() + y*inputFrame->width*4;
                for (int x=0;x<inputFrame->width;++x) {
                    dstRow[x*4+0]=srcRow[x*4+2];
                    dstRow[x*4+1]=srcRow[x*4+1];
                    dstRow[x*4+2]=srcRow[x*4+0];
                    dstRow[x*4+3]=srcRow[x*4+3];
                }
            }
        } else {
            // Unsupported format for makeup, fallback to pass-through
            if (inputFrame->data) {
                size_t dataSize = (size_t)inputFrame->stride * inputFrame->height;
                uint8_t* newData = new uint8_t[dataSize];
                memcpy(newData, inputFrame->data, dataSize);
                outFrame->width = inputFrame->width;
                outFrame->height = inputFrame->height;
                outFrame->format = inputFrame->format;
                outFrame->stride = inputFrame->stride;
                outFrame->data = newData;
                outFrame->ownsData = 1;
                outFrame->gpuTexture = nullptr;
                outFrame->ownsGpuTexture = 0;
                outFrame->timestampNanos = inputFrame->timestampNanos;
                outFrame->rotation = inputFrame->rotation;
                outFrame->isMirrored = inputFrame->isMirrored;
                outFrame->dataU=nullptr; outFrame->dataV=nullptr; outFrame->strideU=0; outFrame->strideV=0; outFrame->nativeHandle=nullptr;
            } else {
                *outFrame = *inputFrame;
                outFrame->ownsData=0; outFrame->ownsGpuTexture=0;
            }
            return HF_RESULT_OK;
        }

        // Generate face mask and lip mask
        huanface::FaceMask faceMask, lipMask;
        std::string maskError;
        const auto& face = tracking.faces[0];
        if (engine->maskGenerator) {
            engine->maskGenerator->GenerateFaceMask(face.mesh, inputImage.width, inputImage.height, faceMask, maskError);
            engine->maskGenerator->GenerateFeatureMask(face, huanface::FeatureMaskType::LIP, inputImage.width, inputImage.height, lipMask, maskError);
        }

        // BEAUTY: run the real beauty engine (CPU reference path) with the
        // generic "beauty.*" float parameters set through HF_SetParameterFloat
        // (names per HFBeautyParameters::ToFloatMap). Zero retouch intensity
        // keeps a cheap pass-through; failures degrade to pass-through.
        huanface::HFImage beautified = inputImage;
        if (engine->beautyEngine && engine->beautyEngine->GetFullEngine()) {
            huanface::HFBeautyParameters bp;
            auto applyF = [&](const char* name, float& target) {
                auto it = engine->floatParams.find(name);
                if (it != engine->floatParams.end()) target = it->second;
            };
            auto applyB = [&](const char* name, bool& target) {
                auto it = engine->floatParams.find(name);
                if (it != engine->floatParams.end()) target = (it->second != 0.0f);
            };
            applyB("beauty.enabled", bp.enabled);
            applyF("beauty.globalIntensity", bp.globalIntensity);
            applyF("beauty.opacity", bp.opacity);
            applyB("beauty.smoothing.enabled", bp.smoothing.enabled);
            applyF("beauty.smoothing.intensity", bp.smoothing.intensity);
            applyF("beauty.smoothing.radius", bp.smoothing.radius);
            applyF("beauty.smoothing.opacity", bp.smoothing.opacity);
            applyF("beauty.smoothing.edgePreservation", bp.smoothing.edgePreservation);
            applyB("beauty.texture.enabled", bp.texture.enabled);
            applyF("beauty.texture.intensity", bp.texture.intensity);
            applyF("beauty.texture.preservation", bp.texture.preservation);
            applyF("beauty.texture.opacity", bp.texture.opacity);
            applyB("beauty.blemish.enabled", bp.blemish.enabled);
            applyF("beauty.blemish.intensity", bp.blemish.intensity);
            applyF("beauty.blemish.radius", bp.blemish.radius);
            applyF("beauty.blemish.opacity", bp.blemish.opacity);
            applyB("beauty.tone.enabled", bp.tone.enabled);
            applyF("beauty.tone.intensity", bp.tone.intensity);
            applyF("beauty.tone.temperature", bp.tone.temperature);
            applyF("beauty.tone.tint", bp.tone.tint);
            applyF("beauty.tone.saturation", bp.tone.saturation);
            applyF("beauty.tone.opacity", bp.tone.opacity);
            applyB("beauty.brightness.enabled", bp.brightness.enabled);
            applyF("beauty.brightness.intensity", bp.brightness.intensity);
            applyF("beauty.brightness.opacity", bp.brightness.opacity);
            applyB("beauty.contrast.enabled", bp.contrast.enabled);
            applyF("beauty.contrast.intensity", bp.contrast.intensity);
            applyF("beauty.contrast.opacity", bp.contrast.opacity);
            applyB("beauty.retouch.enabled", bp.retouch.enabled);
            applyF("beauty.retouch.intensity", bp.retouch.intensity);
            applyF("beauty.retouch.smoothing", bp.retouch.smoothing);
            applyF("beauty.retouch.texture", bp.retouch.texture);
            applyF("beauty.retouch.blemish", bp.retouch.blemish);
            applyF("beauty.retouch.tone", bp.retouch.tone);
            applyF("beauty.retouch.brightness", bp.retouch.brightness);
            applyF("beauty.retouch.contrast", bp.retouch.contrast);
            applyF("beauty.retouch.opacity", bp.retouch.opacity);
            if (bp.enabled && !huanface::IsBeautyIntensityZero(bp.retouch.intensity)) {
                huanface::HFImage beautyOut;
                std::string beautyError;
                const HFResult beautyRes = engine->beautyEngine->GetFullEngine()->ProcessCPU(
                    inputImage, tracking, bp, beautyOut, beautyError);
                if (beautyRes == HF_RESULT_OK && !beautyOut.data.empty()) {
                    beautified = std::move(beautyOut);
                }
                // On failure: keep pass-through (the pipeline must never break).
            }
        }

        // Makeup params from engine
        huanface::MakeupParams makeupParams;
        // Read intensity_lip param if set
        {
            auto it = engine->floatParams.find("intensity_lip");
            if (it != engine->floatParams.end()) makeupParams.intensityLip = it->second;
            auto it2 = engine->floatParams.find("intensity");
            if (it2 != engine->floatParams.end()) makeupParams.intensityLip = it2->second;
        }
        {
            auto it = engine->colorParams.find("lip_color");
            if (it != engine->colorParams.end()) makeupParams.lipColor = it->second;
        }

        huanface::HFImage makeupOutput;
        std::string makeupError;
        HFResult makeupRes = HF_RESULT_OK;
        if (engine->makeupEngine && engine->makeupEngine->GetProto()) {
            if (lipMask.IsValid()) {
                makeupRes = engine->makeupEngine->GetProto()->Process(beautified, face, lipMask, makeupParams, makeupOutput, makeupError);
            } else {
                makeupOutput = beautified;
            }
        } else {
            makeupOutput = beautified;
        }

        if (makeupRes != HF_RESULT_OK) {
            makeupOutput = beautified;
        }

        // Convert makeupOutput HFImage to HFFrameC output
        // Allocate owned data
        size_t outDataSize = (size_t)makeupOutput.width * makeupOutput.height * 4;
        uint8_t* outData = new uint8_t[outDataSize];
        memcpy(outData, makeupOutput.data.data(), outDataSize);

        // If input was BGRA8, convert back
        if (inputFrame->format == HF_FORMAT_BGRA8) {
            for (size_t i=0;i<outDataSize;i+=4) {
                std::swap(outData[i+0], outData[i+2]);
            }
        }

        outFrame->width = makeupOutput.width;
        outFrame->height = makeupOutput.height;
        outFrame->format = inputFrame->format;
        outFrame->stride = makeupOutput.width * 4;
        outFrame->data = outData;
        outFrame->ownsData = 1;
        outFrame->dataU = nullptr;
        outFrame->dataV = nullptr;
        outFrame->strideU = 0;
        outFrame->strideV = 0;
        outFrame->gpuTexture = nullptr;
        outFrame->ownsGpuTexture = 0;
        outFrame->timestampNanos = inputFrame->timestampNanos;
        outFrame->rotation = inputFrame->rotation;
        outFrame->isMirrored = inputFrame->isMirrored;
        outFrame->nativeHandle = nullptr;

        // Render backend: if D3D11 available and GPU texture, would blit
        // For Phase 4 minimal Linux, CPU path is used

        return HF_RESULT_OK;
    } catch (const std::bad_alloc&) {
        return HF_RESULT_OUT_OF_MEMORY;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_ProcessFrameWithBundle(HFEngine engine, const HFFrameC* inputFrame, HFBundle bundle, HFFrameC* outFrame) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !inputFrame || !bundle || !outFrame) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;

    // For Phase 3 minimal, ignore bundle and call normal ProcessFrame
    // In real impl, would use bundle's shaders and textures
    return HF_ProcessFrame(engine, inputFrame, outFrame);
}

HFResult HF_GetFaceData(HFEngine engine, HFTrackingDataC* outTrackingData) {
    if (!g_initialized.load()) return HF_RESULT_NOT_INITIALIZED;
    if (!engine || !outTrackingData) return HF_RESULT_INVALID_PARAM;
    if (!engine->initialized) return HF_RESULT_NOT_INITIALIZED;

    try {
        std::lock_guard<std::mutex> lock(engine->mutex);
        if (!engine->hasLastTracking) {
            outTrackingData->faceCount = 0;
            outTrackingData->faces = nullptr;
            outTrackingData->timestampNanos = 0;
            return HF_RESULT_OK;
        }

        // Deep copy tracking data for C ABI (caller must free via HF_FreeFaceData)
        outTrackingData->faceCount = engine->lastTrackingData.faceCount;
        outTrackingData->timestampNanos = engine->lastTrackingData.timestampNanos;
        if (engine->lastTrackingData.faceCount > 0 && engine->lastTrackingData.faces) {
            outTrackingData->faces = new HFFaceDataC[engine->lastTrackingData.faceCount];
            for (int i=0;i<engine->lastTrackingData.faceCount;++i) {
                outTrackingData->faces[i] = engine->lastTrackingData.faces[i];
                // Deep copy landmarks
                if (engine->lastTrackingData.faces[i].landmarkCount > 0 && engine->lastTrackingData.faces[i].landmarks) {
                    int count = engine->lastTrackingData.faces[i].landmarkCount;
                    outTrackingData->faces[i].landmarks = new float[count*2];
                    memcpy(outTrackingData->faces[i].landmarks, engine->lastTrackingData.faces[i].landmarks, count*2*sizeof(float));
                } else {
                    outTrackingData->faces[i].landmarks = nullptr;
                }
                if (engine->lastTrackingData.faces[i].landmarkCount > 0 && engine->lastTrackingData.faces[i].landmarks3D) {
                    int count = engine->lastTrackingData.faces[i].landmarkCount;
                    outTrackingData->faces[i].landmarks3D = new float[count*3];
                    memcpy(outTrackingData->faces[i].landmarks3D, engine->lastTrackingData.faces[i].landmarks3D, count*3*sizeof(float));
                } else {
                    outTrackingData->faces[i].landmarks3D = nullptr;
                }
            }
        } else {
            outTrackingData->faces = nullptr;
        }

        return HF_RESULT_OK;
    } catch (const std::bad_alloc&) {
        return HF_RESULT_OUT_OF_MEMORY;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_FreeFaceData(HFTrackingDataC* trackingData) {
    if (!trackingData) return HF_RESULT_INVALID_PARAM;
    try {
        if (trackingData->faces) {
            for (int i=0;i<trackingData->faceCount;++i) {
                if (trackingData->faces[i].landmarks) delete[] trackingData->faces[i].landmarks;
                if (trackingData->faces[i].landmarks3D) delete[] trackingData->faces[i].landmarks3D;
            }
            delete[] trackingData->faces;
        }
        trackingData->faces = nullptr;
        trackingData->faceCount = 0;
        trackingData->timestampNanos = 0;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

HFResult HF_FreeFrame(HFFrameC* frame) {
    if (!frame) return HF_RESULT_INVALID_PARAM;
    try {
        if (frame->ownsData && frame->data) {
            delete[] frame->data;
            frame->data = nullptr;
        }
        if (frame->ownsData && frame->dataU) {
            delete[] frame->dataU;
            frame->dataU = nullptr;
        }
        if (frame->ownsData && frame->dataV) {
            delete[] frame->dataV;
            frame->dataV = nullptr;
        }
        // GPU texture ownership: for Phase 3, we don't own GPU texture via C API, engine owns via backend
        // So we just null it if ownsGpuTexture
        if (frame->ownsGpuTexture && frame->gpuTexture) {
            // In real impl, would call backend DestroyTexture
            frame->gpuTexture = nullptr;
        }
        frame->width = 0;
        frame->height = 0;
        frame->format = HF_FORMAT_UNKNOWN;
        frame->ownsData = 0;
        frame->ownsGpuTexture = 0;
        return HF_RESULT_OK;
    } catch (...) {
        return HF_RESULT_FAIL;
    }
}

} // extern "C"
