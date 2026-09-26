#pragma once

// Concrete event types published through the EventBus.

#include <cstdint>
#include <string>

namespace haocam::events {

// Camera state transitions and errors.
struct CameraStatus {
    int state = 0; // CameraState as int (avoids header ping-pong)
    std::string stateName;
    std::string deviceId;
    std::string detail;
};

// Aggregated performance counters (published ~2x per second).
struct PerformanceStats {
    double previewFps = 0.0;   // engine output rate
    double cameraFps = 0.0;    // capture rate
    double frameTimeMs = 0.0;  // engine processing wall time
    double gpuTimeMs = 0.0;    // GPU timestamp query result
    double cpuTimeMs = 0.0;    // CPU-side processing time
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t droppedFrames = 0;
    uint32_t pooledTextures = 0;
    uint32_t activeStages = 0;
    uint64_t heapBytes = 0;    // process commit estimate where available
};

// Effect provider availability changes (Phase 2+).
struct ProviderStatusChanged {
    std::string slot;
    std::string provider;
    bool available = false;
    std::string detail;
};

} // namespace haocam::events
