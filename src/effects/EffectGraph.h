#pragma once

// The ordered effect pipeline (architecture spec, section 19).
//
// Default processing order:
//   1. Camera Capture        6. Makeup
//   2. Color Conversion      7. AR
//   3. Face Tracking         8. Color / LUT
//   4. Face Reshape          9. Final Composite
//   5. Beauty               10. Preview / Recording / Virtual Camera
//
// The order is data, not hardcoded branching: stages are enabled/disabled
// by EffectManager depending on which providers are available.

#include <vector>

#include "effects/EffectNode.h"

namespace haocam {

class EffectGraph {
public:
    EffectGraph(); // builds the default stage list in canonical order

    // Number of nodes and index-based access.
    size_t nodeCount() const { return m_nodes.size(); }
    EffectNode* node(size_t index) { return &m_nodes[index]; }
    const EffectNode* node(size_t index) const { return &m_nodes[index]; }

    EffectNode* findStage(EffectStage stage);
    const EffectNode* findStage(EffectStage stage) const;

    void setStageEnabled(EffectStage stage, bool enabled);
    bool isStageEnabled(EffectStage stage) const;

    // Names of enabled stages in execution order (diagnostics overlay).
    std::vector<std::string> activeStageNames() const;

    // First disabled stage that precedes an enabled one - used to detect
    // ordering mistakes when providers register out of order (tests).
    bool isOrderValid() const;

private:
    std::vector<EffectNode> m_nodes;
};

} // namespace haocam
