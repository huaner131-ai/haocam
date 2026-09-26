#pragma once

// D3D11 device/context wrapper. HaoCam's Phase-1 design runs the effect
// pipeline on the Direct3D 11 device owned by Qt Quick's RHI so camera
// frames never cross device boundaries. This wrapper can also create and
// own a standalone device (used by Media Foundation when no external device
// is provided, and by tests).

#include <cstdint>
#include <memory>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace haocam::gfx {

struct D3D11DeviceOptions {
    bool videoSupport = true;    // D3D11_CREATE_DEVICE_VIDEO_SUPPORT
    bool bgraSupport = true;     // D3D11_CREATE_DEVICE_BGRA_SUPPORT
    bool debugLayer = false;
    bool allowWarpFallback = true;
};

class D3D11Context {
public:
    D3D11Context() = default;
    ~D3D11Context();

    D3D11Context(const D3D11Context&) = delete;
    D3D11Context& operator=(const D3D11Context&) = delete;

    // Adopts an externally owned device (e.g. Qt Quick's RHI device).
    // Does not take ownership; AddRefs.
    bool adoptExternal(ID3D11Device* device, ID3D11DeviceContext* context = nullptr);

    // Creates a fresh device (hardware, with optional WARP fallback).
    bool createStandalone(const D3D11DeviceOptions& options = {});

    bool valid() const { return m_device != nullptr; }
    ID3D11Device* device() const { return m_device; }
    ID3D11DeviceContext* context() const { return m_context; }
    uint32_t featureLevel() const { return m_featureLevel; }
    bool external() const { return m_external; }

    // Enables D3D11 multithread protection (required when Media Foundation
    // and the render thread share the immediate context).
    void enableMultithreadProtection();

private:
    void release();

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    uint32_t m_featureLevel = 0;
    bool m_external = false;
    bool m_multithreadProtected = false;
};

} // namespace haocam::gfx
