#include "effects/beauty/huanface/HuanFaceProvider.h"

#include <chrono>
#include <cstring>

#include "core/events/EventBus.h"
#include "core/events/Events.h"
#include "core/logging/Logger.h"
#include "core/threading/NamedThread.h"
#include "effects/EffectProvider.h"
#include "effects/EffectContext.h"
#if defined(_WIN32)
#include "graphics/D3D11/GpuFrameCopier.h"
#include "graphics/D3D11/D3D11Texture.h" // D3D11TexturePool (own-pool fallback)
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#endif

#include <huanface_c_api.h>

#include <limits>

namespace haocam {

namespace {
constexpr const char* kCategory = "beauty";

void publishBeautyStatus(const std::string& provider, bool available,
                         const std::string& detail) {
    events::ProviderStatusChanged event;
    event.slot = "Beauty";
    event.provider = provider;
    event.available = available;
    event.detail = detail;
    core::EventBus::instance().publish(event);
}
constexpr int kRetryMs = 4000; // engine (re)create retry window
constexpr int kTelemetryMs = 5000; // periodic one-line pipeline stats
// The SDK's CPU reference path costs >1 s/frame at 2560x1440; capping its
// input width keeps the whole path ~4x cheaper (2x2 box downscale, bilinear
// upscale back before upload).
constexpr uint32_t kMaxSdkWidth = 1280;

// Parameter names per HFBeautyParameters::ToFloatMap (SDK beauty_params.h).
constexpr const char* kPEnabled = "beauty.enabled";
constexpr const char* kPGlobal = "beauty.globalIntensity";
constexpr const char* kPSmoothing = "beauty.smoothing.intensity";
constexpr const char* kPSmoothingEnabled = "beauty.smoothing.enabled";
constexpr const char* kPTexture = "beauty.texture.intensity";
constexpr const char* kPTextureEnabled = "beauty.texture.enabled";
constexpr const char* kPToneEnabled = "beauty.tone.enabled";
constexpr const char* kPToneIntensity = "beauty.tone.intensity";
constexpr const char* kPToneTint = "beauty.tone.tint";
constexpr const char* kPBrightnessEnabled = "beauty.brightness.enabled";
constexpr const char* kPBrightness = "beauty.brightness.intensity";
constexpr const char* kPRetouch = "beauty.retouch.intensity";
} // namespace

HuanFaceProvider::HuanFaceProvider() {
    storeConfig(BeautyConfig{});
}

HuanFaceProvider::~HuanFaceProvider() { shutdown(); }

void HuanFaceProvider::configure(void* d3d11Device, ITexturePool* texturePool) {
    m_device = static_cast<ID3D11Device*>(d3d11Device);
    m_texturePool = texturePool;
}

bool HuanFaceProvider::initialize(const EffectContext& context) {
    if (m_running.exchange(true)) return true;
    m_stopRequested = false;
    if (context.device) m_device = context.device;
    if (context.texturePool) m_texturePool = context.texturePool;
#if defined(_WIN32)
    if (m_device) {
        m_copier = gfx::GpuFrameCopier::create(m_device);
    }
    if (!m_copier) {
        setStatus("GPU copier unavailable (no D3D11 device)", false);
        // Keep the worker alive: it will report a precise status and retry.
    }
    if (m_device && !m_texturePool) {
        // EffectContext::texturePool is currently never set - without our own
        // pool every processed frame was silently dropped at upload time.
        m_ownPool = gfx::D3D11TexturePool::create(m_device);
        m_texturePool = m_ownPool.get();
    }
#else
    setStatus("CPU/CI build: engine lifecycle only (no GPU copies)", false);
#endif
    m_thread = std::thread([this] { run(); });
    return true;
}

void HuanFaceProvider::shutdown() {
    if (!m_running.exchange(false)) {
        if (m_thread.joinable()) {
            m_stopRequested = true;
            m_thread.join();
        }
        return;
    }
    m_stopRequested = true;
    {
        std::lock_guard<std::mutex> lock(m_inputMutex);
        m_hasPending = false;
        m_inputCv.notify_all();
    }
    if (m_thread.joinable()) m_thread.join();

    if (m_engine) {
        HF_DestroyEngine(static_cast<HFEngine>(m_engine));
        m_engine = nullptr;
    }
#if defined(_WIN32)
    m_copier.reset();
    m_ownPool.reset();
#endif
}

bool HuanFaceProvider::isAvailable() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_available;
}

std::string HuanFaceProvider::statusText() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_status;
}

void HuanFaceProvider::setStatus(const std::string& status, bool available) {
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        if (m_status == status && m_available == available) return;
        m_status = status;
        m_available = available;
    }
    if (available) {
        HAOCAM_LOG_INFO(kCategory, "HuanFace: {}", status);
    } else {
        HAOCAM_LOG_WARN(kCategory, "HuanFace: {}", status);
    }
    publishBeautyStatus("HuanFace", available, status);
}

std::shared_ptr<const BeautyConfig> HuanFaceProvider::snapshotConfig() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

void HuanFaceProvider::storeConfig(const BeautyConfig& config) {
    auto next = std::make_shared<const BeautyConfig>(config);
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_config = next;
    }
    m_inputCv.notify_all();
}

void HuanFaceProvider::setSmoothing(float value) {
    BeautyConfig config = *snapshotConfig();
    config.smoothing = clamp01(value);
    storeConfig(config);
}

void HuanFaceProvider::setWhitening(float value) {
    BeautyConfig config = *snapshotConfig();
    config.whitening = clamp01(value);
    storeConfig(config);
}

void HuanFaceProvider::setRosy(float value) {
    BeautyConfig config = *snapshotConfig();
    config.rosy = clamp01(value);
    storeConfig(config);
}

void HuanFaceProvider::setSharpen(float value) {
    BeautyConfig config = *snapshotConfig();
    config.sharpen = clamp01(value);
    storeConfig(config);
}

void HuanFaceProvider::setFaceSlim(float) { /* unsupported by the SDK yet */ }
void HuanFaceProvider::setEyeSize(float) { /* unsupported by the SDK yet */ }
void HuanFaceProvider::setNoseSize(float) { /* unsupported by the SDK yet */ }
void HuanFaceProvider::setJawSlim(float) { /* unsupported by the SDK yet */ }

void HuanFaceProvider::reset() { storeConfig(BeautyConfig{}); }

BeautyConfig HuanFaceProvider::config() const { return *snapshotConfig(); }

uint64_t HuanFaceProvider::maxLagFrames() const {
    // Half the uint64 range = never stale (hold-latest); avoids overflow in
    // EffectManager's `beautyFrameId + maxLag >= frame.id` check.
    return std::numeric_limits<uint64_t>::max() / 2;
}

void HuanFaceProvider::submitFrame(const GpuTextureRef& texture, uint64_t frameId) {
    {
        std::lock_guard<std::mutex> lock(m_inputMutex);
        m_pendingTexture = texture;
        m_pendingFrameId = frameId;
        m_hasPending = texture != nullptr;
        if (texture) m_submittedFrames.fetch_add(1, std::memory_order_relaxed);
        m_inputCv.notify_one();
    }
}

bool HuanFaceProvider::latestOutput(GpuTextureRef& outTexture,
                                    uint64_t& outFrameId) const {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    if (!m_outputTexture) return false;
    outTexture = m_outputTexture;
    outFrameId = m_outputFrameId;
    return true;
}

bool HuanFaceProvider::createEngineLocked() {
    // HF_Init is idempotent (mutex + atomic flag inside the SDK).
    if (HF_Init() != HF_RESULT_OK) {
        setStatus("HF_Init failed", false);
        return false;
    }

    HFEngineConfigC config{};
    config.backendType = HF_RENDER_BACKEND_AUTO; // null backend on CPU-only runs
    config.width = 1920;
    config.height = 1080;
    config.enableDebug = 0;
    config.faceTrackerType = "auto";
    config.maxFaces = 4;
    config.detectSmallFace = 0;
    config.minFaceRatio = 0.02f;
    config.faceLandmarkQuality = 1;
    config.faceDetectMode = 1;
    config.useAsyncAIInference = 0;
    config.enableFaceMeshV2 = 1;

    HFEngine engine = nullptr;
    const HFResult rc = HF_CreateEngine(&config, &engine);
    if (rc != HF_RESULT_OK || !engine) {
        setStatus(std::string("engine create failed: ") + HF_GetResultString(rc),
                  false);
        return false;
    }
    m_engine = engine;
    m_engineConfig = BeautyConfig{}; // force a full parameter apply next frame
    HAOCAM_LOG_INFO(kCategory, "HuanFace engine created (beauty thread)");
    return true;
}

void HuanFaceProvider::applyConfigToEngine(const BeautyConfig& config) {
    auto* engine = static_cast<HFEngine>(m_engine);
    if (!engine) return;

    // Normalized [0,1] application values -> SDK "beauty.*" params.
    // Mapping (documented; SDK has no dedicated whitening/rosy/sharpen):
    //   smoothing -> beauty.smoothing.intensity
    //   whitening -> beauty.brightness.intensity (skin-only lift, [0,0.6])
    //   rosy      -> beauty.tone.tint (+magenta) + tone.intensity, warm push
    //   sharpen   -> beauty.texture.intensity (detail refinement)
    const bool anyActive = config.smoothing > 0.001f || config.whitening > 0.001f ||
                           config.rosy > 0.001f || config.sharpen > 0.001f;
    HF_SetParameterFloat(engine, kPEnabled, anyActive ? 1.0f : 0.0f);
    HF_SetParameterFloat(engine, kPGlobal, 1.0f);

    HF_SetParameterFloat(engine, kPSmoothingEnabled, config.smoothing > 0.001f ? 1.0f : 0.0f);
    HF_SetParameterFloat(engine, kPSmoothing, config.smoothing);

    HF_SetParameterFloat(engine, kPTextureEnabled, config.sharpen > 0.001f ? 1.0f : 0.0f);
    HF_SetParameterFloat(engine, kPTexture, config.sharpen);

    HF_SetParameterFloat(engine, kPBrightnessEnabled, config.whitening > 0.001f ? 1.0f : 0.0f);
    HF_SetParameterFloat(engine, kPBrightness, config.whitening * 0.6f);

    HF_SetParameterFloat(engine, kPToneEnabled, config.rosy > 0.001f ? 1.0f : 0.0f);
    HF_SetParameterFloat(engine, kPToneIntensity, config.rosy * 0.5f);
    HF_SetParameterFloat(engine, kPToneTint, config.rosy * 0.4f);

    // The C-API pipeline gates the beauty pass on the retouch intensity.
    HF_SetParameterFloat(engine, kPRetouch, anyActive ? 1.0f : 0.0f);

    m_engineConfig = config;
}

BeautyResult HuanFaceProvider::process(const Frame& input, const FaceData& face) {
    (void)face; // the SDK tracks internally (reported via detectedFaceCount)
    BeautyResult result;
    if (!isAvailable()) {
        result.error = statusText();
        return result;
    }
    if (!input.texture.valid()) {
        result.error = "no GPU frame";
        return result;
    }
    // Interface-conformance path: enqueue and return the latest completed
    // output (the engine drives submit/latestOutput for freshness control).
    submitFrame(input.texture.ref, input.id);
    GpuTextureRef texture;
    uint64_t frameId = 0;
    if (latestOutput(texture, frameId)) {
        result.success = true;
        result.texture = texture;
        (void)frameId;
    } else {
        result.error = "beauty output pending (engine warming up)";
    }
    return result;
}

void HuanFaceProvider::run() {
    core::setThreadName("haocam-beauty");
    auto lastRetry = std::chrono::steady_clock::now() - std::chrono::milliseconds(kRetryMs);
    auto lastTelemetry = std::chrono::steady_clock::now();
    auto lastFailureWarn = lastTelemetry;

    while (!m_stopRequested.load()) {
        // ---- Periodic one-line stats (makes silent pass-through visible) ----
        const auto nowTelem = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(nowTelem - lastTelemetry)
                .count() >= kTelemetryMs) {
            lastTelemetry = nowTelem;
            const auto cfg = snapshotConfig();
            const bool gate =
                cfg->smoothing > 0.0f || cfg->whitening > 0.0f || cfg->rosy > 0.0f ||
                cfg->sharpen > 0.0f;
            // NOTE: the project logger supports only "{}" placeholders (no
            // printf-style precision) - "{:.2f}" would print literally and
            // shift every following argument.
            HAOCAM_LOG_INFO(
                kCategory,
                "stats: submitted={} processed={} discarded={} faces={} gate={} "
                "smoothing={} whitening={} rosy={} sharpen={} "
                "failures={} readbackMs={} sdkMs={} uploadMs={}",
                m_submittedFrames.load(std::memory_order_relaxed),
                m_processedFrames.load(std::memory_order_relaxed),
                m_discardedFrames.load(std::memory_order_relaxed),
                m_faceCount.load(std::memory_order_relaxed),
                gate ? "ON" : "off",
                cfg->smoothing, cfg->whitening, cfg->rosy, cfg->sharpen,
                m_processFailures.load(std::memory_order_relaxed),
                static_cast<int>(m_readbackMs.load(std::memory_order_relaxed)),
                static_cast<int>(m_sdkProcessMs.load(std::memory_order_relaxed)),
                static_cast<int>(m_uploadMs.load(std::memory_order_relaxed)));
        }

        // ---- Engine lifecycle (created/retried on this thread) ----
        if (!m_engine) {
            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRetry)
                    .count() >= kRetryMs) {
                lastRetry = now;
                setStatus("SDK found - initializing engine...", false);
                if (!createEngineLocked()) {
                    continue; // wait for the next retry window
                }
                setStatus("Ready (CPU beauty path; SDK-internal face tracking)", true);
            }
        }

        // ---- Wait for input or config change ----
        GpuTextureRef input;
        uint64_t inputFrameId = 0;
        {
            std::unique_lock<std::mutex> lock(m_inputMutex);
            m_inputCv.wait_for(lock, std::chrono::milliseconds(100), [&] {
                return m_hasPending || m_stopRequested.load();
            });
            if (m_stopRequested.load()) break;
            if (m_hasPending) {
                input = std::move(m_pendingTexture);
                inputFrameId = m_pendingFrameId;
                m_pendingTexture.reset();
                m_hasPending = false;
            }
        }
        if (!input) {
            const auto config = snapshotConfig();
            if (*config != m_engineConfig && m_engine) {
                applyConfigToEngine(*config);
            }
            continue;
        }

#if defined(_WIN32)
        if (!m_engine || !m_copier) continue;
#else
        if (!m_engine) continue; // Linux/CI: engine lifecycle only (null GPU)
#endif

#if defined(_WIN32)
        // ---- Readback (GPU -> CPU, staging ring) ----
        const auto readStart = std::chrono::steady_clock::now();
        if (!m_copier->readBGRA(input, m_readBuffer)) {
            continue;
        }
        const uint32_t width = input->width();
        const uint32_t height = input->height();
        m_readbackMs.store(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                      readStart)
                .count(),
            std::memory_order_relaxed);

        // ---- Cap SDK resolution, apply config diff, process (public C API) ----
        uint32_t procW = width;
        uint32_t procH = height;
        const uint8_t* procData = m_readBuffer.data();
        if (width > kMaxSdkWidth) {
            procW = width / 2;
            procH = height / 2;
            m_smallBuffer.resize(static_cast<size_t>(procW) * procH * 4);
            huanfaceDownscale2x(m_readBuffer.data(), static_cast<int>(width),
                                static_cast<int>(height), m_smallBuffer.data());
            procData = m_smallBuffer.data();
        }

        const auto config = snapshotConfig();
        if (*config != m_engineConfig) {
            applyConfigToEngine(*config);
        }

        HFFrameC inFrame{};
        inFrame.width = static_cast<int>(procW);
        inFrame.height = static_cast<int>(procH);
        inFrame.format = HF_FORMAT_BGRA8; // the SDK converts internally
        inFrame.timestampNanos = static_cast<int64_t>(inputFrameId) * 33000000LL;
        inFrame.data = const_cast<uint8_t*>(procData);
        inFrame.stride = static_cast<int>(procW) * 4;
        inFrame.ownsData = 0;
        inFrame.ownsGpuTexture = 0;

        HFFrameC outFrame{};
        const auto processStart = std::chrono::steady_clock::now();
        const HFResult rc = HF_ProcessFrame(static_cast<HFEngine>(m_engine), &inFrame,
                                            &outFrame);
        const auto processEnd = std::chrono::steady_clock::now();
        m_sdkProcessMs.store(
            std::chrono::duration<double, std::milli>(processEnd - processStart).count(),
            std::memory_order_relaxed);
        if (rc != HF_RESULT_OK) {
            m_processFailures.fetch_add(1, std::memory_order_relaxed);
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - lastFailureWarn)
                    .count() >= kTelemetryMs) {
                lastFailureWarn = std::chrono::steady_clock::now();
                HAOCAM_LOG_WARN(kCategory, "HuanFace ProcessFrame failed ({}x so far): {}",
                                m_processFailures.load(std::memory_order_relaxed),
                                HF_GetResultString(rc));
            }
            if (outFrame.data) HF_FreeFrame(&outFrame);
            continue;
        }

        // ---- Face count for diagnostics (best effort) ----
        HFTrackingDataC tracking{};
        if (HF_GetFaceData(static_cast<HFEngine>(m_engine), &tracking) == HF_RESULT_OK) {
            m_faceCount.store(tracking.faceCount, std::memory_order_relaxed);
            HF_FreeFaceData(&tracking);
        }

        // ---- Output BGRA -> GPU upload (pooled texture) ----
        if (outFrame.width != static_cast<int>(procW) ||
            outFrame.height != static_cast<int>(procH) || !outFrame.data) {
            // Counted (not silent): a metadata-only OK frame with no data is
            // exactly what an UNPATCHED HuanFace C-API returns on the
            // with-face path - this counter is how we see it from the log.
            m_discardedFrames.fetch_add(1, std::memory_order_relaxed);
            HAOCAM_LOG_DEBUG(kCategory, "HuanFace output size/format mismatch");
            HF_FreeFrame(&outFrame);
            continue;
        }

        const uint8_t* uploadData = outFrame.data;
        if (procW != width || procH != height) {
            m_scaleBuffer.resize(static_cast<size_t>(width) * height * 4);
            huanfaceUpscaleBilinear(outFrame.data, static_cast<int>(procW),
                                    static_cast<int>(procH), m_scaleBuffer.data(),
                                    static_cast<int>(width), static_cast<int>(height));
            uploadData = m_scaleBuffer.data();
        }

        GpuTextureRef uploaded;
        if (m_texturePool) {
            const auto uploadStart = std::chrono::steady_clock::now();
            uploaded = m_copier->uploadBGRA(*m_texturePool, uploadData, width, height);
            m_uploadMs.store(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                          uploadStart)
                    .count(),
                std::memory_order_relaxed);
        }
        HF_FreeFrame(&outFrame);
        if (!uploaded) continue;

        {
            std::lock_guard<std::mutex> lock(m_outputMutex);
            m_outputTexture = std::move(uploaded);
            m_outputFrameId = inputFrameId;
        }
        m_processedFrames.fetch_add(1, std::memory_order_relaxed);
#endif // _WIN32
    }
}

} // namespace haocam
