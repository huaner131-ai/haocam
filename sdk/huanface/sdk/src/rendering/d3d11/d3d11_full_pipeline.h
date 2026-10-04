/**
 * HuanFace Full GPU Beauty & Makeup Pipeline — Phase 9
 * Real D3D11 multi-pass chaining with ping-pong render targets
 * No CPU readback between passes, production HLSL, real masks, real parameters, real GPU timing
 */

#pragma once

#ifdef _WIN32

#include "d3d11_backend.h"
#include "../../beauty/beauty_params.h"
#include "../../beauty/beauty_mask.h"
#include "../../makeup/makeup_params.h"
#include "../../makeup/makeup_mask.h"
#include <vector>
#include <string>
#include <map>

namespace huanface {

// ============================================================================
// Beauty Constants — must match beauty_common.hlsl cbuffer layout
// ============================================================================
struct BeautyConstantsGPU {
    float skinColor[4] = {0,0,0,0};
    float smoothingIntensity = 0.5f;
    float smoothingRadius = 2.0f;
    float smoothingOpacity = 0.8f;
    float edgePreservation = 0.6f;

    float textureIntensity = 0.5f;
    float texturePreservation = 0.7f;
    float textureOpacity = 0.7f;
    float textureDetailThreshold = 0.3f;

    float blemishIntensity = 0.5f;
    float blemishRadius = 2.5f;
    float blemishOpacity = 0.8f;
    float pad0 = 0;

    float toneIntensity = 0.5f;
    float toneTemperature = 0.0f;
    float toneTint = 0.0f;
    float toneSaturation = 0.0f;

    float brightness = 0.0f;
    float contrast = 0.0f;
    float beautyOpacity = 0.9f;
    float globalIntensity = 1.0f;

    float texelSize[2] = {0.0025f, 0.0025f};
    float pad1[2] = {0,0};
};

// Makeup Constants — must match makeup_common.hlsl
struct MakeupConstantsGPU {
    float makeupColor[4] = {1.0f, 0.2f, 0.3f, 1.0f};
    float makeupIntensity = 0.8f;
    float makeupOpacity = 0.9f;
    float makeupFeather = 1.0f;
    float makeupScale = 1.0f;
    float blendParams[4] = {0, 1.0f, 0.5f, 1.1f}; // x=blendMode, y=thickness, z=irisEnhance, w=scale
};

// ============================================================================
// GPU Pass Identification — for logging/debugging per Phase 9 requirement
// ============================================================================
struct GPUPassInfo {
    std::string name;
    std::string inputResource;
    std::string outputResource;
    std::string shaderFile;
    std::string entryPoint;
    std::string target;
    int width = 0;
    int height = 0;
    int faceIndex = -1;
    double gpuMs = 0;
    bool valid = false;
};

// ============================================================================
// Ping-Pong Render Targets
// ============================================================================
struct PingPongRTs {
    IRenderTarget* rtA = nullptr;
    IRenderTarget* rtB = nullptr;
    IRenderTarget* current = nullptr; // latest output
    bool isA = true;
    int width = 0;
    int height = 0;

    void Swap() {
        isA = !isA;
        current = isA ? rtA : rtB;
    }
    IRenderTarget* GetSource() const { return isA ? rtB : rtA; } // previous
    IRenderTarget* GetDest() const { return isA ? rtA : rtB; }   // next to write
};

// ============================================================================
// Full GPU Pipeline — Phase 9
// ============================================================================
class FullGPUPipeline {
public:
    FullGPUPipeline() = default;
    ~FullGPUPipeline() { Shutdown(); }

    HFResult Init(IRenderBackend* backend);
    void Shutdown();
    bool IsInitialized() const { return initialized_; }

    // Compile all production shaders
    HFResult CompileShaders();

    // Resource management
    PingPongRTs AcquirePingPong(int width, int height, HFFormat format = HF_FORMAT_RGBA8);
    void ReleasePingPong(PingPongRTs& pp);
    IGpuTexture* CreateTextureFromImage(const HFImage& img);
    IGpuTexture* CreateTextureFromMask(const HFBeautyMask& mask);
    IGpuTexture* CreateTextureFromMakeupMask(const HFMakeupMask& mask);

    // Beauty pipeline: Input -> Smoothing -> Texture -> Blemish -> Tone -> Brightness -> Contrast -> Retouch -> BeautyOutput
    // Returns final RT in ping-pong (caller must not release until done)
    // Each pass uses output of previous as input (real chaining)
    struct BeautyPipelineResult {
        bool success = false;
        std::string error;
        PingPongRTs pingPong;
        std::vector<GPUPassInfo> passes;
        double totalGpuMs = 0;
        double perPassMs[7] = {0}; // smoothing, texture, blemish, tone, brightness, contrast, retouch
    };

    BeautyPipelineResult ExecuteBeautyPipeline(
        IGpuTexture* inputTexture,
        IGpuTexture* skinMaskTexture,
        const HFBeautyParameters& params,
        int width, int height);

    // Makeup pipeline: BeautyOutput -> Foundation -> Blush -> Eyeshadow -> Eyebrow -> Eyeliner -> Eyelash -> Lip -> Pupil -> Blend -> Final
    struct MakeupPipelineResult {
        bool success = false;
        std::string error;
        PingPongRTs pingPong;
        std::vector<GPUPassInfo> passes;
        double totalGpuMs = 0;
        double perPassMs[9] = {0}; // foundation, blush, eyeshadow, eyebrow, eyeliner, eyelash, lip, pupil, blend
    };

    MakeupPipelineResult ExecuteMakeupPipeline(
        IGpuTexture* beautyOutputTexture,
        const std::map<MakeupMaskType, IGpuTexture*>& makeupMaskTextures,
        const HFMakeupParameters& params,
        int width, int height);

    // Full pipeline: Beauty -> Makeup
    struct FullPipelineResult {
        bool success = false;
        std::string error;
        PingPongRTs pingPong; // final RT is pingPong.current
        std::vector<GPUPassInfo> beautyPasses;
        std::vector<GPUPassInfo> makeupPasses;
        double beautyGpuMs = 0;
        double makeupGpuMs = 0;
        double totalGpuMs = 0;
    };

    FullPipelineResult ExecuteFullPipeline(
        IGpuTexture* inputTexture,
        IGpuTexture* skinMaskTexture,
        const std::map<MakeupMaskType, IGpuTexture*>& makeupMaskTextures,
        const HFBeautyParameters& beautyParams,
        const HFMakeupParameters& makeupParams,
        int width, int height);

    // Validation: prove chaining - output A != input, and B uses A not original
    struct ChainingValidationResult {
        bool isChained = false;
        std::string details;
        double diffA = 0; // difference between input and after pass A
        double diffB = 0; // difference between A and B, and between original and B
    };

    ChainingValidationResult ValidateBeautyChaining(IGpuTexture* input, IGpuTexture* skinMask, const HFBeautyParameters& params, int w, int h);
    ChainingValidationResult ValidateMakeupChaining(IGpuTexture* beautyOutput, const std::map<MakeupMaskType, IGpuTexture*>& masks, const HFMakeupParameters& params, int w, int h);

    // Getters for testing
    D3D11Backend* GetD3D11Backend() const { return d3dBackend_; }
    bool AreShadersCompiled() const { return shadersCompiled_; }
    std::string GetShaderCompileLog() const { return shaderLog_; }

    // Readback for validation — allowed ONLY after complete pipeline, not between production passes
    // Production pipeline: GPU->GPU->GPU, Validation: Final RT -> staging/readback -> CPU
    HFImage ReadbackTexture(IGpuTexture* tex);
    HFImage ReadbackRenderTarget(IRenderTarget* rt);

    // Sentinel chaining test — proves actual texture dependency, not just name
    struct SentinelResult {
        bool isChained = false;
        double diffA = 0; // diff between input and pass A output
        double diffBvsA = 0; // diff between pass A output and pass B using A
        double diffBvsOriginal = 0; // diff between pass B using A vs using original
        double diffBOrigVsAOrig = 0; // diff between B(original) vs B(A)
        std::string details;
    };
    SentinelResult ExecuteChainingSentinelTest(IGpuTexture* input, IGpuTexture* skinMask, int w, int h);

    // GPU output comparison helpers
    static double CalcMAE(const HFImage& a, const HFImage& b);
    static double CalcMaxError(const HFImage& a, const HFImage& b);
    static double CalcRMSE(const HFImage& a, const HFImage& b);

    // For resource lifetime tracking
    struct ResourceStats {
        int textureCount = 0;
        int rtCount = 0;
        int shaderCount = 0;
    };
    ResourceStats GetResourceStats() const { return stats_; }

private:
    bool initialized_ = false;
    IRenderBackend* backend_ = nullptr;
    D3D11Backend* d3dBackend_ = nullptr;
    GPUResourcePool pool_;
    bool shadersCompiled_ = false;
    std::string shaderLog_;

    // Production shaders
    struct Shaders {
        IShader* beautySmoothing = nullptr;
        IShader* beautyTexture = nullptr;
        IShader* beautyBlemish = nullptr;
        IShader* beautyTone = nullptr;
        IShader* beautyBrightness = nullptr;
        IShader* beautyContrast = nullptr;
        IShader* beautyFinal = nullptr;

        IShader* makeupFoundation = nullptr;
        IShader* makeupBlush = nullptr;
        IShader* makeupEyeshadow = nullptr;
        IShader* makeupEyebrow = nullptr;
        IShader* makeupEyeliner = nullptr;
        IShader* makeupEyelash = nullptr;
        IShader* makeupLip = nullptr;
        IShader* makeupPupil = nullptr;
        IShader* makeupBlend = nullptr;
    } shaders_;

    // Constant buffers
    ComPtr<ID3D11Buffer> beautyCB_;
    ComPtr<ID3D11Buffer> makeupCB_;
    ComPtr<ID3D11SamplerState> linearSampler_;
    ComPtr<ID3D11SamplerState> pointSampler_;

    // Fullscreen quad
    IMesh* quadMesh_ = nullptr;

    ResourceStats stats_;

    // Helpers
    bool CreateConstantBuffers();
    bool CreateSamplers();
    bool CreateQuad();
    void UpdateBeautyConstants(const HFBeautyParameters& params, int w, int h);
    void UpdateMakeupConstants(const HFFloat4& color, float intensity, float opacity, HFBlendMode blendMode, float thickness, float irisEnhance, float scale);
    void BindBeautyTextures(IGpuTexture* input, IGpuTexture* skinMask, IGpuTexture* intermediate = nullptr);
    void BindMakeupTextures(IGpuTexture* input, IGpuTexture* mask, IGpuTexture* makeupTex = nullptr);
    void UnbindTextures(int count = 4);
    void DrawQuad(IShader* shader);
    double ExecutePassWithTiming(IShader* shader, IRenderTarget* srcRT, IRenderTarget* dstRT, const std::string& passName, GPUPassInfo& outInfo);

    std::string FindShaderPath(const std::string& fileName);
    IShader* CompileShaderFromFile(const std::string& fileName);
};

} // namespace huanface

#endif // _WIN32
