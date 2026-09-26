#include "app/App.h"

#include <QQuickWindow>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSGRendererInterface>

#include "core/logging/Logger.h"
#include "core/settings/AppSettings.h"

#include "ui/controllers/BeautyController.h"
#include "ui/controllers/CameraController.h"
#include "ui/controllers/DiagnosticsController.h"
#include "ui/controllers/EngineController.h"
#include "ui/controllers/FilterController.h"

namespace haocam::app {

namespace {
constexpr const char* kCategory = "app";
}

App::~App() { shutdown(); }

bool App::startup() {
    // ---- Settings + logging ----
    auto settings = std::make_unique<core::AppSettings>();
    settings->load();
    const auto configDir = settings->filePath().parent_path();
    const auto logDir = configDir / "logs";
    Logger::instance().initialize(
        logDir, core::logLevelFromString(
                    settings->getString("general", "logLevel", "info")),
        core::LogLevel::Debug);
    HAOCAM_LOG_INFO(kCategory, "HaoCam {} starting", "0.1.0");

    // Runtime configuration (credentials etc. - never logged, never committed).
    const core::AppConfig appConfig = core::AppConfig::load(configDir);

#ifdef Q_OS_WIN
    // HaoCam's Phase-1 pipeline targets Direct3D 11 (the Qt Quick default).
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#endif

    // ---- Core + controllers ----
    m_engineController = std::make_unique<EngineController>();
    m_engineController->setSettings(std::move(settings));
    m_engineController->setAppConfig(appConfig);
    EngineController::setSharedInstance(m_engineController.get());

    m_cameraController = std::make_unique<CameraController>(*m_engineController);
    m_beautyController = std::make_unique<BeautyController>(*m_engineController);
    m_filterController = std::make_unique<FilterController>(*m_engineController);
    m_diagnosticsController = std::make_unique<DiagnosticsController>(*m_engineController);

    // ---- QML ----
    qmlRegisterSingletonInstance("HaoCam.Controllers", 1, 0, "EngineController",
                                 m_engineController.get());
    qmlRegisterSingletonInstance("HaoCam.Controllers", 1, 0, "CameraController",
                                 m_cameraController.get());
    qmlRegisterSingletonInstance("HaoCam.Controllers", 1, 0, "BeautyController",
                                 m_beautyController.get());
    qmlRegisterSingletonInstance("HaoCam.Controllers", 1, 0, "FilterController",
                                 m_filterController.get());
    qmlRegisterSingletonInstance("HaoCam.Controllers", 1, 0, "DiagnosticsController",
                                 m_diagnosticsController.get());

    QObject::connect(
        &m_qmlEngine, &QQmlApplicationEngine::objectCreationFailed, &m_qmlEngine,
        [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);

    m_qmlEngine.loadFromModule("HaoCam", "Main");
    m_qmlLoaded = !m_qmlEngine.rootObjects().isEmpty();
    if (!m_qmlLoaded) {
        HAOCAM_LOG_ERROR(kCategory, "QML main window failed to load");
        return false;
    }

    m_cameraController->refreshDevices();
    HAOCAM_LOG_INFO(kCategory, "Startup complete");
    return true;
}

void App::shutdown() {
    if (m_engineController) {
        m_engineController->shutdownEngine();
    }
    m_cameraController.reset();
    m_filterController.reset();
    m_diagnosticsController.reset();
    m_engineController.reset();
    if (m_qmlLoaded) {
        HAOCAM_LOG_INFO(kCategory, "Shutdown complete");
        core::Logger::instance().shutdown();
    }
}

} // namespace haocam::app
