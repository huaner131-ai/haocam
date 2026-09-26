#pragma once

// Synthetic camera source. Produces animated NV12 test-pattern frames from a
// worker thread so the pipeline (and the UI) stays fully functional on
// machines without a camera. It is also the Phase-1 source on non-Windows
// development builds. Frames are CPU-resident; the render layer uploads them
// once and then keeps processing on the GPU.

#include <atomic>
#include <thread>

#include "camera/ICameraSource.h"
#include "frame/TexturePool.h"

namespace haocam {

class NullCameraSource final : public ICameraSource {
public:
    NullCameraSource() = default;
    ~NullCameraSource() override;

    std::vector<CameraDevice> enumerateDevices() override;
    bool start(const std::string& deviceId, const CameraFormatPreference& preference) override;
    void stop() override;
    bool setFormat(const CameraFormatDesc& format) override;

    bool setMirrorPreview(bool mirror) override { m_mirror = mirror; return true; }
    bool setExposure(double) override { return false; }
    bool setWhiteBalance(double) override { return false; }
    bool setFocus(double) override { return false; }
    bool setZoom(double) override { return false; }

    CameraState state() const override { return m_state; }
    CameraFormatDesc activeFormat() const override { return m_activeFormat; }

private:
    void run();

    FrameBufferPool m_buffers;
    std::atomic<CameraState> m_state{CameraState::Idle};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_mirror{false};
    std::atomic<uint32_t> m_fpsRequest{30};
    CameraFormatDesc m_activeFormat;
    std::thread m_thread;
};

} // namespace haocam
