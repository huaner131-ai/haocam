#include "camera/CameraManager.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "core/events/EventBus.h"
#include "core/events/Events.h"
#include "core/logging/Logger.h"
#include "core/threading/NamedThread.h"
#include "frame/FramePool.h"

#ifdef _WIN32
#include "camera/MediaFoundationCapture.h"
#else
#include "camera/NullCameraSource.h"
#endif

namespace haocam {

namespace {
constexpr const char* kCategory = "camera";
constexpr int kReconnectBaseDelayMs = 1000;
constexpr int kReconnectMaxDelayMs = 8000;
} // namespace

CameraManager::CameraManager(core::FrameQueue<Frame>& frameQueue)
    : m_queue(frameQueue), m_frames(std::make_unique<FramePool>()) {}

CameraManager::~CameraManager() { stop(); }

void CameraManager::setExternalCaptureDevice(void* d3d11Device) {
    m_externalCaptureDevice = d3d11Device;
    std::lock_guard<std::mutex> lock(m_mutex);
#ifdef _WIN32
    if (auto* mf = dynamic_cast<MediaFoundationCapture*>(m_source.get())) {
        mf->setExternalDevice(d3d11Device);
    }
#endif
}

void CameraManager::setSource(std::unique_ptr<ICameraSource> source) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_source && m_running.load()) {
        m_source->stop();
    }
    m_source = std::move(source);
}

std::vector<CameraDevice> CameraManager::enumerateDevices() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_source) {
#ifdef _WIN32
        m_source = std::make_unique<MediaFoundationCapture>();
#else
        m_source = std::make_unique<NullCameraSource>();
#endif
    }
    m_devices = m_source->enumerateDevices();
    HAOCAM_LOG_INFO(kCategory, "Enumerated {} camera device(s)", m_devices.size());
    return m_devices;
}

bool CameraManager::start(const std::string& deviceId) {
    if (m_devices.empty()) {
        enumerateDevices();
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_devices.empty()) {
            m_state = CameraState::NoDevice;
            m_lastError = "No camera device found";
            HAOCAM_LOG_WARN(kCategory, "Start requested but no camera device is available");
        }
    }
    if (m_state == CameraState::NoDevice) {
        onStateChanged(m_state, m_lastError);
        return false;
    }

    ICameraSource* source = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_source) {
#ifdef _WIN32
            m_source = std::make_unique<MediaFoundationCapture>();
#else
            m_source = std::make_unique<NullCameraSource>();
#endif
        }
#ifdef _WIN32
        if (m_externalCaptureDevice) {
            if (auto* mf = dynamic_cast<MediaFoundationCapture*>(m_source.get())) {
                mf->setExternalDevice(m_externalCaptureDevice);
            }
        }
#endif
        source = m_source.get();
        m_activeDeviceId = deviceId.empty() ? m_devices.front().id : deviceId;
        CameraSourceCallbacks callbacks;
        callbacks.onFrameReady = [this](Frame&& frame) { onFrame(std::move(frame)); };
        callbacks.onStateChanged = [this](CameraState state, const std::string& detail) {
            onStateChanged(state, detail);
        };
        source->setCallbacks(std::move(callbacks));
    }

    const CameraFormatPreference preference;
    HAOCAM_LOG_INFO(kCategory, "Starting camera '{}' (preferred {}x{} @ {} fps)",
                    m_activeDeviceId, preference.resolution.width,
                    preference.resolution.height, preference.fps);

    m_running.store(true);
    if (!source->start(m_activeDeviceId, preference)) {
        m_running.store(false);
        m_lastError = "Failed to start camera";
        onStateChanged(CameraState::Failed, m_lastError);
        return false;
    }

    // Watchdog: measures capture fps and drives reconnection with backoff.
    if (!m_watchdogThread) {
        m_watchdogThread = std::make_unique<std::thread>([this] {
            core::setThreadName("haocam-watchdog");
            int reconnectDelayMs = kReconnectBaseDelayMs;
            auto lastFrameTime = std::chrono::steady_clock::now();
            uint64_t lastFrameCount = 0;
            while (m_running.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                const uint64_t count = m_framesCaptured.load(std::memory_order_relaxed);
                const auto now = std::chrono::steady_clock::now();
                if (count != lastFrameCount) {
                    const double seconds =
                        std::chrono::duration<double>(now - lastFrameTime).count();
                    if (seconds > 0.0) {
                        m_measuredFps.store(static_cast<double>(count - lastFrameCount) /
                                            seconds);
                    }
                    lastFrameTime = now;
                    lastFrameCount = count;
                    reconnectDelayMs = kReconnectBaseDelayMs;
                    continue;
                }

                bool shouldReconnect = false;
                std::string reconnectDeviceId;
                ICameraSource* reconnectSource = nullptr;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    shouldReconnect = (m_state == CameraState::Reconnecting);
                    reconnectDeviceId = m_activeDeviceId;
                    reconnectSource = m_source.get();
                }
                if (!shouldReconnect || !reconnectSource) continue;

                HAOCAM_LOG_INFO(kCategory, "Reconnection attempt...");
                if (reconnectSource->start(reconnectDeviceId, CameraFormatPreference{})) {
                    HAOCAM_LOG_INFO(kCategory, "Camera reconnected");
                } else {
                    reconnectDelayMs =
                        std::min(reconnectDelayMs * 2, kReconnectMaxDelayMs);
                    HAOCAM_LOG_WARN(kCategory, "Reconnection failed; next retry in {} ms",
                                    reconnectDelayMs);
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(static_cast<int>(reconnectDelayMs)));
                }
            }
        });
    }

    onStateChanged(CameraState::Running, {});
    return true;
}

void CameraManager::stop() {
    m_running.store(false);
    std::unique_ptr<std::thread> watchdog;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_source) m_source->stop();
        watchdog = std::move(m_watchdogThread);
        m_state = CameraState::Idle;
    }
    if (watchdog && watchdog->joinable()) {
        watchdog->join();
    }
    HAOCAM_LOG_INFO(kCategory, "Camera stopped");
}

bool CameraManager::selectFormat(const CameraFormatDesc& format) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_source) return false;
    const bool ok = m_source->setFormat(format);
    if (ok) {
        m_activeFormat = m_source->activeFormat();
        HAOCAM_LOG_INFO(kCategory, "Format set: {}", describeFormat(m_activeFormat));
    } else {
        HAOCAM_LOG_WARN(kCategory, "Format rejected: {}", describeFormat(format));
    }
    return ok;
}

void CameraManager::setMirror(bool mirror) {
    m_mirror.store(mirror, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_source) m_source->setMirrorPreview(mirror);
}

CameraState CameraManager::state() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

CameraFormatDesc CameraManager::activeFormat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeFormat;
}

std::string CameraManager::lastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

double CameraManager::measuredFps() const {
    return m_measuredFps.load(std::memory_order_relaxed);
}

void CameraManager::onFrame(Frame&& frame) {
    m_framesCaptured.fetch_add(1, std::memory_order_relaxed);
    frame.metadata.mirrored = m_mirror.load(std::memory_order_relaxed);
    m_queue.push(std::move(frame));
}

void CameraManager::onStateChanged(CameraState state, const std::string& detail) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = state;
        if (!detail.empty()) m_lastError = detail;
    }

    events::CameraStatus event;
    event.state = static_cast<int>(state);
    event.stateName = toStringCameraState(state);
    event.deviceId = m_activeDeviceId;
    event.detail = detail;
    core::EventBus::instance().publish(event);

    const char* logState = toStringCameraState(state);
    if (!detail.empty()) {
        HAOCAM_LOG_WARN(kCategory, "Camera state {} : {}", logState, detail);
    } else {
        HAOCAM_LOG_INFO(kCategory, "Camera state {}", logState);
    }
}

void CameraManager::scheduleReconnect(const std::string& detail) {
    // The watchdog thread performs actual retries; mark the state here.
    onStateChanged(CameraState::Reconnecting, detail);
}

} // namespace haocam
