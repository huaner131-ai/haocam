#pragma once

// QML controller for camera enumeration, selection, format and mirror.

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "core/events/EventBus.h"

namespace haocam::app {

class EngineController;

class CameraController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString selectedDeviceId READ selectedDeviceId WRITE selectDevice NOTIFY
                   selectedDeviceChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString stateDetail READ stateDetail NOTIFY stateChanged)
    Q_PROPERTY(QString activeFormat READ activeFormat NOTIFY activeFormatChanged)
    Q_PROPERTY(bool mirror READ mirror WRITE setMirror NOTIFY mirrorChanged)

public:
    explicit CameraController(EngineController& engine, QObject* parent = nullptr);

    QVariantList devices() const { return m_devices; }
    QString selectedDeviceId() const { return m_selectedDeviceId; }
    QString state() const { return m_state; }
    QString stateDetail() const { return m_stateDetail; }
    QString activeFormat() const;
    bool mirror() const { return m_mirror; }

    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void selectDevice(const QString& deviceId);
    Q_INVOKABLE void applyResolution(int width, int height, int fps);
    Q_INVOKABLE void setMirror(bool mirror);

signals:
    void devicesChanged();
    void selectedDeviceChanged();
    void stateChanged();
    void activeFormatChanged();
    void mirrorChanged();

private:
    void onBusStateChanged(int state, QString stateName, QString deviceId, QString detail);

    EngineController& m_engine;
    core::EventBus::Token m_busToken = 0;
    QVariantList m_devices;
    QString m_selectedDeviceId;
    QString m_state = "Idle";
    QString m_stateDetail;
    bool m_mirror = true;
    bool m_hasActiveFormatRequest = false;
    int m_requestedWidth = 1920;
    int m_requestedHeight = 1080;
    int m_requestedFps = 60;
};

} // namespace haocam::app
