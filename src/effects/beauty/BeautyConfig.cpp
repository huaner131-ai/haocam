#include "effects/beauty/BeautyProvider.h"

#include <algorithm>

#include "core/config/Json.h"

namespace haocam {

void BeautyConfig::reset() { *this = BeautyConfig(); }

void BeautyConfig::clamp() {
    smoothing = clamp01(smoothing);
    whitening = clamp01(whitening);
    rosy = clamp01(rosy);
    sharpen = clamp01(sharpen);
    faceSlim = clamp01(faceSlim);
    eyeSize = clamp01(eyeSize);
    noseSize = clamp01(noseSize);
    jawSlim = clamp01(jawSlim);
}

bool BeautyConfig::operator==(const BeautyConfig& other) const {
    return smoothing == other.smoothing && whitening == other.whitening &&
           rosy == other.rosy && sharpen == other.sharpen && faceSlim == other.faceSlim &&
           eyeSize == other.eyeSize && noseSize == other.noseSize &&
           jawSlim == other.jawSlim;
}

float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

// --- JSON (de)serialization used by settings persistence -------------------

core::JsonValue beautyConfigToJson(const BeautyConfig& config) {
    core::JsonValue json = core::JsonValue::makeObject();
    json.set("smoothing", core::JsonValue(static_cast<double>(config.smoothing)));
    json.set("whitening", core::JsonValue(static_cast<double>(config.whitening)));
    json.set("rosy", core::JsonValue(static_cast<double>(config.rosy)));
    json.set("sharpen", core::JsonValue(static_cast<double>(config.sharpen)));
    json.set("faceSlim", core::JsonValue(static_cast<double>(config.faceSlim)));
    json.set("eyeSize", core::JsonValue(static_cast<double>(config.eyeSize)));
    json.set("noseSize", core::JsonValue(static_cast<double>(config.noseSize)));
    json.set("jawSlim", core::JsonValue(static_cast<double>(config.jawSlim)));
    return json;
}

BeautyConfig beautyConfigFromJson(const core::JsonValue& json) {
    BeautyConfig config;
    config.smoothing = clamp01(static_cast<float>(json.at("smoothing").asNumber(0.0)));
    config.whitening = clamp01(static_cast<float>(json.at("whitening").asNumber(0.0)));
    config.rosy = clamp01(static_cast<float>(json.at("rosy").asNumber(0.0)));
    config.sharpen = clamp01(static_cast<float>(json.at("sharpen").asNumber(0.0)));
    config.faceSlim = clamp01(static_cast<float>(json.at("faceSlim").asNumber(0.0)));
    config.eyeSize = clamp01(static_cast<float>(json.at("eyeSize").asNumber(0.0)));
    config.noseSize = clamp01(static_cast<float>(json.at("noseSize").asNumber(0.0)));
    config.jawSlim = clamp01(static_cast<float>(json.at("jawSlim").asNumber(0.0)));
    return config;
}

} // namespace haocam
