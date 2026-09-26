#include <algorithm>

#include "ui/controllers/FilterController.h"

#include "core/logging/Logger.h"
#include "ui/controllers/EngineController.h"

namespace haocam::app {

namespace {
constexpr const char* kCategory = "filters";

float normalize(int value) { return static_cast<float>(value) / 100.0f; }
} // namespace

FilterController::FilterController(EngineController& engine, QObject* parent)
    : QObject(parent), m_engine(engine) {
    if (const auto* settings = engine.settings()) {
        m_brightness = settings->getInt("filters", "brightness", 0);
        m_contrast = settings->getInt("filters", "contrast", 0);
        m_saturation = settings->getInt("filters", "saturation", 0);
    }
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(800);
    m_saveTimer.connect(&m_saveTimer, &QTimer::timeout, this, [this] {
        if (auto* settings = m_engine.settings()) {
            settings->setInt("filters", "brightness", m_brightness);
            settings->setInt("filters", "contrast", m_contrast);
            settings->setInt("filters", "saturation", m_saturation);
            settings->save();
        }
    });
    applyToEngine();
}

void FilterController::applyToEngine() {
    if (auto* compositor = m_engine.compositor()) {
        compositor->adjustments().brightness = normalize(m_brightness);
        compositor->adjustments().contrast = normalize(m_contrast);
        compositor->adjustments().saturation = normalize(m_saturation);
    }
}

void FilterController::setBrightness(int value) {
    if (value == m_brightness) return;
    m_brightness = std::clamp(value, -100, 100);
    applyToEngine();
    m_saveTimer.start();
    emit brightnessChanged();
}

void FilterController::setContrast(int value) {
    if (value == m_contrast) return;
    m_contrast = std::clamp(value, -100, 100);
    applyToEngine();
    m_saveTimer.start();
    emit contrastChanged();
}

void FilterController::setSaturation(int value) {
    if (value == m_saturation) return;
    m_saturation = std::clamp(value, -100, 100);
    applyToEngine();
    m_saveTimer.start();
    emit saturationChanged();
}

void FilterController::reset() {
    m_brightness = 0;
    m_contrast = 0;
    m_saturation = 0;
    applyToEngine();
    m_saveTimer.start();
    emit brightnessChanged();
    emit contrastChanged();
    emit saturationChanged();
    HAOCAM_LOG_INFO(kCategory, "Color filters reset");
}

} // namespace haocam::app
