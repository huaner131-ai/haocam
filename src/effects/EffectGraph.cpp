#include "effects/EffectGraph.h"

#include "effects/EffectProvider.h"

#include <algorithm>

namespace haocam {

const char* toString(ProviderType type) {
    switch (type) {
        case ProviderType::Beauty: return "Beauty";
        case ProviderType::Makeup: return "Makeup";
        case ProviderType::AR: return "AR";
        case ProviderType::Native: return "Native";
    }
    return "Unknown";
}

const char* toString(EffectStage stage) {
    switch (stage) {
        case EffectStage::CameraCapture: return "Camera Capture";
        case EffectStage::ColorConversion: return "Color Conversion";
        case EffectStage::FaceTracking: return "Face Tracking";
        case EffectStage::FaceReshape: return "Face Reshape";
        case EffectStage::Beauty: return "Beauty";
        case EffectStage::Makeup: return "Makeup";
        case EffectStage::AR: return "AR";
        case EffectStage::ColorLut: return "Color / LUT";
        case EffectStage::FinalComposite: return "Final Composite";
        case EffectStage::Outputs: return "Outputs";
    }
    return "Unknown";
}

EffectGraph::EffectGraph() {
    const EffectStage stages[] = {
        EffectStage::CameraCapture,   EffectStage::ColorConversion,
        EffectStage::FaceTracking,    EffectStage::FaceReshape,
        EffectStage::Beauty,          EffectStage::Makeup,
        EffectStage::AR,              EffectStage::ColorLut,
        EffectStage::FinalComposite,  EffectStage::Outputs,
    };
    for (EffectStage stage : stages) {
        m_nodes.emplace_back(stage, toString(stage));
    }
    // Stages that always run in Phase 1.
    setStageEnabled(EffectStage::CameraCapture, true);
    setStageEnabled(EffectStage::ColorConversion, true);
    setStageEnabled(EffectStage::FinalComposite, true);
    setStageEnabled(EffectStage::Outputs, true);
}

EffectNode* EffectGraph::findStage(EffectStage stage) {
    for (auto& node : m_nodes) {
        if (node.stage() == stage) return &node;
    }
    return nullptr;
}

const EffectNode* EffectGraph::findStage(EffectStage stage) const {
    for (const auto& node : m_nodes) {
        if (node.stage() == stage) return &node;
    }
    return nullptr;
}

void EffectGraph::setStageEnabled(EffectStage stage, bool enabled) {
    if (EffectNode* node = findStage(stage)) node->setEnabled(enabled);
}

bool EffectGraph::isStageEnabled(EffectStage stage) const {
    const EffectNode* node = findStage(stage);
    return node && node->enabled();
}

std::vector<std::string> EffectGraph::activeStageNames() const {
    std::vector<std::string> names;
    for (const auto& node : m_nodes) {
        if (node.enabled()) names.push_back(node.name());
    }
    return names;
}

bool EffectGraph::isOrderValid() const {
    // Guards future edits/provider registrations that could shuffle nodes:
    // the node vector must follow the canonical EffectStage sequence.
    size_t last = 0;
    for (const auto& node : m_nodes) {
        const size_t current = static_cast<size_t>(node.stage());
        if (current < last) return false;
        last = current;
    }
    return true;
}

} // namespace haocam
