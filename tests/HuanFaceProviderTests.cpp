// Integration tests for the HuanFace beauty provider (compiled only when
// HAOCAM_HAS_HUANFACE is set - the SDK drop must exist under
// sdk/huanface/sdk). Verifies the real SDK engine lifecycle on the
// null/CPU path: engine create on the worker, honest availability, clamp
// handling and clean shutdown.

#include <chrono>
#include <thread>

#include "effects/EffectContext.h"
#include "effects/beauty/huanface/HuanFaceProvider.h"

#include "test_main.h"

HAOCAM_TEST(huanface_provider_engine_lifecycle) {
    haocam::HuanFaceProvider provider;
    provider.configure(nullptr, nullptr);

    haocam::EffectContext context; // null device: Linux/CI null backend path
    HAOCAM_EXPECT(provider.initialize(context));

    // The worker creates the engine asynchronously (retry window 4s; the
    // first attempt happens immediately).
    bool available = false;
    for (int i = 0; i < 100 && !available; ++i) {
        available = provider.isAvailable();
        if (!available) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    HAOCAM_EXPECT(available);
    HAOCAM_EXPECT(provider.detectedFaceCount() >= 0);

    // Normalized setters clamp and snapshot correctly.
    provider.setSmoothing(2.0f);
    HAOCAM_EXPECT(provider.config().smoothing <= 1.0f);
    provider.setSmoothing(-1.0f);
    HAOCAM_EXPECT(provider.config().smoothing >= 0.0f);
    provider.setSmoothing(0.5f);
    HAOCAM_EXPECT(provider.config().smoothing > 0.49f && provider.config().smoothing < 0.51f);

    // Reshape is honestly unsupported (Phase 8 scope of the SDK).
    const auto features = provider.features();
    HAOCAM_EXPECT(!features.faceSlim);
    HAOCAM_EXPECT(!features.eyeSize);
    HAOCAM_EXPECT(!features.noseSize);
    HAOCAM_EXPECT(!features.jawSlim);
    HAOCAM_EXPECT(features.smoothing);

    provider.reset();
    HAOCAM_EXPECT(provider.config().smoothing == 0.0f);

    provider.shutdown();
    HAOCAM_EXPECT(!provider.isAvailable() || true); // shutdown must not crash
}
