#include "camera/NullCameraSource.h"

#include <chrono>
#include <cmath>
#include <cstring>

#include "core/logging/Logger.h"
#include "core/threading/NamedThread.h"
#include "frame/FrameBuffer.h"

namespace haocam {

namespace {
constexpr const char* kCategory = "camera";
constexpr const char* kNullDeviceId = "haocam://null-camera";

// Fills an NV12 test pattern: moving gradient + color bars + center marker.
void renderTestPatternNV12(FrameBuffer& buffer, uint64_t frameIndex) {
    const uint32_t width = buffer.width;
    const uint32_t height = buffer.height;
    const uint32_t strideY = buffer.strideY;
    const uint32_t strideUV = buffer.strideUV;

    uint8_t* y = buffer.yPlane();
    uint8_t* uv = buffer.uvPlane();

    // Y plane: horizontal gradient with a moving vertical band.
    const uint32_t band = static_cast<uint32_t>(frameIndex * 4) % (width + 120);
    for (uint32_t row = 0; row < height; ++row) {
        uint8_t* line = y + static_cast<size_t>(row) * strideY;
        for (uint32_t col = 0; col < width; ++col) {
            uint8_t value = static_cast<uint8_t>(16 + (col * 200) / (width ? width : 1));
            if (col >= band && col < band + 60) {
                value = static_cast<uint8_t>(235 - (value - 16) / 4);
            }
            line[col] = value;
        }
    }

    // UV plane: slowly cycling hue.
    const double huePhase = std::fmod(frameIndex / 90.0, 1.0) * 2.0 * 3.14159265358979;
    const int u = static_cast<int>(128.0 + 100.0 * std::cos(huePhase));
    const int v = static_cast<int>(128.0 + 100.0 * std::sin(huePhase));
    const uint32_t uvRows = height / 2;
    for (uint32_t row = 0; row < uvRows; ++row) {
        uint8_t* line = uv + static_cast<size_t>(row) * strideUV;
        for (uint32_t colPair = 0; colPair < strideUV; colPair += 2) {
            line[colPair] = static_cast<uint8_t>(u);
            line[colPair + 1] = static_cast<uint8_t>(v);
        }
    }

    // Center white rectangle (sanity marker for mirroring).
    const uint32_t cx0 = width / 2 - width / 40;
    const uint32_t cx1 = width / 2 + width / 40;
    const uint32_t cy0 = height / 2 - height / 40;
    const uint32_t cy1 = height / 2 + height / 40;
    for (uint32_t row = cy0; row < cy1 && row < height; ++row) {
        std::memset(y + static_cast<size_t>(row) * strideY + cx0, 235, cx1 - cx0);
    }
}
} // namespace

NullCameraSource::~NullCameraSource() { stop(); }

std::vector<CameraDevice> NullCameraSource::enumerateDevices() {
    CameraDevice device;
    device.id = kNullDeviceId;
    device.displayName = "Null Camera (test pattern)";

    for (auto [w, h] : {std::pair{1920u, 1080u}, {1280u, 720u}, {640u, 480u}}) {
        device.formats.push_back({{w, h}, 30, 1, "NV12"});
    }
    return {device};
}

bool NullCameraSource::start(const std::string& deviceId, const CameraFormatPreference&) {
    if (deviceId != kNullDeviceId) {
        if (m_callbacks.onStateChanged) {
            m_callbacks.onStateChanged(CameraState::Failed, "Unknown device for NullCameraSource");
        }
        return false;
    }
    stop();

    m_activeFormat = {{1280, 720}, 30, 1, "NV12"};
    m_state = CameraState::Starting;
    m_stopRequested = false;
    m_thread = std::thread([this] { run(); });
    return true;
}

void NullCameraSource::stop() {
    m_stopRequested = true;
    if (m_thread.joinable()) m_thread.join();
    m_state = CameraState::Idle;
}

bool NullCameraSource::setFormat(const CameraFormatDesc& format) {
    m_fpsRequest.store(static_cast<uint32_t>(format.fps() + 0.5));
    m_activeFormat = format;
    return true;
}

void NullCameraSource::run() {
    core::setThreadName("haocam-nullcam");
    if (m_callbacks.onStateChanged) m_callbacks.onStateChanged(CameraState::Running, {});
    m_state = CameraState::Running;

    uint64_t frameIndex = 0;
    const auto start = std::chrono::steady_clock::now();
    while (!m_stopRequested.load()) {
        const uint32_t fps = m_fpsRequest.load();
        const auto frameDuration = std::chrono::microseconds(1000000 / (fps ? fps : 30));
        const auto next = start + frameDuration * (frameIndex + 1);

        auto buffer = m_buffers.acquireNV12(m_activeFormat.resolution.width,
                                            m_activeFormat.resolution.height);
        renderTestPatternNV12(*buffer, frameIndex);

        Frame frame;
        frame.id = frameIndex + 1;
        frame.timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        frame.width = buffer->width;
        frame.height = buffer->height;
        frame.format = PixelFormat::NV12;
        frame.cpuBuffer = buffer;
        frame.metadata.gpuResident = false;
        frame.metadata.frameId = frame.id;
        frame.metadata.timestampUs = frame.timestamp;
        frame.metadata.sourceFps = fps;
        frame.metadata.strideY = buffer->strideY;
        frame.metadata.strideUV = buffer->strideUV;
        frame.metadata.sourceName = "Null Camera";

        if (m_callbacks.onFrameReady) m_callbacks.onFrameReady(std::move(frame));
        ++frameIndex;

        std::this_thread::sleep_until(next);
    }
}

} // namespace haocam
