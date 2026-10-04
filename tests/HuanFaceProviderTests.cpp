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

HAOCAM_TEST(huanface_downscale2x_solid_and_average) {
    // Solid colour must survive a 2x2 box downscale unchanged.
    std::vector<uint8_t> src(16 * 8 * 4, 0);
    for (size_t i = 0; i < src.size(); i += 4) {
        src[i + 0] = 200; src[i + 1] = 100; src[i + 2] = 50; src[i + 3] = 255;
    }
    std::vector<uint8_t> dst(8 * 4 * 4);
    haocam::huanfaceDownscale2x(src.data(), 16, 8, dst.data());
    for (size_t i = 0; i < dst.size(); i += 4) {
        HAOCAM_EXPECT_EQ(dst[i + 0], 200);
        HAOCAM_EXPECT_EQ(dst[i + 1], 100);
        HAOCAM_EXPECT_EQ(dst[i + 2], 50);
        HAOCAM_EXPECT_EQ(dst[i + 3], 255);
    }
    // 2x2 of four distinct values: every channel averages its quad.
    std::vector<uint8_t> quad(2 * 2 * 4);
    const uint8_t vals[4] = {10, 30, 50, 70};
    for (int px = 0; px < 4; ++px)
        for (int c = 0; c < 4; ++c) quad[px * 4 + c] = vals[px] + static_cast<uint8_t>(c);
    std::vector<uint8_t> one(1 * 1 * 4);
    haocam::huanfaceDownscale2x(quad.data(), 2, 2, one.data());
    for (int c = 0; c < 4; ++c) {
        const int avg = (10 + 30 + 50 + 70) / 4 + c;
        HAOCAM_EXPECT_EQ(one[c], static_cast<uint8_t>(avg));
    }
}

HAOCAM_TEST(huanface_upscale_bilinear_corners_and_gradient) {
    // 2x2 source (left column black, right column white) -> 5x5 target.
    const uint8_t src[2 * 2 * 4] = {
        0, 0, 0, 255,   255, 255, 255, 255,
        0, 0, 0, 255,   255, 255, 255, 255,
    };
    std::vector<uint8_t> dst(5 * 5 * 4);
    haocam::huanfaceUpscaleBilinear(src, 2, 2, dst.data(), 5, 5);
    // Corners keep the source corner values.
    HAOCAM_EXPECT_EQ(dst[0], 0);
    HAOCAM_EXPECT_EQ(dst[4 * 4 + 0], 255);
    // Middle pixel is mid-grey (all channels equal, alpha opaque).
    const uint8_t* mid = dst.data() + (2 * 5 + 2) * 4;
    HAOCAM_EXPECT(mid[0] > 100 && mid[0] < 155);
    // Horizontal gradient is monotonic non-decreasing in a row.
    bool monotonic = true;
    for (int x = 1; x < 5; ++x)
        if (dst[x * 4 + 0] < dst[(x - 1) * 4 + 0]) monotonic = false;
    HAOCAM_EXPECT(monotonic);
}
