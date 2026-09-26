#pragma once

// GPU-side pass graph. One RenderPass corresponds to one (or few) GPU draws
// executed by the compositor in EffectGraph order. Phase 1 passes:
//
//   ColorConvert : NV12 -> BGRA8 (+ brightness/contrast/saturation, mirror)
//   Composite    : processed texture -> final output texture (aspect fit)
//
// Later phases attach provider passes between Composite's inputs.

#include <string>
#include <vector>

namespace haocam::gfx {

enum class RenderPassId {
    ColorConvert = 0,
    Composite,
    Count,
};

const char* toString(RenderPassId pass);

struct RenderPassStats {
    uint64_t invocations = 0;
    double lastGpuTimeMs = 0.0;
};

class RenderGraph {
public:
    void setPassEnabled(RenderPassId pass, bool enabled);
    bool isPassEnabled(RenderPassId pass) const;

    std::vector<std::string> activePassNames() const;

    RenderPassStats& stats(RenderPassId pass) { return m_stats[static_cast<size_t>(pass)]; }

private:
    bool m_enabled[static_cast<size_t>(RenderPassId::Count)] = {true, true};
    RenderPassStats m_stats[static_cast<size_t>(RenderPassId::Count)];
};

} // namespace haocam::gfx
