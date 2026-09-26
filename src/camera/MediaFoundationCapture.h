#pragma once

// Windows camera capture through the Media Foundation Source Reader.
//
// Design notes (see docs/CAMERA.md):
//   * Frames stay GPU-resident: the reader is configured with an
//     IMFDXGIDeviceManager so NV12 samples surface as ID3D11Texture2D.
//     Each sample is CopySubresourceRegion'd into a pooled texture with
//     SHADER_RESOURCE binding (decoder outputs are array slices without
//     SRV binding and must not be used directly).
//   * If a device/convert path produces CPU buffers instead, the frame is
//     published with a CPU buffer and uploaded once by the render layer.
//   * Device removal / ReadSample errors transition to Reconnecting; the
//     CameraManager watchdog retries with backoff. HaoCam never crashes
//     when a camera disappears.
//   * All COM objects are created, used and released on the camera thread.

#include <atomic>
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
    std::thread m_thread;
};

} // namespace haocam
