#pragma once

// QML controller for the Phase-1 native color filter
// (pipeline stage 8: Color / LUT). Range is [-100, 100] mapped to [-1, 1].

#include <QObject>
#include <QTimer>

#include "graphics/compositor/Compositor.h"

namespace haocam::app {

class EngineController;

class FilterController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(int contrast READ contrast WRITE setContrast NOTIFY contrastChanged)
    Q_PROPERTY(int saturation READ saturation WRITE setSaturation NOTIFY saturationChanged)

public:
    explicit FilterController(EngineController& engine, QObject* parent = nullptr);

    int brightness() const { return m_brightness; }
    int contrast() const { return m_contrast; }
    int saturation() const { return m_saturation; }

    Q_INVOKABLE void reset();

public slots:
    void setBrightness(int value);
    void setContrast(int value);
    void setSaturation(int value);

signals:
    void brightnessChanged();
    void contrastChanged();
    void saturationChanged();

private:
    void applyToEngine();

    EngineController& m_engine;
    int m_brightness = 0;
    int m_contrast = 0;
    int m_saturation = 0;
    QTimer m_saveTimer;
};

} // namespace haocam::app
