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
                                          onBusStateChanged(event.state, event.stateName,
                                                            event.deviceId, event.detail);
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
    // Mode changes apply on capture restart (the reliable path for UVC
    // devices); the source remembers the requested mode.
    const std::string deviceId = m_selectedDeviceId.toStdString();
    m_engine.cameraManager().stop();
    m_engine.cameraManager().start(deviceId);
    emit activeFormatChanged();
}

QString CameraController::activeFormat() const {
    const auto format = m_engine.cameraManager().activeFormat();
    if (format.resolution.width == 0) return QStringLiteral("not started");
    return QString::fromStdString(describeFormat(format));
}

bool CameraController::mirror() const { return m_mirror; }

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
