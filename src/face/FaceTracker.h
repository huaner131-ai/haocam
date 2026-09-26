#pragma once

// Face tracker interface. Real trackers (MediaPipe, provider-internal) are
// introduced in Phase 2; Phase 1 ships only the interface and a null
// tracker that reports "not detected" and logs availability honestly.

#include <memory>
#include <string>

#include "face/FaceData.h"
#include "frame/Frame.h"

namespace haocam {

enum class TrackerBackend {
    None,       // no tracker available
    MediaPipe,  // planned Phase 2
    Provider,   // tracker embedded in an SDK provider (Facebetter / Snap)
};

class IFaceTracker {
public:
    virtual ~IFaceTracker() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool isAvailable() const = 0;

    virtual const char* name() const = 0;
    virtual TrackerBackend backend() const = 0;

    // Tracks faces in the input frame and fills `result`. Implementations
    // must be thread-safe with respect to shutdown().
    virtual bool track(const Frame& frame, FaceData& result) = 0;

    virtual void setMinDetectionConfidence(float confidence) = 0;
    virtual float minDetectionConfidence() const = 0;
};

// Null tracker: always reports "no face", never fails the pipeline.
class NullFaceTracker final : public IFaceTracker {
public:
    bool initialize() override;
    void shutdown() override;
    bool isAvailable() const override { return false; }
    const char* name() const override { return "NullTracker"; }
    TrackerBackend backend() const override { return TrackerBackend::None; }
    bool track(const Frame& frame, FaceData& result) override;
    void setMinDetectionConfidence(float confidence) override { m_confidence = confidence; }
    float minDetectionConfidence() const override { return m_confidence; }

private:
    float m_confidence = 0.5f;
};

// Returns the best available tracker (Phase 2: MediaPipe). Currently the
// null tracker, which keeps the pipeline functional and honest.
std::unique_ptr<IFaceTracker> createDefaultTracker();

} // namespace haocam
