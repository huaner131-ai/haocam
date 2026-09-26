// HaoCam entry point.

#include <QGuiApplication>

#include "app/App.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Prefer the dedicated GPU on hybrid-graphics laptops.
    SetEnvironmentVariableW(L"SHIM_MCCOMPAT", L"0x800000001");
#endif

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("HaoCam");
    QGuiApplication::setOrganizationName("HaoCam");
    QGuiApplication::setApplicationVersion("0.1.0");

    haocam::app::App haocamApp;
    if (!haocamApp.startup()) {
        return 1;
    }

    const int exitCode = QGuiApplication::exec();
    haocamApp.shutdown();
    return exitCode;
}
