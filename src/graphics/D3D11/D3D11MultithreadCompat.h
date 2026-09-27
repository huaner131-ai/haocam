#pragma once

// ID3D11Multithread compatibility shim.
//
// The full ID3D11Multithread declaration is NOT reliably available from
// <d3d11.h> alone: mingw-w64 only forward-declares it there (definition in
// d3d11_4.h), and even the MSVC SDK 10.0.26100 does not declare it from
// <d3d11.h> (observed with cl 19.44 - C2061 at the using-declaration).
// HaoCam only needs SetMultithreadProtected(TRUE) on the shared device, so
// this header declares a vtable-compatible interface carrying the REAL IID
// (IID_ID3D11Multithread from d3d11_4.h) and QueryInterfaces it from the
// immediate context. Identical behavior on MSVC and MinGW, no SDK-variant
// dependency.
//
// IID verified against d3d11_4.h (official SDK copy as vendored by renderdoc
// and the mingw-w64 headers):
//   DEFINE_GUID(IID_ID3D11Multithread,
//               0x9B7E4E00, 0x342C, 0x4106,
//               0xA1, 0x9F, 0x4F, 0x27, 0x04, 0xF6, 0x89, 0xF0);

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>

namespace haocam::gfx {

// {9B7E4E00-342C-4106-A19F-4F2704F689F0}
struct DECLSPEC_UUID("9B7E4E00-342C-4106-A19F-4F2704F689F0")
    MultithreadInterface : public IUnknown {
    virtual BOOL STDMETHODCALLTYPE SetMultithreadProtected(BOOL bMTProtect) = 0;
    virtual BOOL STDMETHODCALLTYPE GetMultithreadProtected() = 0;
};

inline IID multithreadIid() {
    return {0x9B7E4E00, 0x342C, 0x4106,
            {0xA1, 0x9F, 0x4F, 0x27, 0x04, 0xF6, 0x89, 0xF0}};
}

// Convenience: enables multithread protection on a device; returns true on
// success (or when already enabled).
inline bool enableDeviceMultithreadProtect(ID3D11Device* device) {
    if (!device) return false;
    ID3D11DeviceContext* context = nullptr;
    device->GetImmediateContext(&context);
    if (!context) return false;
    MultithreadInterface* mt = nullptr;
    const bool ok = SUCCEEDED(
        context->QueryInterface(multithreadIid(), reinterpret_cast<void**>(&mt)));
    if (mt) {
        mt->SetMultithreadProtected(TRUE);
        mt->Release();
    }
    context->Release();
    return ok;
}

} // namespace haocam::gfx
