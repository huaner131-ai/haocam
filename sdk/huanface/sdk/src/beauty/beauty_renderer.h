/**
 * Beauty Renderer — Phase 7 Full Beauty & Face Retouching Engine
 * REAL landmark -> REAL mesh -> REAL skin mask -> REAL beauty algorithm -> REAL HLSL -> REAL D3D11 -> Makeup -> Output
 * CPU reference path + GPU D3D11 path
 */

#pragma once
#include "beauty_mask.h"
#include "beauty_params.h"
#include "../face/face_data.h"
#include "../../include/huanface/huanface_image.h"
#include "../../include/huanface_c_api.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace huanface {

// ============================================================================
// Beauty Feature Order (deterministic pipeline)
// ============================================================================
enum class BeautyFeatureOrder {
    Smoothing = 0,
    Texture = 1,
    Blemish = 2,
    Tone = 3,
    Brightness = 4,
    Contrast = 5,
    Count = 6
};

inline std::string BeautyFeatureOrderToString(BeautyFeatureOrder o) {
    switch(o){
        case BeautyFeatureOrder::Smoothing: return "Smoothing";
        case BeautyFeatureOrder::Texture: return "Texture";
        case BeautyFeatureOrder::Blemish: return "Blemish";
        case BeautyFeatureOrder::Tone: return "Tone";
        case BeautyFeatureOrder::Brightness: return "Brightness";
        case BeautyFeatureOrder::Contrast: return "Contrast";
        default: return "Unknown";
    }
}

// ============================================================================
// CPU Reference Beauty Renderer — for validation
// ============================================================================
class CPUBeautyRenderer {
public:
    CPUBeautyRenderer() = default;
    ~CPUBeautyRenderer() = default;

    HFResult Init();
    void Shutdown();

    // Process single face with all beauty features, CPU reference
    HFResult ProcessFace(const HFImage& input,
                         const HFFaceData& face,
                         const HFBeautyParameters& params,
                         const std::map<BeautyMaskType, HFBeautyMask>& masks,
                         HFImage& outOutput,
                         std::string& outError);

    // Process multi-face
    HFResult ProcessMultiFace(const HFImage& input,
                              const HFTrackingData& tracking,
                              const HFBeautyParameters& params,
                              HFImage& outOutput,
                              std::string& outError);

    // Individual feature renderers — CPU
    HFResult RenderSmoothing(const HFImage& input, const HFFaceData& face, const HFBeautyMask& skinMask,
                             const HFSkinSmoothingParams& params, HFImage& out, std::string& err);
    HFResult RenderTextureRefinement(const HFImage& input, const HFFaceData& face, const HFBeautyMask& skinMask,
                                     const HFSkinTextureParams& params, HFImage& out, std::string& err);
    HFResult RenderBlemishReduction(const HFImage& input, const HFFaceData& face, const HFBeautyMask& skinMask,
                                    const HFBlemishReductionParams& params, HFImage& out, std::string& err);
    HFResult RenderSkinTone(const HFImage& input, const HFFaceData& face, const HFBeautyMask& skinMask,
                            const HFSkinToneParams& params, HFImage& out, std::string& err);
    HFResult RenderBrightness(const HFImage& input, const HFFaceData& face, const HFBeautyMask& skinMask,
                              const HFBrightnessParams& params, HFImage& out, std::string& err);
    HFResult RenderContrast(const HFImage& input, const HFFaceData& face, const HFBeautyMask& skinMask,
                            const HFContrastParams& params, HFImage& out, std::string& err);
    HFResult RenderRetouch(const HFImage& input, const HFFaceData& face,
                           const std::map<BeautyMaskType, HFBeautyMask>& masks,
                           const HFBeautyParameters& params, HFImage& out, std::string& err);

    // Performance breakdown
    struct PerfMetrics {
        double maskMs=0;
        double smoothingMs=0;
        double textureMs=0;
        double blemishMs=0;
        double toneMs=0;
        double brightnessMs=0;
        double contrastMs=0;
        double totalMs=0;
    };
    PerfMetrics GetLastPerf() const { return lastPerf; }

private:
    bool initialized = false;
    PerfMetrics lastPerf;
    static uint8_t FloatToU8(float f) { return (uint8_t)std::max(0.0f, std::min(255.0f, f*255.0f)); }
    static float U8ToFloat(uint8_t v) { return v/255.0f; }

    // Edge-aware smoothing helpers — Phase 8 Optimized with ROI
    static HFImage BilateralLikeBlur(const HFImage& input, const HFBeautyMask& mask, float radius, float edgePreservation, float intensity);
    static HFImage GaussianBlur(const HFImage& input, float radius);
    static HFImage GaussianBlurROI(const HFImage& input, const HFBeautyMask& mask, float radius);
};

// ============================================================================
// D3D11 GPU Beauty Renderer — real HLSL shaders
// ============================================================================
class D3D11BeautyRenderer {
public:
    D3D11BeautyRenderer() = default;
    ~D3D11BeautyRenderer() = default;

    HFResult Init(void* d3d11Device, void* d3d11Context);
    void Shutdown();

    bool IsInitialized() const { return initialized; }

    HFResult ProcessFaceGPU(const HFImage& input,
                            const HFFaceData& face,
                            const HFBeautyParameters& params,
                            const std::map<BeautyMaskType, HFBeautyMask>& masks,
                            HFImage& outOutput,
                            std::string& outError);

    bool AreShadersCompiled() const { return shadersCompiled; }
    std::string GetShaderCompileLog() const { return shaderCompileLog; }
    bool AreResourcesCreated() const { return resourcesCreated; }

private:
    bool initialized = false;
    bool shadersCompiled = false;
    bool resourcesCreated = false;
    std::string shaderCompileLog;
    void* device = nullptr;
    void* context = nullptr;

    std::string LoadShaderSource(const std::string& name);
    bool CompileShaders();

    struct GPUResources {
        bool inputTextureCreated = false;
        bool maskTextureCreated = false;
        bool outputTextureCreated = false;
        bool intermediateTextureCreated = false;
        bool constantBufferCreated = false;
        bool samplerCreated = false;
        bool vertexShaderCreated = false;
        bool pixelShaderCreated = false;
    } gpuResources;
};

// ============================================================================
// Full Beauty Engine — Phase 7, deterministic pipeline
// ============================================================================
class FullBeautyEngine {
public:
    FullBeautyEngine() = default;
    ~FullBeautyEngine() = default;

    HFResult Init(const HFEngineConfigC& config);
    void Shutdown();

    // Main process: input frame + tracking data + params -> output frame
    // Pipeline: Input -> Face Data -> Beauty Masks -> Smoothing -> Texture -> Blemish -> Tone -> Brightness/Contrast -> Output
    HFResult Process(const HFFrameC* input,
                     const HFTrackingData& tracking,
                     const HFBeautyParameters& params,
                     HFFrameC& outOutput,
                     std::string& outError);

    // CPU reference path
    HFResult ProcessCPU(const HFImage& input,
                        const HFTrackingData& tracking,
                        const HFBeautyParameters& params,
                        HFImage& outOutput,
                        std::string& outError);

    // GPU path
    HFResult ProcessGPU(const HFImage& input,
                        const HFTrackingData& tracking,
                        const HFBeautyParameters& params,
                        HFImage& outOutput,
                        std::string& outError);

    // Mask generation for debug
    HFResult GenerateDebugMasks(const HFFaceData& face, int w, int h,
                                std::map<BeautyMaskType, HFBeautyMask>& outMasks,
                                std::string& err);

    void SetParameters(const HFBeautyParameters& p) { params = p; }
    HFBeautyParameters GetParameters() const { return params; }

    void EnableFeature(BeautyFeatureOrder feature, bool enabled);
    bool IsFeatureEnabled(BeautyFeatureOrder feature) const;

    bool SaveDebugMasks(const std::map<BeautyMaskType, HFBeautyMask>& masks, const std::string& basePath);
    bool SaveDebugFeatureOutputs(const HFImage& input, const HFTrackingData& tracking, const std::string& basePath);

    bool AreResourcesValid() const;

    // Combined Beauty + Makeup pipeline order
    // Beauty before Makeup: smoothing -> foundation -> blush -> lip -> eye makeup
    std::vector<BeautyFeatureOrder> GetPipelineOrder() const { return pipelineOrder; }

private:
    bool initialized = false;
    HFBeautyParameters params;
    std::unique_ptr<HFBeautyMaskGenerator> maskGenerator;
    std::unique_ptr<CPUBeautyRenderer> cpuRenderer;
    std::unique_ptr<D3D11BeautyRenderer> gpuRenderer;

    std::vector<BeautyFeatureOrder> pipelineOrder = {
        BeautyFeatureOrder::Smoothing,
        BeautyFeatureOrder::Texture,
        BeautyFeatureOrder::Blemish,
        BeautyFeatureOrder::Tone,
        BeautyFeatureOrder::Brightness,
        BeautyFeatureOrder::Contrast
    };
};

} // namespace huanface
