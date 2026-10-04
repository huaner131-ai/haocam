#pragma once

// HuanFace beauty engine adapter (user's own SDK, github.com/hanlimzie-glitch/
// HuanFace). COMPILED ONLY when HAOCAM_ENABLE_HUANFACE=ON and the SDK drop-in
// exists under sdk/huanface/sdk (include/ + src/).
//
// Written strictly against the PUBLIC C ABI (sdk/include/huanface_c_api.h) -
// never internal SDK headers (same isolation rule as FacebetterProvider):
//
//   HF_Init(); HF_CreateEngine(HFEngineConfigC, HFEngine*)
//   HF_SetParameterFloat(engine, "beauty.<feature>.<field>", v)
//       (names per HFBeautyParameters::ToFloatMap in the SDK)
//   HF_ProcessFrame(engine, HFFrameC* in, HFFrameC* out)  // BGRA8 CPU in/out
//   HF_GetFaceData(engine, HFTrackingDataC*) / HF_FreeFaceData
//   HF_FreeFrame / HF_DestroyEngine
//
// The SDK runs its own face tracking internally (heuristic fallback without
// ONNX models - reported honestly in the status text) and its beauty engine
// CPU path (Phase 7). No reshape params exist yet in the SDK - face slim /
// eye / nose / jaw are reported as UNSUPPORTED (never faked).
//
// Threading: engine created + used ONLY on this provider's beauty worker
// thread. HF_SetParameterFloat is mutex-guarded inside the SDK, so config
// pushes are safe from the UI thread via the worker.
//
// Data path (documented per spec section 34; measured counters exposed):
//   D3D11 processed BGRA -> staging readback -> HF_ProcessFrame(BGRA8 in/out)
//   -> pooled D3D11 texture upload -> composite override.
//   No BGRA<->RGBA conversion needed: the SDK's face path accepts BGRA8
//   natively (it converts internally).

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "effects/beauty/BeautyProvider.h"

#if defined(_WIN32)
namespace haocam::gfx {
class GpuFrameCopier;
class D3D11TexturePool;
}
#endif

struct ID3D11Device;

namespace haocam {
class ITexturePool; // frame/TexturePool.h

class HuanFaceProvider final : public IBeautyProvider, public IBeautyAsync {
public:
    HuanFaceProvider();
    ~HuanFaceProvider() override;

    // `d3d11Device` is the pipeline device (raw ID3D11Device*) used only for
    // the readback/upload copies. The HuanFace engine itself is CPU-side.
    void configure(void* d3d11Device, ITexturePool* texturePool);

    // IEffectProvider
    bool initialize(const EffectContext& context) override;
    void shutdown() override;
    bool isAvailable() const override;
    std::string statusText() const override;
    const char* name() const override { return "HuanFace"; }
    ProviderType type() const override { return ProviderType::Beauty; }

    // IBeautyProvider (thread-safe, normalized values)
    void setSmoothing(float value) override;
    void setWhitening(float value) override;
    void setRosy(float value) override;
    void setSharpen(float value) override;
    void setFaceSlim(float value) override;
    void setEyeSize(float value) override;
    void setNoseSize(float value) override;
    void setJawSlim(float value) override;
    void reset() override;
    BeautyConfig config() const override;
    BeautyResult process(const Frame& input, const FaceData& face) override;

    // Feature support: reshape is NOT implemented by the SDK yet (Phase 8
    // scope) - reported honestly per spec section 17.
    Features features() const override {
        Features f;
        f.smoothing = true;  // beauty.smoothing.intensity
        f.whitening = true;  // beauty.brightness.intensity (skin-only lift)
        f.rosy = true;       // beauty.tone.tint/intensity (warm/magenta push)
        f.sharpen = true;    // beauty.texture.intensity (detail refinement)
        f.faceSlim = false;  // no reshape in HuanFace yet
        f.eyeSize = false;
        f.noseSize = false;
        f.jawSlim = false;
        return f;
    }

    // IBeautyAsync
    void submitFrame(const GpuTextureRef& texture, uint64_t frameId);
    bool latestOutput(GpuTextureRef& outTexture, uint64_t& outFrameId) const;

    // The SDK's CPU reference path costs >1 s per frame at 1440p, so outputs
    // legitimately lag far more than a fast GPU provider's. Hold-latest: the
    // compositor keeps showing the most recent beautified frame (the provider
    // also publishes no-face pass-throughs, so the preview stays live).
    uint64_t maxLagFrames() const;

    // Diagnostics
    double lastReadbackMs() const { return m_readbackMs.load(); }
    double lastSdkProcessMs() const { return m_sdkProcessMs.load(); }
    double lastUploadMs() const { return m_uploadMs.load(); }
    int detectedFaceCount() const { return m_faceCount.load(); }
    uint64_t processedFrames() const override { return m_processedFrames.load(); }

private:
    void run();
    bool createEngineLocked(); // beauty thread only
    void applyConfigToEngine(const BeautyConfig& config); // beauty thread only
    void setStatus(const std::string& status, bool available);
    std::shared_ptr<const BeautyConfig> snapshotConfig() const;
    void storeConfig(const BeautyConfig& config);

    // --- configuration (set before initialize) ---
    ID3D11Device* m_device = nullptr;
    ITexturePool* m_texturePool = nullptr;

    // --- parameter snapshot (UI thread writes, beauty thread reads) ---
    mutable std::mutex m_configMutex;
    std::shared_ptr<const BeautyConfig> m_config;

    // --- worker state (beauty thread only) ---
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
    void* m_engine = nullptr; // HFEngine (opaque; C API only in the .cpp)
#if defined(_WIN32)
    std::unique_ptr<gfx::GpuFrameCopier> m_copier; // Windows only (D3D11 copies)
    // Created when the EffectContext supplies no texture pool (the engine
    // currently never does): without it every processed frame was dropped.
    std::shared_ptr<gfx::D3D11TexturePool> m_ownPool;
#endif
    std::vector<uint8_t> m_readBuffer;  // BGRA readback scratch (reused)
    std::vector<uint8_t> m_smallBuffer; // downscaled SDK input (reused)
    std::vector<uint8_t> m_scaleBuffer; // upscaled SDK output (reused)
    BeautyConfig m_engineConfig;       // last applied to the engine

    // --- pending input slot (overwrite = drop-stale) ---
    std::mutex m_inputMutex;
    std::condition_variable m_inputCv;
    GpuTextureRef m_pendingTexture;
    uint64_t m_pendingFrameId = 0;
    bool m_hasPending = false;

    // --- latest output slot ---
    mutable std::mutex m_outputMutex;
    GpuTextureRef m_outputTexture;
    uint64_t m_outputFrameId = 0;

    // --- status ---
    mutable std::mutex m_statusMutex;
    std::string m_status = "Not initialized";
    bool m_available = false;

    // --- stats ---
    std::atomic<double> m_readbackMs{0.0};
    std::atomic<double> m_sdkProcessMs{0.0};
    std::atomic<double> m_uploadMs{0.0};
    std::atomic<int> m_faceCount{0};
    std::atomic<uint64_t> m_processedFrames{0};
    std::atomic<uint64_t> m_submittedFrames{0};
    std::atomic<uint64_t> m_processFailures{0};
    std::atomic<uint64_t> m_discardedFrames{0};
    std::atomic<uint64_t> m_passThroughFrames{0};
};


// Pure CPU BGRA8 resize helpers used to cap the HuanFace SDK input
// resolution (its CPU path scales with pixel count). Header-inline so the
// unit tests can exercise them on every platform.
inline void huanfaceDownscale2x(const uint8_t* src, int w, int h, uint8_t* dst) {
    const int dw = w / 2;
    const int dh = h / 2;
    for (int y = 0; y < dh; ++y) {
        const uint8_t* r0 = src + static_cast<size_t>(2 * y) * w * 4;
        const uint8_t* r1 = r0 + static_cast<size_t>(w) * 4;
        uint8_t* d = dst + static_cast<size_t>(y) * dw * 4;
        for (int x = 0; x < dw; ++x) {
            for (int c = 0; c < 4; ++c) {
                const int sum = r0[(2 * x) * 4 + c] + r0[(2 * x + 1) * 4 + c] +
                                r1[(2 * x) * 4 + c] + r1[(2 * x + 1) * 4 + c];
                d[x * 4 + c] = static_cast<uint8_t>((sum + 2) >> 2);
            }
        }
    }
}

inline void huanfaceUpscaleBilinear(const uint8_t* src, int sw, int sh, uint8_t* dst,
                                    int dw, int dh) {
    const float sx = (sw > 1) ? static_cast<float>(sw - 1) / static_cast<float>(dw - 1)
                              : 0.0f;
    const float sy = (sh > 1) ? static_cast<float>(sh - 1) / static_cast<float>(dh - 1)
                              : 0.0f;
    for (int y = 0; y < dh; ++y) {
        const float fy = y * sy;
        const int y0 = static_cast<int>(fy);
        const int y1 = (y0 + 1 < sh) ? y0 + 1 : y0;
        const float ly = fy - static_cast<float>(y0);
        uint8_t* d = dst + static_cast<size_t>(y) * dw * 4;
        for (int x = 0; x < dw; ++x) {
            const float fx = x * sx;
            const int x0 = static_cast<int>(fx);
            const int x1 = (x0 + 1 < sw) ? x0 + 1 : x0;
            const float lx = fx - static_cast<float>(x0);
            const uint8_t* p00 = src + (static_cast<size_t>(y0) * sw + x0) * 4;
            const uint8_t* p01 = src + (static_cast<size_t>(y0) * sw + x1) * 4;
            const uint8_t* p10 = src + (static_cast<size_t>(y1) * sw + x0) * 4;
            const uint8_t* p11 = src + (static_cast<size_t>(y1) * sw + x1) * 4;
            for (int c = 0; c < 4; ++c) {
                const float v = p00[c] * (1.0f - lx) * (1.0f - ly) +
                                p01[c] * lx * (1.0f - ly) +
                                p10[c] * (1.0f - lx) * ly + p11[c] * lx * ly;
                d[x * 4 + c] = static_cast<uint8_t>(v + 0.5f);
            }
        }
    }
}

} // namespace haocam

