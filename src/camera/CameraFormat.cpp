#include "camera/CameraFormat.h"

#include "camera/ICameraSource.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace haocam {

const char* toStringCameraState(CameraState state) {
    switch (state) {
        case CameraState::Idle: return "Idle";
        case CameraState::Starting: return "Starting";
        case CameraState::Running: return "Running";
        case CameraState::Reconnecting: return "Reconnecting";
        case CameraState::NoDevice: return "NoDevice";
        case CameraState::Failed: return "Failed";
    }
    return "Unknown";
}

namespace {

int resolutionRank(const Resolution& r) {
    if (r == Resolution{1920, 1080}) return 10;
    if (r == Resolution{2560, 1440}) return 9;
    if (r == Resolution{1280, 720}) return 8;
    if (r == Resolution{640, 480}) return 4;
    return 5;
}

double scoreFormat(const CameraFormatDesc& f, const CameraFormatPreference& p) {
    const double fps = f.fps();
    double score = 0.0;

    if (f.resolution == p.resolution) {
        score += 1000.0;
        const double fpsGap = std::abs(fps - p.fps);
        if (fps >= p.fps - 0.01) {
            score += 200.0 - fpsGap * 2.0; // meets target fps, prefer closest above
        } else {
            score += fps * 1.5; // below target fps: reward higher fps
        }
    } else {
        // Area proximity to the preferred resolution, log-scaled so 1440p
        // beats 480p clearly but 4K does not crush 1080p preferences.
        const double ratio = static_cast<double>(f.resolution.area()) /
                             static_cast<double>(p.resolution.area());
        const double areaPenalty = std::abs(std::log2(ratio > 0 ? ratio : 1e-9));
        score += std::max(0.0, 500.0 - areaPenalty * 250.0);
        score += static_cast<double>(resolutionRank(f.resolution)) * 5.0;
        score += std::min(fps, 60.0) * 2.0;
    }

    // Uncompressed is preferred over compressed at similar score.
    if (f.pixelFormat == "NV12") score += 30.0;
    return score;
}

} // namespace

const CameraFormatDesc* pickBestFormat(const std::vector<CameraFormatDesc>& formats,
                                       const CameraFormatPreference& preference) {
    const CameraFormatDesc* best = nullptr;
    double bestScore = -1.0;
    for (const auto& format : formats) {
        const double score = scoreFormat(format, preference);
        if (score > bestScore) {
            bestScore = score;
            best = &format;
        }
    }
    return best;
}

std::vector<CameraFormatDesc> dedupeFormats(const std::vector<CameraFormatDesc>& formats) {
    std::vector<CameraFormatDesc> result;
    for (const auto& format : formats) {
        bool merged = false;
        for (auto& existing : result) {
            if (existing.resolution == format.resolution &&
                existing.fpsNumerator == format.fpsNumerator &&
                existing.fpsDenominator == format.fpsDenominator) {
                // Prefer NV12 over compressed formats when both exist.
                if (format.pixelFormat == "NV12" && existing.pixelFormat != "NV12") {
                    existing.pixelFormat = "NV12";
                }
                merged = true;
                break;
            }
        }
        if (!merged) result.push_back(format);
    }
    std::sort(result.begin(), result.end(), [](const CameraFormatDesc& a, const CameraFormatDesc& b) {
        if (a.resolution.area() != b.resolution.area()) {
            return a.resolution.area() > b.resolution.area();
        }
        return a.fps() > b.fps();
    });
    return result;
}

std::string describeFormat(const CameraFormatDesc& format) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%ux%u @ %.0f fps (%s)", format.resolution.width,
                  format.resolution.height, format.fps(), format.pixelFormat.c_str());
    return buf;
}

} // namespace haocam
