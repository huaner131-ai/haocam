/**
 * Blend Modes Implementation — Phase 6
 */

#include "blend_modes.h"

namespace huanface {

// No extra implementation needed, all in header for inline
// But provide ToFloatMap etc for params

std::map<std::string, float> HFMakeupParameters::ToFloatMap() const {
    std::map<std::string, float> m;
    m["makeup.lip.intensity"] = lip.intensity;
    m["makeup.lip.opacity"] = lip.opacity;
    m["makeup.lip.feather"] = lip.feather;
    m["makeup.foundation.intensity"] = foundation.intensity;
    m["makeup.foundation.opacity"] = foundation.opacity;
    m["makeup.blush.intensity"] = blush.intensity;
    m["makeup.blush.opacity"] = blush.opacity;
    m["makeup.eyebrow.intensity"] = eyebrow.intensity;
    m["makeup.eyebrow.opacity"] = eyebrow.opacity;
    m["makeup.eyeliner.intensity"] = eyeliner.intensity;
    m["makeup.eyeliner.opacity"] = eyeliner.opacity;
    m["makeup.eyeliner.thickness"] = eyeliner.thickness;
    m["makeup.eyelash.intensity"] = eyelash.intensity;
    m["makeup.eyelash.opacity"] = eyelash.opacity;
    m["makeup.eyelash.length"] = eyelash.length;
    m["makeup.eyeshadow.intensity"] = eyeshadow.intensity;
    m["makeup.eyeshadow.opacity"] = eyeshadow.opacity;
    m["makeup.pupil.intensity"] = pupil.intensity;
    m["makeup.pupil.opacity"] = pupil.opacity;
    m["makeup.pupil.irisEnhancement"] = pupil.irisEnhancement;
    return m;
}

std::map<std::string, HFFloat4> HFMakeupParameters::ToColorMap() const {
    std::map<std::string, HFFloat4> m;
    m["makeup.lip.color"] = lip.color;
    m["makeup.foundation.color"] = foundation.color;
    m["makeup.blush.color"] = blush.color;
    m["makeup.eyebrow.color"] = eyebrow.color;
    m["makeup.eyeliner.color"] = eyeliner.color;
    m["makeup.eyelash.color"] = eyelash.color;
    m["makeup.eyeshadow.color"] = eyeshadow.color;
    m["makeup.pupil.color"] = pupil.color;
    return m;
}

} // namespace huanface
