#pragma once

// Application bootstrap: logging, settings, controllers, QML engine.
// Owns the lifetime of the core engine objects (camera manager, effect
// pipeline) and exposes the controllers to QML as singletons.

#include <memory>

#include <QQmlApplicationEngine>

namespace haocam::app {

class BeautyController;
class CameraController;
class DiagnosticsController;
class EngineController;
class FilterController;

class App {
public:
    App() = default;
    ~App();

    bool startup();
    void shutdown();

private:
    std::unique_ptr<EngineController> m_engineController;
    std::unique_ptr<CameraController> m_cameraController;
    std::unique_ptr<FilterController> m_filterController;
    std::unique_ptr<DiagnosticsController> m_diagnosticsController;
    QQmlApplicationEngine m_qmlEngine;
    bool m_qmlLoaded = false;
};

} // namespace haocam::app
