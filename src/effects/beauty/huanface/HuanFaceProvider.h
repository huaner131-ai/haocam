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
#endif
    std::vector<uint8_t> m_readBuffer; // BGRA readback + output scratch (reused)
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
};

} // namespace haocam
