// Phase 2: adaptive landmark smoothing (spec section 19).

#include <cmath>
#include <vector>

#include "face/FaceData.h"
#include "face/LandmarkSmoother.h"
#include "test_main.h"

using namespace haocam;

namespace {
std::vector<Point2D> landmarks(float x, float y = 0.5f, size_t count = 68) {
    return std::vector<Point2D>(count, Point2D{x, y});
}
constexpr double kFrame30 = 1.0 / 30.0; // seconds
} // namespace

HAOCAM_TEST(smoother_first_sample_passes_through) {
    LandmarkSmoother smoother;
    auto points = landmarks(0.4f);

    smoother.apply(points, kFrame30);
    HAOCAM_EXPECT(points[0].x == 0.4f);
    HAOCAM_EXPECT(points[30].y == 0.5f);
}

HAOCAM_TEST(smoother_converges_to_constant_target) {
    LandmarkSmoother smoother;
    auto first = landmarks(0.0f);
    smoother.apply(first, kFrame30);

    bool converged = false;
    for (int i = 0; i < 600 && !converged; ++i) {
        auto points = landmarks(1.0f);
        smoother.apply(points, kFrame30);
        HAOCAM_EXPECT(std::isfinite(points[0].x));
        if (points[0].x > 0.99f) converged = true;
    }
    HAOCAM_EXPECT(converged);
}

HAOCAM_TEST(smoother_is_adaptive_fast_motion_less_damped) {
    // From the same state, a large step must retain a bigger fraction of the
    // jump than a small step (1-Euro-style adaptive cutoff).
    LandmarkSmoother smootherSlow;
    auto base = landmarks(0.5f);
    smootherSlow.apply(base, kFrame30);

    auto small = landmarks(0.51f);
    smootherSlow.apply(small, kFrame30);
    const float smallFraction = (small[0].x - 0.5f) / 0.01f;

    LandmarkSmoother smootherFast;
    auto base2 = landmarks(0.5f);
    smootherFast.apply(base2, kFrame30);

    auto large = landmarks(0.9f);
    smootherFast.apply(large, kFrame30);
    const float largeFraction = (large[0].x - 0.5f) / 0.4f;

    HAOCAM_EXPECT(smallFraction > 0.0f);
    HAOCAM_EXPECT(smallFraction < 1.0f);
    HAOCAM_EXPECT(largeFraction > smallFraction);
}

HAOCAM_TEST(smoother_resets_after_long_gap) {
    LandmarkSmoother smoother;
    auto first = landmarks(0.2f);
    smoother.apply(first, kFrame30);

    auto second = landmarks(0.8f);
    smoother.apply(second, kFrame30);
    HAOCAM_EXPECT(second[0].x < 0.8f); // smoothed

    auto afterGap = landmarks(0.8f);
    smoother.apply(afterGap, 0.6); // > 0.5 s gap: clamped pass-through
    HAOCAM_EXPECT(afterGap[0].x == 0.8f);
}

HAOCAM_TEST(smoother_handles_invalid_dt) {
    LandmarkSmoother smoother;
    auto first = landmarks(0.3f);
    smoother.apply(first, 0.0); // first call with dt<=0: default 1/30

    auto second = landmarks(0.3f);
    smoother.apply(second, -1.0);
    HAOCAM_EXPECT(second[0].x == 0.3f);
    HAOCAM_EXPECT(std::isfinite(second[0].x));
}

HAOCAM_TEST(smoother_survives_layout_change) {
    LandmarkSmoother smoother;
    auto face468 = landmarks(0.3f, 0.3f, 468);
    smoother.apply(face468, kFrame30);

    auto face68 = landmarks(0.6f, 0.6f, 68);
    smoother.apply(face68, kFrame30);
    HAOCAM_EXPECT(face68[0].x >= 0.3f);
    HAOCAM_EXPECT(face68[0].x <= 0.6f);
    HAOCAM_EXPECT(std::isfinite(face68[67].y));
}

HAOCAM_TEST(smoother_empty_input_resets_state) {
    LandmarkSmoother smoother;
    auto points = landmarks(0.5f);
    smoother.apply(points, kFrame30);

    std::vector<Point2D> none;
    smoother.apply(none, kFrame30); // resets

    auto after = landmarks(0.9f);
    smoother.apply(after, kFrame30);
    HAOCAM_EXPECT(after[0].x == 0.9f); // fresh first sample: pass-through
}

HAOCAM_TEST(smoother_stays_finite_over_noise) {
    LandmarkSmoother smoother;
    for (int i = 0; i < 50; ++i) {
        auto points = landmarks(0.5f + 0.01f * static_cast<float>(i % 7));
        smoother.apply(points, kFrame30);
        for (const auto& p : points) {
            HAOCAM_EXPECT(std::isfinite(p.x));
            HAOCAM_EXPECT(std::isfinite(p.y));
        }
    }
}
