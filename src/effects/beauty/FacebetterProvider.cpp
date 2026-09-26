#include "effects/beauty/FacebetterProvider.h"

#include <chrono>
#include <cstring>

#include "core/events/EventBus.h"
#include "core/events/Events.h"
#include "core/logging/Logger.h"
#include "core/threading/NamedThread.h"
#include "graphics/D3D11/GpuFrameCopier.h"

// This TU is compiled Windows-only (CMake-gated); the D3D11 device arrives
// as void* from EffectContext.
#include <windows.h>
#include <d3d11.h>

// Official Facebetter SDK 2.0 headers (drop-in under sdk/facebetter/include).
#include <facebetter/beauty_effect_engine.h>
#include <facebetter/beauty_params.h>
#include <facebetter/image_frame.h>
#include <facebetter/type_defines.h>

namespace haocam {

namespace {
constexpr const char* kCategory = "beauty";

constexpr int kEngineEventLicenseOk = 0;
constexpr int kEngineEventLicenseFailed = 1;
constexpr int kEngineEventInitComplete = 100;
constexpr int kEngineEventInitFailed = 101;

constexpr int kRetryMs = 10000;

void publishBeautyStatus(const std::string& provider, bool available,
                         const std::string& detail) {
    events::ProviderStatusChanged event;
    event.slot = "Beauty";
    event.provider = provider;
    event.available = available;
    event.detail = detail;
    core::EventBus::instance().publish(event);
}
} // namespace

FacebetterProvider::FacebetterProvider() {
    m_config = std::make_shared<const BeautyConfig>();
}

FacebetterProvider::~FacebetterProvider() { shutdown(); }

void FacebetterProvider::configure(const core::FacebetterSettings& settings,
                                   void* d3d11Device, ITexturePool* texturePool) {
    m_settings = settings;
    m_device = static_cast<ID3D11Device*>(d3d11Device);
    m_texturePool = texturePool;
}

bool FacebetterProvider::initialize(const EffectContext& context) {
    (void)context;
    if (m_running.load()) return true;
    if (!m_device) {
        setStatus("Initialization failed: no D3D11 device", false);
        return false;
    }
    if (!m_settings.enabled) {
        setStatus("Disabled in config.json", false);
        return false;
    }
    if (!m_settings.hasCredentials()) {
        setStatus("Unavailable: add app_id/app_key (or license_token) to config.json",
                  false);
        return false;
    }

    m_copier = gfx::GpuFrameCopier::create(m_device);
    if (!m_copier) {
        setStatus("Initialization failed: GPU copier unavailable", false);
        return false;
    }

    m_stopRequested = false;
    m_running = true;
    m_thread = std::thread([this] { run(); });

    // Availability becomes true asynchronously once the engine reports
    // init-complete on the beauty thread (spec section 32: no blocking init
    // on the calling thread).
    return true;
}

void FacebetterProvider::shutdown() {
    if (!m_running.exchange(false)) return;
    m_stopRequested = true;
    {
        std::lock_guard<std::mutex> lock(m_inputMutex);
        m_hasPending = false;
        m_inputCv.notify_all();
    }
    if (m_thread.joinable()) m_thread.join();

    auto* engine = static_cast<facebetter::BeautyEffectEngine*>(m_engine);
    if (engine) {
        engine.reset(); // shared_ptr release on the owning thread
    }
    m_engine = nullptr;
    m_copier.reset();
    {
        std::lock_guard<std::mutex> lock(m_outputMutex);
        m_outputTexture.reset();
        m_outputFrameId = 0;
    }
    setStatus("Shut down", false);
}

bool FacebetterProvider::isAvailable() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_available;
}

std::string FacebetterProvider::statusText() const {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_status;
}

void FacebetterProvider::setStatus(const std::string& status, bool available) {
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        if (m_status == status && m_available == available) return;
        m_status = status;
        m_available = available;
    }
    if (available) {
        HAOCAM_LOG_INFO(kCategory, "Facebetter: {}", status);
    } else {
        HAOCAM_LOG_WARN(kCategory, "Facebetter: {}", status);
    }
    publishBeautyStatus("Facebetter", available, status);
}

std::shared_ptr<const BeautyConfig> FacebetterProvider::snapshotConfig() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

void FacebetterProvider::storeConfig(const BeautyConfig& config) {
    auto next = std::make_shared<const BeautyConfig>(config);
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_config = next;
    }
    m_inputCv.notify_all(); // wake the worker to apply parameters
}

void FacebetterProvider::setSmoothing(float value) {
    BeautyConfig config = *snapshotConfig();
    config.smoothing = clamp01(value);
    storeConfig(config);
}

void FacebetterProvider::setWhitening(float value) {
    BeautyConfig config = *snapshotConfig();
    config.whitening = clamp01(value);
    storeConfig(config);
}

void FacebetterProvider::setRosy(float value) {
    BeautyConfig config = *snapshotConfig();
    config.rosy = clamp01(value);
    storeConfig(config);
}

void FacebetterProvider::setSharpen(float value) {
    BeautyConfig config = *snapshotConfig();
    config.sharpen = clamp01(value);
    storeConfig(config);
}

void FacebetterProvider::setFaceSlim(float value) {
    BeautyConfig config = *snapshotConfig();
    config.faceSlim = clamp01(value);
    storeConfig(config);
}

void FacebetterProvider::setEyeSize(float value) {
    BeautyConfig config = *snapshotConfig();
    config.eyeSize = clamp01(value);
    storeConfig(config);
}

void FacebetterProvider::setNoseSize(float value) {
    BeautyConfig config = *snapshotConfig();
    config.noseSize = clamp01(value);
    storeConfig(config);
}

void FacebetterProvider::setJawSlim(float value) {
    BeautyConfig config = *snapshotConfig();
    config.jawSlim = clamp01(value);
    storeConfig(config);
}

void FacebetterProvider::reset() { storeConfig(BeautyConfig{}); }

BeautyConfig FacebetterProvider::config() const { return *snapshotConfig(); }

void FacebetterProvider::submitFrame(const GpuTextureRef& texture, uint64_t frameId) {
    {
        std::lock_guard<std::mutex> lock(m_inputMutex);
        m_pendingTexture = texture;
        m_pendingFrameId = frameId;
        m_hasPending = texture != nullptr;
        m_inputCv.notify_one();
    }
}

bool FacebetterProvider::latestOutput(GpuTextureRef& outTexture,
                                      uint64_t& outFrameId) const {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    if (!m_outputTexture) return false;
    outTexture = m_outputTexture;
    outFrameId = m_outputFrameId;
    return true;
}

bool FacebetterProvider::createEngineLocked() {
    // ---- Engine creation on the beauty thread (SDK-managed GL context,
    // external_context=false per official docs) ----
    facebetter::LogConfig logConfig;
    logConfig.console_enabled = false;
    logConfig.file_enabled = false;
    logConfig.level = facebetter::LogLevel::Info;
    facebetter::BeautyEffectEngine::SetLogConfig(logConfig);

    facebetter::EngineConfig engineConfig;
    engineConfig.app_id = m_settings.appId;
    engineConfig.app_key = m_settings.appKey;
    if (!m_settings.licenseToken.empty()) {
        engineConfig.license_token = m_settings.licenseToken;
    }
    engineConfig.resource_path = m_settings.resourcePath;
    engineConfig.external_context = false;

    std::shared_ptr<facebetter::BeautyEffectEngine> engine =
        facebetter::BeautyEffectEngine::Create(engineConfig);
    if (!engine) {
        setStatus("Initialization failed (check credentials and resource path)", false);
        return false;
    }

    facebetter::EngineCallbacks callbacks;
    callbacks.on_engine_event = [this](int code, const std::string& message) {
        switch (code) {
            case kEngineEventLicenseOk:
                HAOCAM_LOG_INFO(kCategory, "Facebetter: license OK");
                break;
            case kEngineEventLicenseFailed:
                setStatus("Invalid credentials (license rejected)", false);
                break;
            case kEngineEventInitComplete:
                setStatus("Ready", true);
                break;
            case kEngineEventInitFailed:
                setStatus(std::string("Initialization failed: ") + message, false);
                break;
            default:
                HAOCAM_LOG_DEBUG(kCategory, "Facebetter engine event {}: {}", code, message);
                break;
        }
    };
    callbacks.on_face_landmarks = [this](const std::vector<facebetter::FaceDetectionResult>&
                                             faces) {
        m_faceCount.store(static_cast<int>(faces.size()), std::memory_order_relaxed);
    };
    engine->SetCallbacks(callbacks);

    m_engine = new std::shared_ptr<facebetter::BeautyEffectEngine>(std::move(engine));
    m_engineConfig = BeautyConfig{}; // force a full parameter apply next frame
    HAOCAM_LOG_INFO(kCategory, "Facebetter engine created (beauty thread)");
    return true;
}

void FacebetterProvider::applyConfigToEngine(const BeautyConfig& config) {
    auto* engine =
        *static_cast<std::shared_ptr<facebetter::BeautyEffectEngine>*>(m_engine);
    if (!engine) return;

    // Skin parameters ([0,1] -> [0,1], documented API).
    engine->SetSmoothing(config.smoothing);
    engine->SetWhitening(config.whitening);
    engine->SetRosiness(config.rosy);
    engine->SetSharpening(config.sharpen);

    // Face reshape (application [0,1] mapped into the SDK's [-1,1] reshape
    // range; 0 = neutral in both).
    using facebetter::beauty_params::Reshape;
    engine->SetReshape(Reshape::FaceThin, config.faceSlim);
    engine->SetReshape(Reshape::EyeSize, config.eyeSize);
    engine->SetReshape(Reshape::NoseSlim, config.noseSize);
    engine->SetReshape(Reshape::Jawbone, config.jawSlim);

    m_engineConfig = config;
}

void FacebetterProvider::run() {
    core::setThreadName("haocam-beauty");
    auto lastRetry = std::chrono::steady_clock::now() - std::chrono::milliseconds(kRetryMs);
    BeautyConfig lastSubmittedConfig = *snapshotConfig();

    while (!m_stopRequested.load()) {
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
            // No frame this tick: still apply parameter changes so the UI
            // feels immediate on the next frame.
            const auto config = snapshotConfig();
            if (*config != m_engineConfig && m_engine) {
                applyConfigToEngine(*config);
            }
            continue;
        }

        if (!m_engine) continue;

        // ---- Readback (GPU -> CPU, staging ring) ----
        if (!m_copier->readBGRA(input, m_readBuffer)) {
            continue;
        }
        const uint32_t width = input->width();
        const uint32_t height = input->height();

        // ---- BGRA -> RGBA (SDK input format, reusable buffer) ----
        m_rgbaBuffer.resize(m_readBuffer.size());
        {
            const uint8_t* src = m_readBuffer.data();
            uint8_t* dst = m_rgbaBuffer.data();
            const size_t pixels = m_readBuffer.size() / 4;
            for (size_t i = 0; i < pixels; ++i) {
                dst[i * 4 + 0] = src[i * 4 + 2];
                dst[i * 4 + 1] = src[i * 4 + 1];
                dst[i * 4 + 2] = src[i * 4 + 0];
                dst[i * 4 + 3] = src[i * 4 + 3];
            }
        }

        // ---- Apply config diff, then process (official API) ----
        const auto config = snapshotConfig();
        if (*config != m_engineConfig) {
            applyConfigToEngine(*config);
        }

        auto frame = facebetter::ImageFrame::CreateWithRGBA(
            m_rgbaBuffer.data(), static_cast<int>(width), static_cast<int>(height),
            static_cast<int>(width) * 4);
        if (!frame) {
            setStatus("Initialization failed: ImageFrame creation rejected", false);
            continue;
        }
        frame->type = facebetter::FrameType::Video;

        auto* engine =
            *static_cast<std::shared_ptr<facebetter::BeautyEffectEngine>*>(m_engine);
        const auto processStart = std::chrono::steady_clock::now();
        auto output = engine->ProcessImage(frame);
        const auto processEnd = std::chrono::steady_clock::now();
        m_sdkProcessMs.store(
            std::chrono::duration<double, std::milli>(processEnd - processStart).count(),
            std::memory_order_relaxed);

        if (!output || !output->Data()) {
            HAOCAM_LOG_DEBUG(kCategory, "Facebetter returned no output this frame");
            continue;
        }

        // ---- Output RGBA -> BGRA -> GPU upload (pooled texture) ----
        const int outWidth = output->Width();
        const int outHeight = output->Height();
        if (outWidth <= 0 || outHeight <= 0 ||
            static_cast<uint32_t>(outWidth) != width ||
            static_cast<uint32_t>(outHeight) != height) {
            HAOCAM_LOG_WARN(kCategory, "Facebetter output size mismatch ({}x{} vs {}x{})",
                            outWidth, outHeight, width, height);
            continue;
        }

        {
            uint8_t* dst = m_readBuffer.data(); // reuse the read buffer as BGRA scratch
            const uint8_t* src = output->Data();
            const int srcStride = output->Stride();
            const size_t rowBytes = static_cast<size_t>(outWidth) * 4;
            for (int row = 0; row < outHeight; ++row) {
                const uint8_t* srcLine = src + static_cast<size_t>(row) * srcStride;
                uint8_t* dstLine = dst + static_cast<size_t>(row) * rowBytes;
                for (size_t px = 0; px < static_cast<size_t>(outWidth); ++px) {
                    dstLine[px * 4 + 0] = srcLine[px * 4 + 2];
                    dstLine[px * 4 + 1] = srcLine[px * 4 + 1];
                    dstLine[px * 4 + 2] = srcLine[px * 4 + 0];
                    dstLine[px * 4 + 3] = srcLine[px * 4 + 3];
                }
            }
        }

        if (!m_texturePool) continue;
        GpuTextureRef uploaded =
            m_copier->uploadBGRA(*m_texturePool, m_readBuffer.data(), width, height);
        if (!uploaded) continue;

        {
            std::lock_guard<std::mutex> lock(m_outputMutex);
            m_outputTexture = std::move(uploaded);
            m_outputFrameId = inputFrameId;
        }
        lastSubmittedConfig = *config;
        m_processedFrames.fetch_add(1, std::memory_order_relaxed);
        (void)lastSubmittedConfig;
    }
}

BeautyResult FacebetterProvider::process(const Frame& input, const FaceData& face) {
    (void)face;
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

std::unique_ptr<IBeautyProvider> createFacebetterProviderOrDefault(
    std::unique_ptr<IBeautyProvider> nullFallback) {
    (void)nullFallback;
    return std::make_unique<FacebetterProvider>();
}

} // namespace haocam
