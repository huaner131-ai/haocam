#pragma once

// Tracking thread (Phase 2, spec section 13).
//
//   Camera thread -> FrameQueue -> Engine thread -> [latest slot] -> this
//
// The worker keeps only the LATEST submitted frame (drop-stale): if tracking
// falls behind, old frames are overwritten instead of queued, so latency is
// bounded and memory never grows. The newest FaceTrackingResult is published
// as a snapshot for the engine/GPU thread and the UI.
//
// Pixels reach the tracker through IPixelSource: on Windows the source does
// a GPU downscale + staging readback (see graphics/D3D11/D3D11PixelSource);
// on CPU-resident frames (tests, null camera) an NV12->RGBA conversion runs
// into a reusable buffer (spec section 35).

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "face/FaceData.h"
#include "face/FaceTracker.h"
#include "face/LandmarkSmoother.h"
#include "frame/Frame.h"

namespace haocam {

// One submitted tracking job.
struct TrackingInput {
    GpuTextureRef gpu;                 // BGRA8 GPU texture (Windows path)
    std::shared_ptr<FrameBuffer> cpu;  // NV12 CPU buffer (tests / fallback)
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t frameId = 0;
    uint64_t timestampUs = 0;
};

// Produces tightly-packed RGBA8 pixels for the tracker. Implementations must
// reuse buffers (no per-frame allocation in steady state).
class IPixelSource {
public:
    virtual ~IPixelSource() = default;

    // Writes w*h*4 RGBA bytes into `dst`. Returns false when the input
    // cannot be read this frame (caller simply skips tracking).
    virtual bool readRGBA(const TrackingInput& input, std::vector<uint8_t>& dst,
                          uint32_t& width, uint32_t& height) = 0;
    virtual const char* name() const = 0;
};

struct TrackingStats {
    double fps = 0.0;
    double lastProcessMs = 0.0;
    float confidence = 0.0f;
    int faceCount = 0;
    uint64_t processedFrames = 0;
    uint64_t droppedFrames = 0; // overwritten latest-slot frames
};

class TrackingWorker {
public:
    TrackingWorker(std::unique_ptr<IFaceTracker> tracker,
                   std::unique_ptr<IPixelSource> pixelSource);
    ~TrackingWorker();

    // Starts the worker thread and initializes the tracker. Returns false
    // (and keeps the app functional) when the tracker is unavailable.
    bool start(const FaceTrackerConfig& config);
    void stop();

    // Overwrites the pending frame (drop-stale, never blocks for long).
    void submit(TrackingInput&& input);

    // Thread-safe snapshot for the engine/UI threads.
    FaceTrackingResult latestTracking() const;

    bool isRunning() const { return m_running.load(); }
    std::string trackerStatus() const;
    IFaceTracker* tracker() const { return m_tracker.get(); }
    TrackingStats stats() const;

    // Live configuration (thread-safe).
    void setSmoothing(float minCutoff, float beta);
    void setMaxFps(float fps);

private:
    void run();
    void applySmoothing(FaceTrackingResult& result, double dtSeconds);

    std::unique_ptr<IFaceTracker> m_tracker;
    std::unique_ptr<IPixelSource> m_pixelSource;

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};

    mutable std::mutex m_inputMutex;
    std::condition_variable m_inputCv;
    TrackingInput m_pending; // overwritten, never queued
    bool m_hasPending = false;

    mutable std::mutex m_resultMutex;
    FaceTrackingResult m_latest;

    mutable std::mutex m_statsMutex;
    TrackingStats m_stats;
    std::chrono::steady_clock::time_point m_lastResultTime{};

    // Reusable RGBA scratch buffer (spec section 35).
    std::vector<uint8_t> m_rgbaScratch;

    LandmarkSmoother m_smoother;
    std::atomic<float> m_maxFps{30.0f};
};

// CPU fallback pixel source: converts NV12 (stride == width) to RGBA using a
// reusable buffer. Used for CPU-resident frames (null camera, tests).
class CpuNv12PixelSource final : public IPixelSource {
public:
    bool readRGBA(const TrackingInput& input, std::vector<uint8_t>& dst,
                  uint32_t& width, uint32_t& height) override;
    const char* name() const override { return "CpuNv12"; }
};

} // namespace haocam
