#pragma once

// The GPU compositor: turns an input camera frame into the FINAL HaoCam
// frame (one texture distributed to preview / recording / virtual camera).
//
// Phase 1 passes (all D3D11, on the pipeline device):
//   1. ColorConvert : NV12 -> BGRA8 + color adjustments + mirror
//   2. Composite    : processed texture -> final output (aspect-fit)
//
// The compositor owns a ring of final output textures (published to the UI)
// and a texture pool for intermediates. GPU time is measured with D3D11
// timestamp queries.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

#include "frame/Frame.h"
#include "graphics/compositor/RenderGraph.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace haocam {

// Color adjustment parameters driven by FilterController (QML sliders).
struct ColorAdjustments {
    float brightness = 0.0f;  // [-1, 1]
    float contrast = 0.0f;    // [-1, 1]
    float saturation = 0.0f;  // [-1, 1]
};

// One finished output frame, ready for display.
struct CompositorOutput {
    GpuTextureRef texture;
    uint64_t frameId = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

class Compositor {
public:
    Compositor();  // out-of-line: Pipeline is incomplete in this header
    ~Compositor();

    bool initialize(ID3D11Device* device, ID3D11DeviceContext* context);
    void shutdown();
    bool valid() const { return m_device != nullptr; }

    // Processes one camera frame; publishes the newest result into the
    // output ring (releasing older entries). Safe to call repeatedly.
    // `frame` is expected to be GPU-resident (NV12 or BGRA8).
    void process(Frame& frame);

    // Returns the processed (color-converted) texture of the LAST process()
    // call - the input the tracking worker and beauty engine consume.
    GpuTextureRef lastProcessedTexture() const;

    // Composite-source override (beauty output). Set before process() on the
    // engine thread; when set, pass 2 composites this texture instead of the
    // freshly color-converted one. Cleared automatically when the texture
    // reference is empty.
    void setCompositeOverride(GpuTextureRef texture) { m_compositeOverride = std::move(texture); }
    void clearCompositeOverride() { m_compositeOverride.reset(); }

    // Output ring access (render thread). latestFrameId() lets the UI skip
    // work when nothing new arrived. The texture is valid until the next
    // process() overwrites its slot.
    CompositorOutput latestOutput();
    uint64_t latestFrameId() const { return m_latestFrameId.load(std::memory_order_acquire); }
    void notifyOutputConsumed(uint64_t frameId);

    ColorAdjustments& adjustments() { return m_adjustments; }

    gfx::RenderGraph& renderGraph() { return m_renderGraph; }

    // Diagnostics.
    double lastGpuTimeMs() const { return m_gpuTimeMs.load(std::memory_order_relaxed); }
    double lastCpuTimeMs() const { return m_cpuTimeMs.load(std::memory_order_relaxed); }
    double processFps() const { return m_processFps.load(std::memory_order_relaxed); }
    size_t pooledTextures() const;

private:
    struct Pipeline; // defined in Compositor.cpp (D3D11 internals)
    std::unique_ptr<Pipeline> m_pipeline;

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    std::mutex m_outputMutex;
    std::condition_variable m_outputReady;
    CompositorOutput m_outputRing[3];
    uint64_t m_outputIds[3] = {0, 0, 0};
    uint64_t m_consumedId = 0; // guarded by m_outputMutex
    uint32_t m_ringIndex = 0;
    std::atomic<uint64_t> m_latestFrameId{0};

    ColorAdjustments m_adjustments;
    gfx::RenderGraph m_renderGraph;

    GpuTextureRef m_compositeOverride; // engine thread only (set/clear + read)
    GpuTextureRef m_lastProcessed;     // engine thread only (written in process)

    std::chrono::steady_clock::time_point m_lastProcess{};
    std::atomic<double> m_gpuTimeMs{0.0};
    std::atomic<double> m_cpuTimeMs{0.0};
    std::atomic<double> m_processFps{0.0};
};

} // namespace haocam
