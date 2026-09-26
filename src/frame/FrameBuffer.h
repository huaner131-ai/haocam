#pragma once

// CPU-side pixel storage used for fallback paths and synthetic sources.

#include <cstdint>
#include <vector>

namespace haocam {

struct FrameBuffer {
    std::vector<uint8_t> data;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t strideY = 0;
    uint32_t strideUV = 0;

    // NV12 layout: full-resolution Y plane followed by interleaved UV plane
    // (width x height/2 bytes), contiguous, row pitch == width.
    void allocateNV12(uint32_t w, uint32_t h) {
        width = w;
        height = h;
        strideY = w;
        strideUV = w;
        data.assign(static_cast<size_t>(strideY) * h * 3 / 2, 0x10);
    }

    uint8_t* yPlane() { return data.data(); }
    const uint8_t* yPlane() const { return data.data(); }
    uint8_t* uvPlane() { return data.data() + static_cast<size_t>(strideY) * height; }
    const uint8_t* uvPlane() const {
        return data.data() + static_cast<size_t>(strideY) * height;
    }
    size_t yPlaneSize() const { return static_cast<size_t>(strideY) * height; }
    size_t uvPlaneSize() const { return static_cast<size_t>(strideUV) * (height / 2); }
};

} // namespace haocam
