#pragma once

// Small helpers around std::jthread used by the camera/render/web threads.

#include <string>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace haocam::core {

inline void setThreadName(const std::string& name) {
#ifdef _WIN32
    // Windows 10 1607+ supports the exception-free API. The classic
    // MSVC thread-naming trick (RaiseException(0x406D1388)) must NEVER be
    // used unguarded: without an attached debugger the exception surfaces
    // as unhandled and TERMINATES the process (observed: exit code
    // 0x406D1388 on first engine/camera thread naming).
    const std::wstring wname(name.begin(), name.end());
    ::SetThreadDescription(::GetCurrentThread(), wname.c_str());
#else
    (void)name; // pthread_setname_np wiring can be added when running on Linux.
#endif
}

} // namespace haocam::core
