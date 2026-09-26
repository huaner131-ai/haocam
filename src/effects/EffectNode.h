#pragma once

// Logical pipeline stage. Nodes name a stage of the processing order
// (architecture spec, section 19); the EffectGraph decides which nodes run
// and in what order. Providers attach their work to nodes; the GPU pass
// mapping lives in the compositor's render graph.

#include <functional>
#include <string>

namespace haocam {

enum class EffectStage {
    CameraCapture,
    ColorConversion,
    FaceTracking,
    FaceReshape,
    Beauty,
    Makeup,
    AR,
    ColorLut,
    FinalComposite,
    Outputs,
};

const char* toString(EffectStage stage);

class EffectNode {
public:
    using ProcessFn = std::function<struct Frame&(struct Frame&)>;

    EffectNode(EffectStage stage, std::string name)
        : m_stage(stage), m_name(std::move(name)) {}

    EffectStage stage() const { return m_stage; }
    const std::string& name() const { return m_name; }

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    // Runtime hook (optional for Phase 1; providers will own these later).
    void setProcess(ProcessFn fn) { m_process = std::move(fn); }
    bool hasProcess() const { return static_cast<bool>(m_process); }
    ProcessFn& process() { return m_process; }

private:
    EffectStage m_stage;
    std::string m_name;
    bool m_enabled = false;
    ProcessFn m_process;
};

} // namespace haocam
