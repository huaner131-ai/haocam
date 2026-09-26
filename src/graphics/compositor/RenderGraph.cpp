#include "graphics/compositor/RenderGraph.h"

namespace haocam::gfx {

const char* toString(RenderPassId pass) {
    switch (pass) {
        case RenderPassId::ColorConvert: return "ColorConvert";
        case RenderPassId::Composite: return "Composite";
        case RenderPassId::Count: break;
    }
    return "Unknown";
}

void RenderGraph::setPassEnabled(RenderPassId pass, bool enabled) {
    m_enabled[static_cast<size_t>(pass)] = enabled;
}

bool RenderGraph::isPassEnabled(RenderPassId pass) const {
    return m_enabled[static_cast<size_t>(pass)];
}

std::vector<std::string> RenderGraph::activePassNames() const {
    std::vector<std::string> names;
    for (size_t i = 0; i < static_cast<size_t>(RenderPassId::Count); ++i) {
        if (m_enabled[i]) names.push_back(toString(static_cast<RenderPassId>(i)));
    }
    return names;
}

} // namespace haocam::gfx
