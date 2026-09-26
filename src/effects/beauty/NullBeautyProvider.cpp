#include "effects/beauty/NullBeautyProvider.h"

#include "core/logging/Logger.h"

namespace haocam {

namespace {
constexpr const char* kCategory = "beauty";
}

NullBeautyProvider::NullBeautyProvider(std::string reason) : m_reason(std::move(reason)) {}

void NullBeautyProvider::setReason(std::string reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_reason = std::move(reason);
}

bool NullBeautyProvider::initialize(const EffectContext& context) {
    (void)context;
    HAOCAM_LOG_WARN(kCategory, "Beauty provider unavailable: {}", m_reason);
    return false;
}

void NullBeautyProvider::shutdown() {}

bool NullBeautyProvider::isAvailable() const { return false; }

std::string NullBeautyProvider::statusText() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_reason;
}

const char* NullBeautyProvider::name() const { return "NullBeauty"; }
ProviderType NullBeautyProvider::type() const { return ProviderType::Beauty; }

IBeautyProvider::Features NullBeautyProvider::features() const { return {}; }

// The null provider stores (clamped) values so UI state and Reset stay
// consistent even while the feature is unavailable (spec section 18).
void NullBeautyProvider::setSmoothing(float value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.smoothing = clamp01(value);
}
void NullBeautyProvider::setWhitening(float value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.whitening = clamp01(value);
}
void NullBeautyProvider::setRosy(float value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.rosy = clamp01(value);
}
void NullBeautyProvider::setSharpen(float value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.sharpen = clamp01(value);
}
void NullBeautyProvider::setFaceSlim(float value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.faceSlim = clamp01(value);
}
void NullBeautyProvider::setEyeSize(float value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.eyeSize = clamp01(value);
}
void NullBeautyProvider::setNoseSize(float value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.noseSize = clamp01(value);
}
void NullBeautyProvider::setJawSlim(float value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.jawSlim = clamp01(value);
}

void NullBeautyProvider::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.reset();
}

BeautyConfig NullBeautyProvider::config() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

BeautyResult NullBeautyProvider::process(const Frame& input, const FaceData& face) {
    (void)input;
    (void)face;
    BeautyResult result;
    std::lock_guard<std::mutex> lock(m_mutex);
    result.error = m_reason;
    return result;
}

} // namespace haocam
