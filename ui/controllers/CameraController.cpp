#include "ui/controllers/CameraController.h"

#include <QMetaObject>

#include "core/events/EventBus.h"
#include "core/events/Events.h"
#include "core/logging/Logger.h"
#include "ui/controllers/EngineController.h"

namespace haocam::app {

namespace {
constexpr const char* kCategory = "camera-ui";

QString stateNameFromCameraState(CameraState state) {
    return QString::fromLatin1(toStringCameraState(state));
}
} // namespace

CameraController::CameraController(EngineController& engine, QObject* parent)
    : QObject(parent), m_engine(engine) {
    // Camera status events arrive on camera/watchdog threads; marshal into
    // the GUI thread.
    m_busToken = core::EventBus::instance().subscribe<events::CameraStatus>(
        [this](const events::CameraStatus& event) {
            QMetaObject::invokeMethod(this,
                                      [this, event] {
                                          onBusStateChanged(event.state,
                                                            QString::fromStdString(event.stateName),
                                                            QString::fromStdString(event.deviceId),
                                                            QString::fromStdString(event.detail));
                                      },
                                      Qt::QueuedConnection);
        });
}

CameraController::~CameraController() {
    if (m_busToken) {
        core::EventBus::instance().unsubscribe(m_busToken);
    }
}

void CameraController::refreshDevices() {
    m_devices.clear();
    for (const auto& device : m_engine.cameraManager().enumerateDevices()) {
        QVariantMap entry;
        entry.insert("id", QString::fromStdString(device.id));
        entry.insert("name", QString::fromStdString(device.displayName));
        m_devices.append(entry);
    }
    if (m_engine.settings()) {
        const auto lastId =
            m_engine.settings()->getString("camera", "deviceId", "");
        if (!lastId.empty()) m_selectedDeviceId = QString::fromStdString(lastId);
    }
    emit devicesChanged();
}

void CameraController::selectDevice(const QString& deviceId) {
    if (deviceId == m_selectedDeviceId && !deviceId.isEmpty()) return;
    m_selectedDeviceId = deviceId;
    if (m_engine.settings()) {
        m_engine.settings()->setString("camera", "deviceId", deviceId.toStdString());
        m_engine.settings()->save();
    }
    // Restarting the source is the reliable way to switch devices.
    m_engine.cameraManager().stop();
    if (!deviceId.isEmpty()) {
        m_engine.cameraManager().start(deviceId.toStdString());
    } else {
        m_engine.cameraManager().start();
    }
    emit selectedDeviceChanged();
}

void CameraController::applyResolution(int width, int height, int fps) {
    const std::string deviceId = m_selectedDeviceId.toStdString();

    // "Auto": back to the device's native best mode.
    if (width == 0 || height == 0) {
        m_hasActiveFormatRequest = false;
        m_requestedWidth = 0;
        m_requestedHeight = 0;
        m_requestedFps = 0;
        if (!m_engine.cameraManager().clearFormat()) {
            HAOCAM_LOG_WARN(kCategory, "Camera rejected the auto-format request");
            return;
        }
        m_engine.cameraManager().stop();
        m_engine.cameraManager().start(deviceId);
        emit activeFormatChanged();
        return;
    }

    m_hasActiveFormatRequest = true;
    m_requestedWidth = width;
    m_requestedHeight = height;
    m_requestedFps = fps;
    CameraFormatDesc format;
    format.resolution = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    format.fpsNumerator = static_cast<uint32_t>(fps > 0 ? fps : 30);
    format.fpsDenominator = 1;
    format.pixelFormat = "NV12";
    if (!m_engine.cameraManager().selectFormat(format)) {
        HAOCAM_LOG_WARN(kCategory, "Camera rejected format request");
        return;
    }
    // FLICKER FIX: re-selecting the ALREADY ACTIVE mode used to restart the
    // capture for nothing (~0.5s of flicker). Skip the restart when the
    // device already streams exactly this mode.
    const CameraFormatDesc active = m_engine.cameraManager().activeFormat();
    if (active.resolution == format.resolution && active.fps() == format.fps()) {
        HAOCAM_LOG_INFO(kCategory, "Resolution {}x{}@{} already active - no restart",
                        width, height, fps);
        emit activeFormatChanged();
        return;
    }
    // Mode changes apply on capture restart (the reliable path for UVC
    // devices); the source remembers the requested mode.
    m_engine.cameraManager().stop();
    m_engine.cameraManager().start(deviceId);
    emit activeFormatChanged();
}

QString CameraController::activeFormat() const {
    const auto format = m_engine.cameraManager().activeFormat();
    if (format.resolution.width == 0) return QStringLiteral("not started");
    return QString::fromStdString(describeFormat(format));
}

void CameraController::setMirror(bool mirror) {
    if (mirror == m_mirror) return;
    m_mirror = mirror;
    m_engine.cameraManager().setMirror(mirror);
    if (m_engine.settings()) {
        m_engine.settings()->setBool("camera", "mirror", mirror);
        m_engine.settings()->save();
    }
    emit mirrorChanged();
}

void CameraController::onBusStateChanged(int state, QString stateName, QString deviceId,
                                          QString detail) {
    (void)state;
    (void)deviceId;
    m_state = stateName;
    m_stateDetail = detail;
    if (m_selectedDeviceId.isEmpty()) {
        refreshDevices();
    }
    emit stateChanged();
    emit activeFormatChanged();
}

} // namespace haocam::app
