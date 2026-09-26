#pragma once

// High-level camera orchestration: enumeration, selection, automatic
// reconnection with backoff, frame fan-out to the engine, and state
// broadcasting. UI talks to CameraManager (via CameraController), never to
// Media Foundation directly.

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "camera/CameraDevice.h"
#include "camera/CameraFormat.h"
#include "camera/ICameraSource.h"
#include "core/threading/FrameQueue.h"

namespace haocam {

class FramePool;

class CameraManager {
public:
    explicit CameraManager(core::FrameQueue<Frame>& frameQueue);
    ~CameraManager();

    // Uses the platform camera source (Media Foundation on Windows,
    // NullCamera elsewhere or when explicitly requested).
    void setSource(std::unique_ptr<ICameraSource> source);

    // Runs capture on an existing D3D11 device (raw ID3D11Device*). Must be
    // called before start(); forwarded to MediaFoundationCapture on Windows.
    void setExternalCaptureDevice(void* d3d11Device);

    std::vector<CameraDevice> enumerateDevices();

    // Starts the given device; empty id picks the best format automatically.
    bool start(const std::string& deviceId = {});
    void stop();

    bool selectFormat(const CameraFormatDesc& format);
    void setMirror(bool mirror);

    CameraState state() const;
    CameraFormatDesc activeFormat() const;
    std::string lastError() const;

    // Statistics (camera-side).
    uint64_t framesCaptured() const { return m_framesCaptured.load(std::memory_order_relaxed); }
    double measuredFps() const;

private:
    void onFrame(Frame&& frame);
    void onStateChanged(CameraState state, const std::string& detail);
    void scheduleReconnect(const std::string& detail);

    core::FrameQueue<Frame>& m_queue;
    std::unique_ptr<ICameraSource> m_source;
    std::unique_ptr<FramePool> m_frames;

    mutable std::mutex m_mutex;
    std::vector<CameraDevice> m_devices;
    CameraFormatDesc m_activeFormat;
    std::string m_activeDeviceId;
    std::string m_lastError;
    CameraState m_state = CameraState::Idle;
    void* m_externalCaptureDevice = nullptr;

    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_framesCaptured{0};
    std::atomic<double> m_measuredFps{0.0};
    std::atomic<bool> m_mirror{false};
    std::unique_ptr<std::thread> m_watchdogThread;
};

} // namespace haocam
