#include "face/TrackingWorker.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "face/FaceLandmarks.h"

#include "core/logging/Logger.h"
#include "core/threading/NamedThread.h"

namespace haocam {

namespace {
constexpr const char* kCategory = "tracking";

// NV12 (BT.709, limited range) -> RGBA8, CPU fallback path.
void nv12ToRGBA(const FrameBuffer& buffer, std::vector<uint8_t>& dst) {
    const uint32_t width = buffer.width;
    const uint32_t height = buffer.height;
    dst.resize(static_cast<size_t>(width) * height * 4);

    const uint8_t* yPlane = buffer.yPlane();
    const uint8_t* uvPlane = buffer.uvPlane();
    for (uint32_t row = 0; row < height; ++row) {
        const uint8_t* yLine = yPlane + static_cast<size_t>(row) * buffer.strideY;
        const uint8_t* uvLine =
            uvPlane + static_cast<size_t>(row / 2) * buffer.strideUV;
        uint8_t* out = dst.data() + static_cast<size_t>(row) * width * 4;
        for (uint32_t col = 0; col < width; ++col) {
            const float y = (yLine[col] - 16.0f) * (255.0f / 219.0f);
            const float u = (uvLine[(col & ~1u)] - 128.0f) * (255.0f / 224.0f);
            const float v = (uvLine[(col & ~1u) + 1] - 128.0f) * (255.0f / 224.0f);
            float r = y + 1.5748f * v;
            float g = y - 0.1873f * u - 0.4681f * v;
            float b = y + 1.8556f * u;
            out[col * 4 + 0] = static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f));
            out[col * 4 + 1] = static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f));
            out[col * 4 + 2] = static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f));
            out[col * 4 + 3] = 255;
        }
    }
}
} // namespace

bool CpuNv12PixelSource::readRGBA(const TrackingInput& input, std::vector<uint8_t>& dst,
                                  uint32_t& width, uint32_t& height) {
    if (!input.cpu || input.cpu->data.empty() || input.width == 0 || input.height == 0) {
        return false;
    }
    nv12ToRGBA(*input.cpu, dst);
    width = input.width;
    height = input.height;
    return true;
}

TrackingWorker::TrackingWorker(std::unique_ptr<IFaceTracker> tracker,
                               std::unique_ptr<IPixelSource> pixelSource)
    : m_tracker(std::move(tracker)), m_pixelSource(std::move(pixelSource)) {
    m_pending = TrackingInput{};
}

TrackingWorker::~TrackingWorker() { stop(); }

bool TrackingWorker::start(const FaceTrackerConfig& config) {
    if (m_running.load()) return true;
    m_maxFps.store(config.maxTrackingFps > 0 ? config.maxTrackingFps : 30.0f);
    m_smoother.configure(config.smoothingMinCutoff, config.smoothingBeta);

    if (!m_tracker->initialize(config)) {
        HAOCAM_LOG_WARN(kCategory, "Tracker '{}' unavailable: {}", m_tracker->name(),
                        m_tracker->statusText());
        return false;
    }
    if (!m_tracker->isAvailable()) {
        HAOCAM_LOG_WARN(kCategory, "Tracker '{}' not available", m_tracker->name());
        return false;
    }

    // Reserve the scratch buffer once for the expected working size; it
    // grows at most a couple of times per session (resolution changes).
    m_rgbaScratch.reserve(640 * 360 * 4);

    m_stopRequested = false;
    m_running = true;
    m_thread = std::thread([this] { run(); });
    HAOCAM_LOG_INFO(kCategory,
                    "Tracking thread started (tracker={}, source={}, maxFps={})",
                    m_tracker->name(), m_pixelSource->name(), m_maxFps.load());
    return true;
}

void TrackingWorker::stop() {
    if (!m_running.exchange(false)) {
        if (m_tracker) m_tracker->shutdown();
        return;
    }
    m_stopRequested = true;
    {
        std::lock_guard<std::mutex> lock(m_inputMutex);
        m_hasPending = false;
        m_inputCv.notify_all();
    }
    if (m_thread.joinable()) m_thread.join();
    m_tracker->shutdown();
    HAOCAM_LOG_INFO(kCategory, "Tracking thread stopped");
}

void TrackingWorker::submit(TrackingInput&& input) {
    uint64_t dropped = 0;
    {
        std::lock_guard<std::mutex> lock(m_inputMutex);
        if (m_hasPending) ++dropped;
        m_pending = std::move(input);
        m_hasPending = m_pending.gpu != nullptr || m_pending.cpu != nullptr;
        m_inputCv.notify_one();
    }
    if (dropped > 0) {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.droppedFrames += dropped;
    }
}

FaceTrackingResult TrackingWorker::latestTracking() const {
    std::lock_guard<std::mutex> lock(m_resultMutex);
    return m_latest;
}

std::string TrackingWorker::trackerStatus() const {
    return m_tracker ? m_tracker->statusText() : std::string("Unavailable");
}

TrackingStats TrackingWorker::stats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void TrackingWorker::setSmoothing(float minCutoff, float beta) {
    m_smoother.configure(minCutoff, beta);
}

void TrackingWorker::setMaxFps(float fps) {
    m_maxFps.store(fps > 0 ? fps : 30.0f);
}

void TrackingWorker::applySmoothing(FaceTrackingResult& result, double dtSeconds) {
    if (!result.detected) {
        m_smoother.reset();
        return;
    }
    for (FaceData& face : result.faces) {
        m_smoother.apply(face.landmarks, dtSeconds);
        face.updateBoundsFromLandmarks();
        face.headPose = haocam::face::estimateHeadPose(face);
    }
}

void TrackingWorker::run() {
    core::setThreadName("haocam-tracking");
    uint64_t lastFrameId = 0;
    auto lastProcessed = std::chrono::steady_clock::now();
    auto lastFpsStamp = lastProcessed;
    uint64_t fpsCounter = 0;

    while (!m_stopRequested.load()) {
        TrackingInput input;
        {
            std::unique_lock<std::mutex> lock(m_inputMutex);
            m_inputCv.wait_for(lock, std::chrono::milliseconds(50),
                               [&] { return m_hasPending || m_stopRequested.load(); });
            if (m_stopRequested.load()) break;
            if (!m_hasPending) continue;
            input = std::move(m_pending);
            m_hasPending = false;
            m_pending = TrackingInput{};
        }

        // Drop-stale: if this frame is older than the last processed one,
        // skip it entirely (should not happen with latest-slot semantics).
        if (input.frameId != 0 && input.frameId <= lastFrameId) continue;
        lastFrameId = input.frameId;

        // FPS throttle.
        const float maxFps = m_maxFps.load();
        const auto now = std::chrono::steady_clock::now();
        const auto minInterval = std::chrono::duration<double>(1.0 / maxFps);
        const auto sinceLast = std::chrono::duration<double>(now - lastProcessed);
        if (sinceLast < minInterval) {
            const auto sleepFor = std::chrono::duration_cast<std::chrono::milliseconds>(
                minInterval - sinceLast);
            std::this_thread::sleep_for(sleepFor);
        }

        // ---- Pixels ----
        uint32_t width = 0, height = 0;
        const auto readStart = std::chrono::steady_clock::now();
        if (!m_pixelSource->readRGBA(input, m_rgbaScratch, width, height)) {
            continue;
        }

        // ---- Track (backend-specific; never throws past this interface) ----
        const auto processStart = std::chrono::steady_clock::now();
        FaceTrackingResult result;
        if (auto* pixelSink = dynamic_cast<IPixelSink*>(m_tracker.get())) {
            TrackingFrameView view;
            view.rgba = m_rgbaScratch.data();
            view.width = width;
            view.height = height;
            view.strideBytes = width * 4;
            view.timestampUs = input.timestampUs;
            result = pixelSink->processRGBA(view);
        } else {
            result = m_tracker->process(Frame{});
        }
        const auto processEnd = std::chrono::steady_clock::now();
        lastProcessed = processEnd;

        // ---- Smooth + publish ----
        const double dt = std::chrono::duration<double>(processStart - m_lastResultTime).count();
        m_lastResultTime = processStart;
        applySmoothing(result, dt > 0 ? dt : 1.0 / 30.0);

        {
            std::lock_guard<std::mutex> lock(m_resultMutex);
            m_latest = std::move(result);
        }

        // ---- Stats ----
        const double processMs =
            std::chrono::duration<double, std::milli>(processEnd - processStart).count();
        const double readMs =
            std::chrono::duration<double, std::milli>(processStart - readStart).count();
        fpsCounter++;
        const double fpsSpan = std::chrono::duration<double>(processEnd - lastFpsStamp).count();
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.lastProcessMs = processMs + readMs;
            if (fpsSpan >= 0.5) {
                m_stats.fps = fpsCounter / fpsSpan;
                fpsCounter = 0;
                lastFpsStamp = processEnd;
            }
            const FaceData* primary = m_latest.primary();
            m_stats.confidence = primary ? primary->confidence : 0.0f;
            m_stats.faceCount = m_latest.detected ? static_cast<int>(m_latest.faces.size()) : 0;
            m_stats.processedFrames++;
        }
        (void)height;
    }
}

} // namespace haocam
