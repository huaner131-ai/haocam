#pragma once

// Small helpers around std::jthread used by the camera/render/web threads.

#include <string>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace haocam::core {

inline void setThreadName(const std::string& name) {
#ifdef _WIN32
#pragma pack(push, 8)
    struct THREADNAME_INFO {
        DWORD dwType;
        LPCSTR szName;
        DWORD dwThreadID;
        DWORD dwFlags;
    };
#pragma pack(pop)
    THREADNAME_INFO info{};
    info.dwType = 0x1000;
    info.szName = name.c_str();
    info.dwThreadID = static_cast<DWORD>(-1);
    info.dwFlags = 0;
    RaiseException(0x406D1388, EXCEPTION_CONTINUE_EXECUTION, sizeof(info) / sizeof(ULONG_PTR),
                   reinterpret_cast<const ULONG_PTR*>(&info));
#else
    (void)name; // pthread_setname_np wiring can be added when running on Linux.
#endif
}

} // namespace haocam::core
