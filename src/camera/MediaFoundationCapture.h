#pragma once

// Windows camera capture through the Media Foundation Source Reader.
//
// Design notes (see docs/CAMERA.md):
//   * Frames arrive as CPU NV12 samples. Sharing a D3D11 device between
//     Media Foundation and the Qt render thread deadlocked the NVIDIA driver
//     stack (mfplat vs d3d11.dll, stacks 2026-09-28), so MF runs with no
//     D3D11 device; the engine thread uploads each frame to the render
//     device exactly once (GPU-resident from that point on).
//   * The zero-copy device-manager path is kept in code behind
//     kEnableGpuCapture=false for a future keyed-mutex cross-device design.
//   * Device removal / ReadSample errors transition to Reconnecting; the
//     CameraManager watchdog retries with backoff. HaoCam never crashes
//     when a camera disappears.
//   * All COM objects are created, used and released on the camera thread.

#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <thread>

#include "camera/ICameraSource.h"

struct ID3D11Device;

namespace haocam {

class MediaFoundationCapture final : public ICameraSource {
public:
    MediaFoundationCapture() = default;
    ~MediaFoundationCapture() override;

    // Optional: run capture on an existing D3D11 device (void* to keep this
    // header includable in platform-neutral code). Must be called before
    // start(). When unset, a dedicated device with VIDEO_SUPPORT is created.
    void setExternalDevice(void* d3d11Device) { m_externalDevice = d3d11Device; }

    std::vector<CameraDevice> enumerateDevices() override;
    bool start(const std::string& deviceId, const CameraFormatPreference& preference) override;
    void stop() override;
    bool setFormat(const CameraFormatDesc& format) override; // applied on next (re)start
    bool clearFormat() override;

    bool setMirrorPreview(bool mirror) override { m_mirror = mirror; return true; }
    bool setExposure(double value) override;
    bool setWhiteBalance(double kelvin) override;
    bool setFocus(double value) override;
    bool setZoom(double value) override;

    CameraState state() const override { return m_state.load(); }
    CameraFormatDesc activeFormat() const override { return m_activeFormat; }

private:
    void run(const std::string& deviceId, CameraFormatPreference preference);

    std::atomic<CameraState> m_state{CameraState::Idle};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_mirror{false};
    void* m_externalDevice = nullptr;

    CameraFormatDesc m_activeFormat;
    CameraFormatDesc m_requestedFormat;
    std::atomic<bool> m_hasRequestedFormat{false};

    // Modes that failed to stream (device cannot sustain them). Excluded from
    // format selection on every (re)start so reconnects DOWN away from an
    // unsustainable mode instead of re-picking it forever. Reset on device
    // change; guarded because start() (watchdog) reads while the capture
    // thread writes.
    mutable std::mutex m_failedModesMutex;
    std::vector<CameraFormatDesc> m_failedModes;
    std::string m_lastStartedDevice;

    void rememberFailedMode(const CameraFormatDesc& mode);
    std::vector<CameraFormatDesc> usableFormats(
        const std::vector<CameraFormatDesc>& native) const;

    std::thread m_thread;
};

} // namespace haocam
