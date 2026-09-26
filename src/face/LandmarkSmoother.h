#pragma once

// Temporal landmark stabilization (Phase 2, spec section 14).
//
// Adaptive exponential smoothing in the style of the One Euro filter: a
// speed-dependent cutoff keeps slow micro-jitter flat while fast, real
// motion stays responsive. Chosen over a plain low-pass because plain
// exponential smoothing visibly lags head motion at the strengths needed
// to hide landmark jitter, and over a fixed-rate One Euro implementation
// because HaoCam's tracker rate varies with the camera.
//
// Formula per landmark coordinate:
//   cutoff = minCutoff + beta * |speed|
//   alpha  = 1 / (1 + tau / dt), tau = 1 / (2 * pi * cutoff)
//   smooth = smooth + alpha * (sample - smooth)
//
// Header-only, platform-neutral, unit-tested.

#include <cmath>
#include <vector>

#include "face/FaceData.h"

namespace haocam {

class LandmarkSmoother {
public:
    // minCutoff: baseline cutoff frequency in Hz (lower = smoother).
    // beta: speed coefficient (higher = more responsive to fast motion).
    explicit LandmarkSmoother(float minCutoff = 1.2f, float beta = 0.007f)
        : m_minCutoff(minCutoff), m_beta(beta) {}

    void configure(float minCutoff, float beta) {
        m_minCutoff = minCutoff;
        m_beta = beta;
    }

    void reset() {
        m_previous.clear();
        m_hasPrevious = false;
    }

    // Smooths `landmarks` in place. `dtSeconds` is the time since the
    // previous call. A gap longer than 0.5 s passes the samples through
    // unchanged (no motion smear after stalls) and resets the speed
    // estimates. Alpha follows the One Euro definition:
    //   cutoff = minCutoff + beta * |speed|
    //   alpha  = 1 / (1 + 1 / (2*pi * cutoff * dt))
    // i.e. fast motion raises the cutoff and is damped less.
    void apply(std::vector<Point2D>& landmarks, double dtSeconds) {
        if (landmarks.empty()) {
            reset();
            return;
        }

        // Re-size caches only when the layout changes (spec section 35:
        // no per-frame allocation in steady state).
        if (!m_hasPrevious || m_previous.size() != landmarks.size()) {
            m_previous = landmarks;
            m_speed.assign(landmarks.size(), Point2D{0.0f, 0.0f});
            m_hasPrevious = true;
            return; // first sample after (re)initialization is passed through
        }

        if (dtSeconds > 0.5) {
            // Gap: pass through, no smear across the discontinuity.
            m_previous = landmarks;
            m_speed.assign(landmarks.size(), Point2D{0.0f, 0.0f});
            return;
        }

        const double dt = clampDt(dtSeconds);
        const double te = std::max(1e-5, dt);
        const double dxDt = 1.0 / te;
        for (size_t i = 0; i < landmarks.size(); ++i) {
            Point2D& current = landmarks[i];
            const Point2D& previous = m_previous[i];

            // Speed estimate from raw samples.
            const float speedX = static_cast<float>((current.x - previous.x) * dxDt);
            const float speedY = static_cast<float>((current.y - previous.y) * dxDt);

            // Adaptive cutoff + alpha (One Euro: faster motion => higher
            // cutoff => larger alpha => less damping).
            const float cutoffX = m_minCutoff + m_beta * std::abs(speedX);
            const float cutoffY = m_minCutoff + m_beta * std::abs(speedY);
            const float alphaX =
                static_cast<float>(1.0 / (1.0 + 1.0 / (6.28318530717959 * cutoffX * te)));
            const float alphaY =
                static_cast<float>(1.0 / (1.0 + 1.0 / (6.28318530717959 * cutoffY * te)));

            m_speed[i].x = m_speed[i].x + alphaX * (speedX - m_speed[i].x);
            m_speed[i].y = m_speed[i].y + alphaY * (speedY - m_speed[i].y);

            const float smoothedX = previous.x + alphaX * (current.x - previous.x);
            const float smoothedY = previous.y + alphaY * (current.y - previous.y);

            m_previous[i] = Point2D{smoothedX, smoothedY};
            current = Point2D{smoothedX, smoothedY};
        }
    }

private:
    static double clampDt(double dt) {
        if (!(dt > 0.0)) return 1.0 / 30.0;      // first call / invalid
        return std::min(dt, 0.5);                 // after a gap: pass-through
    }

    float m_minCutoff = 1.2f;
    float m_beta = 0.007f;
    std::vector<Point2D> m_previous;
    std::vector<Point2D> m_speed;
    bool m_hasPrevious = false;
};

} // namespace haocam
