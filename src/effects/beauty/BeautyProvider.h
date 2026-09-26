#pragma once

// Beauty provider contract (Phase 2, spec sections 17-21).
//
// IBeautyProvider wraps an external beauty engine (Facebetter). The UI talks
// to EffectManager, EffectManager talks to this interface - never to the SDK.
//
// Parameter model (spec section 18):
//   * Application values are normalized 0.0 (off) .. 1.0 (maximum).
//   * The provider converts to SDK-specific ranges/semantics.
//   * setSmoothing(0.75f) must never be interpreted by the UI layer.
//   * Every public setter clamps to [0, 1] (spec section 30).
//
// Threading (spec section 31):
//   * Setters are called from the UI thread; implementations must be safe
//     against concurrent process() (atomic config snapshot).
//   * process() runs on the beauty worker thread - never the UI thread
//     (spec section 32).

#include <string>

#include "core/config/Json.h"
#include "effects/EffectProvider.h"
#include "face/FaceData.h"
#include "frame/Frame.h"

namespace haocam {

// Normalized beauty parameters (spec section 19). All values [0, 1].
struct BeautyConfig {
    float smoothing = 0.0f;
    float whitening = 0.0f;
    float rosy = 0.0f;
    float sharpen = 0.0f;
    float faceSlim = 0.0f;
    float eyeSize = 0.0f;
    float noseSize = 0.0f;
    float jawSlim = 0.0f;

    void reset();

    // Clamps every member into [0, 1] (spec section 30).
    void clamp();

    bool operator==(const BeautyConfig& other) const;
};

// Result of one beauty pass over a frame. On success `texture` holds the
// processed GPU texture (or, for CPU-pass-through adapters, the caller keeps
// using the original frame and success==false + error explains why).
struct BeautyResult {
    bool success = false;
    GpuTextureRef texture;   // processed frame (GPU-resident when possible)
    std::string error;       // human-readable reason when !success
};

class IBeautyProvider : public IEffectProvider {
public:
    // Normalized setters (UI-facing, spec section 17).
    virtual void setSmoothing(float value) = 0;
    virtual void setWhitening(float value) = 0;
    virtual void setRosy(float value) = 0;
    virtual void setSharpen(float value) = 0;
    virtual void setFaceSlim(float value) = 0;
    virtual void setEyeSize(float value) = 0;
    virtual void setNoseSize(float value) = 0;
    virtual void setJawSlim(float value) = 0;

    virtual void reset() = 0;

    // Thread-safe snapshot of the current parameters.
    virtual BeautyConfig config() const = 0;

    // Processes one frame (with the shared FaceData for providers that use
    // external landmarks). Returns success=false when the provider is
    // unavailable, the frame is empty, or the engine failed - the pipeline
    // then passes the original frame through unchanged (spec section 33).
    virtual BeautyResult process(const Frame& input, const FaceData& face) = 0;

    // Feature support reporting (spec section 17: never fake unsupported
    // features). Providers return false for features their SDK lacks.
    struct Features {
        bool smoothing = true;
        bool whitening = true;
        bool rosy = true;
        bool sharpen = true;
        bool faceSlim = true;
        bool eyeSize = true;
        bool noseSize = true;
        bool jawSlim = true;
    };
    virtual Features features() const { return Features{}; }

    // Diagnostics (default: no measurements). Providers with real engines
    // override these (spec sections 22/34).
    virtual double lastReadbackMs() const { return 0.0; }
    virtual double lastSdkProcessMs() const { return 0.0; }
    virtual double lastUploadMs() const { return 0.0; }
    virtual int detectedFaceCount() const { return 0; }
    virtual uint64_t processedFrames() const { return 0; }
};

// Optional async extension for beauty engines that process on their own
// worker thread (Facebetter). EffectManager drives this directly on the
// engine thread; process(const Frame&, const FaceData&) remains the spec API.
class IBeautyAsync {
public:
    virtual ~IBeautyAsync() = default;
    virtual void submitFrame(const GpuTextureRef& texture, uint64_t frameId) = 0;
    virtual bool latestOutput(GpuTextureRef& outTexture, uint64_t& outFrameId) const = 0;
};

float clamp01(float value);

// JSON helpers (settings persistence).
haocam::core::JsonValue beautyConfigToJson(const BeautyConfig& config);
BeautyConfig beautyConfigFromJson(const haocam::core::JsonValue& json);

} // namespace haocam
