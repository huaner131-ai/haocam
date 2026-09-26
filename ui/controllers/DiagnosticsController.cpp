#include "ui/controllers/DiagnosticsController.h"

#include "core/logging/Logger.h"
#include "ui/controllers/EngineController.h"

#ifdef Q_OS_WIN
#include "graphics/compositor/Compositor.h"
#endif

namespace haocam::app {

DiagnosticsController::DiagnosticsController(EngineController& engine, QObject* parent)
    : QObject(parent), m_engine(engine) {
    if (const auto* settings = engine.settings()) {
        m_overlayVisible = settings->getBool("diagnostics", "overlayVisible", false);
    }
    QObject::connect(&m_timer, &QTimer::timeout, this, [this] { refresh(); });
    m_timer.start(500);
}

void DiagnosticsController::setOverlayVisible(bool visible) {
    if (visible == m_overlayVisible) return;
    m_overlayVisible = visible;
    if (auto* settings = m_engine.settings()) {
        settings->setBool("diagnostics", "overlayVisible", visible);
        settings->save();
    }
    emit overlayVisibleChanged();
}

void DiagnosticsController::refresh() {
    auto& camera = m_engine.cameraManager();
    m_cameraFps = camera.measuredFps();
    m_droppedFrames = m_engine.queueDroppedFrames();

#ifdef Q_OS_WIN
    if (auto* compositor = m_engine.compositor(); compositor && compositor->valid()) {
        m_previewFps = compositor->processFps();
        m_frameTimeMs = compositor->lastCpuTimeMs();
        m_gpuTimeMs = compositor->lastGpuTimeMs();
        m_cpuTimeMs = compositor->lastCpuTimeMs();
        m_pooledTextures = static_cast<int>(compositor->pooledTextures());
    }
#endif

    const auto format = camera.activeFormat();
    if (format.resolution.width > 0) {
        m_cameraResolution = QString("%1x%2 @ %3")
                                 .arg(format.resolution.width)
                                 .arg(format.resolution.height)
                                 .arg(static_cast<int>(format.fps() + 0.5));
    }

    m_activeEffects = m_engine.activeStages();

    emit statsChanged();
}

} // namespace haocam::app
