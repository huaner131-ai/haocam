#pragma once

// Honest fallback beauty provider (spec sections 17/21/33/37).
// Always compiled; reports Unavailable with the configured reason and
// processes nothing. The pipeline passes frames through unchanged.

#include <mutex>
#include <string>

#include "effects/beauty/BeautyProvider.h"

namespace haocam {

class NullBeautyProvider final : public IBeautyProvider {
public:
    explicit NullBeautyProvider(std::string reason);

    // Updates the unavailability reason (e.g. after a failed engine init).
    void setReason(std::string reason);

    bool initialize(const EffectContext& context) override;
    void shutdown() override;
    bool isAvailable() const override;
    std::string statusText() const override;
    const char* name() const override;
    ProviderType type() const override;
    Features features() const override;

    void setSmoothing(float value) override;
    void setWhitening(float value) override;
    void setRosy(float value) override;
    void setSharpen(float value) override;
    void setFaceSlim(float value) override;
    void setEyeSize(float value) override;
    void setNoseSize(float value) override;
    void setJawSlim(float value) override;
    void reset() override;
    BeautyConfig config() const override;
    BeautyResult process(const Frame& input, const FaceData& face) override;

private:
    mutable std::mutex m_mutex;
    std::string m_reason;
    BeautyConfig m_config;
};

} // namespace haocam
