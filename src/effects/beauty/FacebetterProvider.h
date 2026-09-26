#pragma once

// Facebetter beauty engine adapter (Phase 2, spec sections 15-22).
//
// COMPILED ONLY when HAOCAM_ENABLE_FACEBETTER=ON and the official SDK
// drop-in exists under sdk/facebetter/ (include/ + lib/ + resource/).
// Written strictly against the official SDK 2.0 API and documentation
// (docs.facebetter.net/windows/implement-beauty + the official demo):
//
//   #include <facebetter/beauty_effect_engine.h>
//   EngineConfig{app_id, app_key, license_token, resource_path,
//                external_context=false}
//   BeautyEffectEngine::Create(cfg) -> shared_ptr (nullptr = failure)
//   engine->SetCallbacks(EngineCallbacks{on_engine_event, on_face_landmarks})
//   engine->SetSmoothing/SetWhitening/SetRosiness/SetSharpening  [0,1]
//   engine->SetReshape(Reshape::FaceThin|EyeSize|NoseSlim|Jawbone, v) [-1,1]
//   ImageFrame::CreateWithRGBA(data, w, h, stride); frame->type =
//   FrameType::Video; engine->ProcessImage(frame) -> output ImageFrame
//
// Threading: the engine (SDK-managed GL context) is created and used ONLY on
// the beauty worker thread owned by this provider. The GPU readback/upload
// runs through gfx::GpuFrameCopier on the same thread (shared D3D11 immediate
// context is multithread-protected - docs/GPU_PIPELINE.md).
//
// Data path (measured, documented in docs/BEAUTY.md):
//   D3D11 processed BGRA -> staging readback -> BGRA->RGBA -> ProcessImage
//   -> RGBA->BGRA -> pooled D3D11 texture (update) -> composite override

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/config/AppConfig.h"
#include "effects/beauty/BeautyProvider.h"

namespace haocam::gfx {
class GpuFrameCopier;
}

struct ID3D11Device;

namespace haocam {
class ITexturePool; // frame/TexturePool.h

class FacebetterProvider final : public IBeautyProvider, public IBeautyAsync {
public:
    FacebetterProvider();
    ~FacebetterProvider() override;

    // `settings` carries user-provided credentials from config.json (never
    // logged). `d3d11Device` is the pipeline device (raw ID3D11Device*).
    void configure(const core::FacebetterSettings& settings, void* d3d11Device,
                   ITexturePool* texturePool);

    // IEffectProvider
    bool initialize(const EffectContext& context) override;
    void shutdown() override;
    bool isAvailable() const override;
    std::string statusText() const override;
    const char* name() const override { return "Facebetter"; }
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
    Features features() const override { return Features{}; } // all supported per SDK docs

    // Asynchronous processing path (spec section 21 - the SDK processes
    // frames on its own GL thread; HaoCam mirrors that with this worker).
    // process(const Frame&, const FaceData&) delegates here for interface
    // conformance; the engine drives submit/latestOutput directly.
    void submitFrame(const GpuTextureRef& texture, uint64_t frameId);
    bool latestOutput(GpuTextureRef& outTexture, uint64_t& outFrameId) const;

    // Diagnostics (spec sections 22/34).
    double lastReadbackMs() const { return m_readbackMs.load(); }
    double lastSdkProcessMs() const { return m_sdkProcessMs.load(); }
    double lastUploadMs() const { return m_uploadMs.load(); }
    int detectedFaceCount() const { return m_faceCount.load(); }

private:
    void run();
    bool createEngineLocked(); // called on the beauty thread only
    void applyConfigToEngine(const BeautyConfig& config);
    void setStatus(const std::string& status, bool available);
    std::shared_ptr<const BeautyConfig> snapshotConfig() const;
    void storeConfig(const BeautyConfig& config);

    // --- configuration (set before initialize) ---
    core::FacebetterSettings m_settings;
    ID3D11Device* m_device = nullptr; // raw; owned by Qt Quick's RHI
    ITexturePool* m_texturePool = nullptr;

    // --- parameter snapshot (UI thread writes, beauty thread reads) ---
    mutable std::mutex m_configMutex;
    std::shared_ptr<const BeautyConfig> m_config;

    // --- worker state (beauty thread only) ---
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
    void* m_engine = nullptr; // facebetter::BeautyEffectEngine* (type-erased:
                              // the SDK header is only visible in the .cpp)
    std::unique_ptr<gfx::GpuFrameCopier> m_copier;
    std::vector<uint8_t> m_readBuffer;   // BGRA readback (reused)
    std::vector<uint8_t> m_rgbaBuffer;   // RGBA SDK input (reused)
    BeautyConfig m_engineConfig;         // last applied to the engine

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

// Creates the provider when the SDK is compiled in; returns the null provider
// otherwise (EffectManager uses this; defined in FacebetterProvider.cpp /
// BeautyProvider factory translation unit).
std::unique_ptr<IBeautyProvider> createFacebetterProviderOrDefault(
    std::unique_ptr<IBeautyProvider> nullFallback);

} // namespace haocam
