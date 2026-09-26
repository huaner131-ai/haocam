#pragma once

// GPU<->CPU bridge utilities for SDK providers that require CPU pixels
// (Facebetter desktop takes CPU RGBA and returns CPU RGBA - docs
// facebetter.net/windows/implement-beauty). Everything reuses buffers and
// staging textures (spec section 35); costs are measured and surfaced in the
// diagnostics overlay (spec section 22).

#include <cstdint>
#include <memory>
#include <vector>

#include "frame/TexturePool.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace haocam::gfx {

// Reads a BGRA8 GPU texture into a CPU byte buffer through a small ring of
// staging textures (avoids stalling on the same staging texture twice).
class GpuFrameCopier {
public:
    static std::unique_ptr<GpuFrameCopier> create(ID3D11Device* device);

    ~GpuFrameCopier();

    // Copies src (BGRA8) into `dst` (row-pitch-compacted, size w*h*4).
    // Returns false when formats/sizes mismatch or the copy fails.
    bool readBGRA(const GpuTextureRef& src, std::vector<uint8_t>& dst);

    // Uploads tightly-packed BGRA8 data into a pooled texture (SRV + RT bind
    // flags so downstream passes can sample it). Returns null on failure.
    GpuTextureRef uploadBGRA(ITexturePool& pool, const uint8_t* data, uint32_t width,
                             uint32_t height);

    // Diagnostics.
    double lastReadMs() const { return m_lastReadMs; }
    double lastUploadMs() const { return m_lastUploadMs; }

private:
    GpuFrameCopier() = default;

    ID3D11Device* m_device = nullptr;
    struct Staging {
        void* texture = nullptr; // ID3D11Texture2D* (USAGE_STAGING)
        uint64_t frameId = 0;
        bool pending = false;
    };
    Staging m_ring[2];
    int m_next = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    double m_lastReadMs = 0.0;
    double m_lastUploadMs = 0.0;
};

} // namespace haocam::gfx
