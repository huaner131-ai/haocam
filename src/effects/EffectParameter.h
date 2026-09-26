#pragma once

// Typed effect parameter used by the graph and by provider parameter sinks.

#include <string>
#include <variant>

namespace haocam {

struct EffectParameter {
    using Value = std::variant<bool, int, float, std::string>;

    std::string name;
    Value value{0.0f};
    float minValue = 0.0f;
    float maxValue = 1.0f;

    EffectParameter() = default;
    EffectParameter(std::string n, float v, float lo = 0.0f, float hi = 1.0f)
        : name(std::move(n)), value(v), minValue(lo), maxValue(hi) {}
    EffectParameter(std::string n, bool v) : name(std::move(n)), value(v) {}
    EffectParameter(std::string n, std::string v) : name(std::move(n)), value(std::move(v)) {}

    float asFloat(float fallback = 0.0f) const {
        if (const float* f = std::get_if<float>(&value)) return *f;
        if (const int* i = std::get_if<int>(&value)) return static_cast<float>(*i);
        if (const bool* b = std::get_if<bool>(&value)) return *b ? 1.0f : 0.0f;
        return fallback;
    }

    bool asBool(bool fallback = false) const {
        if (const bool* b = std::get_if<bool>(&value)) return *b;
        return asFloat(fallback ? 1.0f : 0.0f) > 0.5f;
    }

    std::string asString(const std::string& fallback = {}) const {
        if (const std::string* s = std::get_if<std::string>(&value)) return *s;
        return fallback;
    }
};

} // namespace haocam
