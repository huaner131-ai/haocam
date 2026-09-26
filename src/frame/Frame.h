#pragma once

// The HaoCam frame model (architecture spec, section 14).
//
// A Frame is GPU-resident whenever possible: `texture` carries a native GPU
// texture handle produced by the capture or effect layer. CPU pixel data is
// only attached as a fallback (null camera source, exotic converters) and is
// then uploaded once before GPU processing.

#include <cstdint>
#include <memory>

#include "frame/FrameBuffer.h"
#include "frame/GPUTexture.h"

namespace haocam {

struct Frame {
    uint64_t id = 0;
    uint64_t timestamp = 0;   // microseconds since epoch
    uint32_t width = 0;
    uint32_t height = 0;
    PixelFormat format = PixelFormat::Unknown;
    GPUTexture texture;       // GPU-resident pixels (preferred)
    FrameMetadata metadata;

    // CPU fallback path (NullCameraSource, rare converter outputs).
    std::shared_ptr<FrameBuffer> cpuBuffer;

    bool isGpu() const { return texture.valid(); }
    bool isValid() const { return width > 0 && height > 0 && (texture.valid() || cpuBuffer); }
};

} // namespace haocam
