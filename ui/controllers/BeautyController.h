#pragma once

// QML controller for the Beauty tab (Phase 2, spec sections 25-27).
//
// Slider values are application-normalized 0.0..1.0 (spec section 18); the
// provider converts to SDK semantics. Setters delegate to the provider
// (thread-safe config snapshots) and persist to settings (debounced).

#include <QTimer>

#include "core/events/EventBus.h"
#include "effects/beauty/BeautyProvider.h"
#include "ui/controllers/EngineController.h"

namespace haocam::app {

class BeautyController : public QObject {
    Q_OBJECT
    Q_PROPERTY(float smoothing READ smoothing WRITE setSmoothing NOTIFY smoothingChanged)
    Q_PROPERTY(float whitening READ whitening WRITE setWhitening NOTIFY whiteningChanged)
    Q_PROPERTY(float rosy READ rosy WRITE setRosy NOTIFY rosyChanged)
    Q_PROPERTY(float sharpen READ sharpen WRITE setSharpen NOTIFY sharpenChanged)
    Q_PROPERTY(float faceSlim READ faceSlim WRITE setFaceSlim NOTIFY faceSlimChanged)
    Q_PROPERTY(float eyeSize READ eyeSize WRITE setEyeSize NOTIFY eyeSizeChanged)
    Q_PROPERTY(float noseSize READ noseSize WRITE setNoseSize NOTIFY noseSizeChanged)
    Q_PROPERTY(float jawSlim READ jawSlim WRITE setJawSlim NOTIFY jawSlimChanged)

    // Provider status (spec section 27): available + human-readable detail.
    Q_PROPERTY(bool available READ available NOTIFY statusChanged)
    Q_PROPERTY(QString statusDetail READ statusDetail NOTIFY statusChanged)

public:
    explicit BeautyController(EngineController& engine, QObject* parent = nullptr);
    ~BeautyController() override;

    float smoothing() const { return m_config.smoothing; }
    float whitening() const { return m_config.whitening; }
    float rosy() const { return m_config.rosy; }
    float sharpen() const { return m_config.sharpen; }
    float faceSlim() const { return m_config.faceSlim; }
    float eyeSize() const { return m_config.eyeSize; }
    float noseSize() const { return m_config.noseSize; }
    float jawSlim() const { return m_config.jawSlim; }

    bool available() const { return m_available; }
    QString statusDetail() const { return m_statusDetail; }

    Q_INVOKABLE void reset();

public slots:
    void setSmoothing(float value);
    void setWhitening(float value);
    void setRosy(float value);
    void setSharpen(float value);
    void setFaceSlim(float value);
    void setEyeSize(float value);
    void setNoseSize(float value);
    void setJawSlim(float value);

signals:
    void smoothingChanged();
    void whiteningChanged();
    void rosyChanged();
    void sharpenChanged();
    void faceSlimChanged();
    void eyeSizeChanged();
    void noseSizeChanged();
    void jawSlimChanged();
    void statusChanged();

private:
    void applyToEngine();
    void refreshStatus();

    EngineController& m_engine;
    BeautyConfig m_config;
    QTimer m_saveTimer;
    core::EventBus::Token m_statusToken = 0;
    bool m_available = false;
    QString m_statusDetail;
};

} // namespace haocam::app
