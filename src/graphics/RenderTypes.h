#pragma once

// Backend-neutral render types shared between the frame model and the
// graphics layer. Direct3D 11 is the initial backend; the abstraction is
// kept small so a Direct3D 12 backend can be added later without touching
// the frame/effect layers.

#include <cstdint>

namespace haocam {

enum class GpuBackend : uint8_t {
    None = 0,
    D3D11,
};

// Bind flags mirror the D3D11 subset HaoCam needs; other backends map them.
enum class TextureBind : uint8_t {
    None = 0,
    ShaderResource = 1 << 0,
    RenderTarget = 1 << 1,
};

inline uint8_t operator|(TextureBind a, TextureBind b) {
    return static_cast<uint8_t>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline bool hasBind(uint8_t flags, TextureBind flag) {
    return (flags & static_cast<uint8_t>(flag)) != 0;
}

} // namespace haocam
