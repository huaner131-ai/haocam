#include "graphics/D3D11/D3D11Context.h"

#include "graphics/D3D11/D3D11MultithreadCompat.h"

#include "core/logging/Logger.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace haocam::gfx {

namespace {

constexpr const char* kCategory = "gpu";

} // namespace

D3D11Context::~D3D11Context() { release(); }

void D3D11Context::release() {
    if (m_context) {
        m_context->Release();
        m_context = nullptr;
    }
    if (m_device) {
        m_device->Release(); // adopted devices were AddRef'd too
        m_device = nullptr;
    }
    m_external = false;
    m_multithreadProtected = false;
}

bool D3D11Context::adoptExternal(ID3D11Device* device, ID3D11DeviceContext* context) {
    if (!device) return false;
    release();

    m_device = device;
    m_device->AddRef();
    if (context) {
        m_context = context;
        m_context->AddRef();
    } else {
        m_device->GetImmediateContext(&m_context);
    }
    m_external = true;
    m_featureLevel = static_cast<uint32_t>(m_device->GetFeatureLevel());
    enableMultithreadProtection();
    HAOCAM_LOG_INFO(kCategory, "Adopted external D3D11 device (feature level 0x{:04X})",
                    m_featureLevel);
    return true;
}

bool D3D11Context::createStandalone(const D3D11DeviceOptions& options) {
    UINT flags = 0;
    if (options.videoSupport) flags |= D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
    if (options.bgraSupport) flags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (options.debugLayer) flags |= D3D11_CREATE_DEVICE_DEBUG;

    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
    };

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL obtained{};

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels,
                                   ARRAYSIZE(levels), D3D11_SDK_VERSION,
                                   device.GetAddressOf(), &obtained, context.GetAddressOf());
    if (FAILED(hr) && options.allowWarpFallback) {
        HAOCAM_LOG_WARN(kCategory, "Hardware D3D11 device unavailable (hr=0x{:08X}); using WARP",
                        static_cast<unsigned>(hr));
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels,
                               ARRAYSIZE(levels), D3D11_SDK_VERSION, device.GetAddressOf(),
                               &obtained, context.GetAddressOf());
    }
    if (FAILED(hr)) {
        HAOCAM_LOG_ERROR(kCategory, "D3D11CreateDevice failed (hr=0x{:08X})",
                         static_cast<unsigned>(hr));
        return false;
    }

    release();
    m_device = device.Detach();
    m_context = context.Detach();
    m_external = false;
    m_featureLevel = static_cast<uint32_t>(obtained);
    enableMultithreadProtection();
    HAOCAM_LOG_INFO(kCategory, "Created standalone D3D11 device (feature level 0x{:04X})",
                    m_featureLevel);
    return true;
}

void D3D11Context::enableMultithreadProtection() {
    if (!m_context || m_multithreadProtected) return;
    if (enableDeviceMultithreadProtect(m_device)) {
        m_multithreadProtected = true;
    }
}

} // namespace haocam::gfx
