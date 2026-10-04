/**
 * Beauty Pipeline — Phase 8 Production D3D11 Rendering & Performance Optimization
 * GPU-resident ping-pong pipeline: Input -> Beauty Mask -> Beauty Passes -> Makeup -> Output
 * Avoids CPU readback, uses pooled intermediate RTs
 * Pipeline:
 *  Input Texture (RGBA8) 
 *   -> [Beauty] Acquire RT A (same size)
 *      - Smoothing: Input + skinMask -> RT A (bilateral-like HLSL)
 *      - Texture: RT A + mask -> RT B (ping-pong)
 *      - Blemish: RT B -> RT A
 *      - Tone: RT A -> RT B
 *      - Brightness/Contrast: RT B -> RT A
 *      Final beauty result in RT A (GPU resident)
 *   -> [Makeup] RT A -> RT B with makeup blending (foundation, blush, etc)
 *   -> Output RT (same as input size) -> Present or Readback only at boundary
 * No Map/Unmap/CopyResource between passes, only RT switches and Draw calls
 * CPU fallback uses same logic with HFImage but optimized ROI
 */

#pragma once
#include "../../include/huanface_c_api.h"
#include "../rendering/render_backend.h"
#include "../rendering/resource_pool.h"
#include "beauty_params.h"
#include "beauty_mask.h"
#include <vector>
#include <memory>

namespace huanface {

class BeautyPipeline {
public:
    BeautyPipeline() = default;
    ~BeautyPipeline() { Shutdown(); }

    bool Init(IRenderBackend* backend) {
        if(!backend || !backend->IsInitialized()) return false;
        backend_=backend;
        pool_.Init(backend);
        initialized_=true;
        return true;
    }

    void Shutdown() {
        if(!initialized_) return;
        pool_.Clear();
        backend_=nullptr;
        initialized_=false;
    }

    bool IsInitialized() const { return initialized_; }

    // GPU path — ping-pong
    struct GPUPipelineResult {
        IRenderTarget* finalRT=nullptr;
        bool success=false;
        std::string error;
    };

    // Acquire two ping-pong RTs
    struct PingPongRTs {
        IRenderTarget* rtA=nullptr;
        IRenderTarget* rtB=nullptr;
        IRenderTarget* current=nullptr; // points to latest result
        bool isA=true;
        void Swap() {
            isA=!isA;
            current = isA ? rtA : rtB;
        }
    };

    PingPongRTs AcquirePingPong(int width, int height, HFFormat format=HF_FORMAT_RGBA8) {
        PingPongRTs pp;
        pp.rtA = pool_.AcquireRenderTarget(width,height,format);
        pp.rtB = pool_.AcquireRenderTarget(width,height,format);
        pp.current = pp.rtA;
        pp.isA=true;
        return pp;
    }

    void ReleasePingPong(PingPongRTs& pp) {
        if(pp.rtA) pool_.ReleaseRenderTarget(pp.rtA);
        if(pp.rtB) pool_.ReleaseRenderTarget(pp.rtB);
        pp.rtA=pp.rtB=pp.current=nullptr;
    }

    // CPU path optimized — same semantics, no global blur, ROI only
    HFResult ExecuteCPUPipeline(const HFImage& input,
                                 const HFFaceData& face,
                                 const std::map<BeautyMaskType, HFBeautyMask>& masks,
                                 const HFBeautyParameters& params,
                                 HFImage& output,
                                 std::string& err);

    // For benchmarking: measure CPU path
    struct PipelineMetrics {
        double maskGenerationMs=0;
        double smoothingMs=0;
        double textureMs=0;
        double blemishMs=0;
        double toneMs=0;
        double brightnessMs=0;
        double contrastMs=0;
        double totalMs=0;
    };

    PipelineMetrics GetLastMetrics() const { return lastMetrics_; }

private:
    IRenderBackend* backend_=nullptr;
    GPUResourcePool pool_;
    bool initialized_=false;
    PipelineMetrics lastMetrics_;
};

} // namespace huanface
