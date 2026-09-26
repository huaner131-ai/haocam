#pragma once

// Windows GPU pixel source for the tracking worker (spec section 12).
//
// Downscales the processed BGRA8 frame to the tracking working resolution on
// the GPU (tracker_ps.hlsl: linear sample + channel re-order so staged BGRA8
// memory reads as SRGBA bytes) and reads it back through a staging texture.
// Full-resolution readbacks are avoided: tracking runs at a reduced
// resolution (default <= 480 wide), keeping the CPU readback a few hundred
// microseconds instead of milliseconds. The tradeoff is documented in
// docs/FACE_TRACKING.md.

#include <memory>

#include "face/TrackingWorker.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace haocam::gfx {
class D3D11ShaderProgram;
class D3D11SamplerCache;
}

namespace haocam {

class D3D11PixelSource final : public IPixelSource {
public:
    // `d3d11Device`/`d3d11Context` are the shared pipeline device/context.
    static std::unique_ptr<D3D11PixelSource> create(void* d3d11Device,
                                                    void* d3d11Context);
    ~D3D11PixelSource() override;

    // Working-resolution cap (aspect preserved).
    void setMaxWorkingWidth(uint32_t width) { m_maxWorkingWidth = width; }

    uint32_t trackingWidth() const { return m_trackWidth; }
    uint32_t trackingHeight() const { return m_trackHeight; }
    double lastReadMs() const { return m_lastReadMs; }

    // IPixelSource
    bool readRGBA(const TrackingInput& input, std::vector<uint8_t>& dst,
                  uint32_t& width, uint32_t& height) override;
    const char* name() const override { return "D3D11(downscale)"; }

private:
    D3D11PixelSource() = default;

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    void* m_downscalePipeline = nullptr;  // opaque (D3D11 internals, .cpp-local)
    void* m_trackTarget = nullptr;        // pooled BGRA target (downscaled)
    void* m_staging = nullptr;            // staging texture
    uint32_t m_trackWidth = 0;
    uint32_t m_trackHeight = 0;
    uint32_t m_maxWorkingWidth = 480;
    double m_lastReadMs = 0.0;
};

} // namespace haocam
