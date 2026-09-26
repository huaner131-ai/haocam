#pragma once

// Metadata carried alongside every HaoCam frame.

#include <cstdint>
#include <string>

#include "graphics/RenderTypes.h"

namespace haocam {

enum class PixelFormat : uint8_t {
    Unknown = 0,
    NV12,    // camera native, GPU resident
    BGRA8,   // composited final frame
    RGBA8,
};

inline const char* toString(PixelFormat format) {
    switch (format) {
        case PixelFormat::Unknown: return "Unknown";
        case PixelFormat::NV12: return "NV12";
        case PixelFormat::BGRA8: return "BGRA8";
        case PixelFormat::RGBA8: return "RGBA8";
    }
    return "Unknown";
}

struct FrameMetadata {
    uint64_t frameId = 0;
    uint64_t timestampUs = 0;   // capture timestamp, microseconds since epoch
    uint64_t durationUs = 0;    // expected frame duration from the camera mode
    uint32_t sourceFps = 0;     // configured camera frame rate
    uint32_t strideY = 0;       // CPU-path strides (GPU paths ignore these)
    uint32_t strideUV = 0;
    bool gpuResident = false;   // true when pixels never left the GPU
    bool mirrored = false;      // mirror applied during processing
    std::string sourceName;     // camera device name (diagnostics)
};

} // namespace haocam
