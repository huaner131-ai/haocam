#pragma once

// ID3D11Multithread compatibility shim.
//
// The Windows SDK (MSVC) declares ID3D11Multithread in <d3d11.h>; mingw-w64
// only forward-declares it. HaoCam needs SetMultithreadProtected(TRUE) on
// any device shared between Media Foundation and the render thread, so this
// header exposes a vtable-compatible alias for every supported toolchain.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>

namespace haocam::gfx {

#ifdef _MSC_VER
using MultithreadInterface = ID3D11Multithread;
inline const IID& multithreadIid() {
    return __uuidof(ID3D11Multithread);
}
#else
struct DECLSPEC_UUID("5C175893-18EE-4396-8331-010C8D77B53E")
    MultithreadInterface : public IUnknown {
    virtual BOOL STDMETHODCALLTYPE SetMultithreadProtected(BOOL bMTProtect) = 0;
    virtual BOOL STDMETHODCALLTYPE GetMultithreadProtected() = 0;
};
inline IID multithreadIid() {
    return {0x5C175893, 0x18EE, 0x4396, {0x83, 0x31, 0x01, 0x0C, 0x8D, 0x77, 0xB5, 0x3E}};
}
#endif

// Convenience: enables multithread protection on a device; returns true on
// success (or when already enabled).
inline bool enableDeviceMultithreadProtect(ID3D11Device* device) {
    if (!device) return false;
    ID3D11DeviceContext* context = nullptr;
    device->GetImmediateContext(&context);
    if (!context) return false;
    MultithreadInterface* mt = nullptr;
    const bool ok =
        SUCCEEDED(context->QueryInterface(multithreadIid(), reinterpret_cast<void**>(&mt)));
    if (mt) {
        mt->SetMultithreadProtected(TRUE);
        mt->Release();
    }
    context->Release();
    return ok;
}

} // namespace haocam::gfx
