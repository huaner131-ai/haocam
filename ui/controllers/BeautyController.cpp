#include "ui/controllers/BeautyController.h"

#include <QMetaObject>

#include "core/events/Events.h"
#include "core/logging/Logger.h"

namespace haocam::app {

namespace {
constexpr const char* kCategory = "beauty-ui";
} // namespace

BeautyController::BeautyController(EngineController& engine, QObject* parent)
    : QObject(parent), m_engine(engine) {
    if (const auto* settings = engine.settings()) {
        m_config = beautyConfigFromJson(settings->getObject("beauty"));
    }
    m_config.clamp();

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(600);
    QObject::connect(&m_saveTimer, &QTimer::timeout, this, [this] {
        if (auto* settings = m_engine.settings()) {
            settings->setObject("beauty", beautyConfigToJson(m_config));
            settings->save();
        }
    });

    // Provider status events arrive from provider threads; marshal to GUI.
    m_statusToken = core::EventBus::instance().subscribe<events::ProviderStatusChanged>(
        [this](const events::ProviderStatusChanged& event) {
            if (event.slot != "Beauty") return;
            QMetaObject::invokeMethod(this, [this, event] { refreshStatus(); },
                                      Qt::QueuedConnection);
        });
    refreshStatus();
    applyToEngine();
}

BeautyController::~BeautyController() {
    if (m_statusToken) core::EventBus::instance().unsubscribe(m_statusToken);
}

void BeautyController::applyToEngine() {
    if (auto* beauty = m_engine.effectManager().beauty()) {
        beauty->setSmoothing(m_config.smoothing);
        beauty->setWhitening(m_config.whitening);
        beauty->setRosy(m_config.rosy);
        beauty->setSharpen(m_config.sharpen);
        beauty->setFaceSlim(m_config.faceSlim);
        beauty->setEyeSize(m_config.eyeSize);
        beauty->setNoseSize(m_config.noseSize);
        beauty->setJawSlim(m_config.jawSlim);
    }
}

void BeautyController::refreshStatus() {
    auto* beauty = m_engine.effectManager().beauty();
    m_available = beauty && beauty->isAvailable();
    m_statusDetail = QString::fromStdString(beauty ? beauty->statusText() : "Unavailable");
    emit statusChanged();
}

void BeautyController::setSmoothing(float value) {
    m_config.smoothing = clamp01(value);
    applyToEngine();
    m_saveTimer.start();
    emit smoothingChanged();
}

void BeautyController::setWhitening(float value) {
    m_config.whitening = clamp01(value);
    applyToEngine();
    m_saveTimer.start();
    emit whiteningChanged();
}

void BeautyController::setRosy(float value) {
    m_config.rosy = clamp01(value);
    applyToEngine();
    m_saveTimer.start();
    emit rosyChanged();
}

void BeautyController::setSharpen(float value) {
    m_config.sharpen = clamp01(value);
    applyToEngine();
    m_saveTimer.start();
    emit sharpenChanged();
}

void BeautyController::setFaceSlim(float value) {
    m_config.faceSlim = clamp01(value);
    applyToEngine();
    m_saveTimer.start();
    emit faceSlimChanged();
}

void BeautyController::setEyeSize(float value) {
    m_config.eyeSize = clamp01(value);
    applyToEngine();
    m_saveTimer.start();
    emit eyeSizeChanged();
}

void BeautyController::setNoseSize(float value) {
    m_config.noseSize = clamp01(value);
    applyToEngine();
    m_saveTimer.start();
    emit noseSizeChanged();
}

void BeautyController::setJawSlim(float value) {
    m_config.jawSlim = clamp01(value);
    applyToEngine();
    m_saveTimer.start();
    emit jawSlimChanged();
}

void BeautyController::reset() {
    m_config.reset();
    applyToEngine();
    m_saveTimer.start();
    emit smoothingChanged();
    emit whiteningChanged();
    emit rosyChanged();
    emit sharpenChanged();
    emit faceSlimChanged();
    emit eyeSizeChanged();
    emit noseSizeChanged();
    emit jawSlimChanged();
    HAOCAM_LOG_INFO(kCategory, "Beauty parameters reset from UI");
}

} // namespace haocam::app
