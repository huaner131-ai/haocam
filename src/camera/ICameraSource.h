#pragma once

// Platform camera source abstraction. The Windows implementation uses Media
// Foundation (MediaFoundationCapture); NullCameraSource provides synthetic
// frames when no camera exists or during development on other platforms.

#include <functional>

#include "camera/CameraDevice.h"
#include "camera/CameraFormat.h"
#include "frame/Frame.h"

namespace haocam {

enum class CameraState {
    Idle,
    Starting,
    Running,
    Reconnecting, // device lost, automatic retry in progress
    NoDevice,
    Failed,
};

const char* toStringCameraState(CameraState state);

// Callbacks invoked from the camera thread. Implementations must be cheap
// and never block (GPU work happens on the render thread).
struct CameraSourceCallbacks {
    std::function<void(Frame&&)> onFrameReady;
    std::function<void(CameraState, const std::string&)> onStateChanged;
};

class ICameraSource {
public:
    virtual ~ICameraSource() = default;

    // Enumerates available capture devices on this system.
    virtual std::vector<CameraDevice> enumerateDevices() = 0;

    // Opens `deviceId` and configures the best matching format.
    virtual bool start(const std::string& deviceId, const CameraFormatPreference& preference) = 0;
    virtual void stop() = 0;

    // Applies a different mode without reopening the device when possible.
    virtual bool setFormat(const CameraFormatDesc& format) = 0;

    // Optional hardware controls; report false when unsupported.
    virtual bool setMirrorPreview(bool mirror) = 0;
    virtual bool setExposure(double value) = 0;       // log2 seconds, manual
    virtual bool setWhiteBalance(double kelvin) = 0;
    virtual bool setFocus(double value) = 0;
    virtual bool setZoom(double value) = 0;

    virtual CameraState state() const = 0;
    virtual CameraFormatDesc activeFormat() const = 0;

    void setCallbacks(CameraSourceCallbacks callbacks) { m_callbacks = std::move(callbacks); }

protected:
    CameraSourceCallbacks m_callbacks;
};

} // namespace haocam
