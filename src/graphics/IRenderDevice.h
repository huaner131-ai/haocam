#pragma once

// Backend-neutral render device interface.
//
// Phase 1 runs on Direct3D 11 (Qt Quick's device). The interface is kept
// intentionally minimal so a Direct3D 12 backend can slot in later without
// touching the frame or effect layers; concrete implementations live in
// src/graphics/D3D11/.

#include <cstdint>

#include "frame/TexturePool.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace haocam::gfx {

class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    virtual bool valid() const = 0;
    virtual GpuBackend backend() const = 0;

    // Raw device access for the effect pipeline (EffectContext).
    virtual ID3D11Device* nativeDevice() const = 0;
    virtual ID3D11DeviceContext* nativeContext() const = 0;

    // Texture allocation for intermediates and outputs.
    virtual ITexturePool& texturePool() = 0;
};

} // namespace haocam::gfx
