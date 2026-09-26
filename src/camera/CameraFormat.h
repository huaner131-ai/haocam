#pragma once

// Camera format model + deterministic best-format selection.
//
// HaoCam prefers 1920x1080 @ 60 FPS when the hardware offers it, falls back
// to the highest-area mode at >= 30 FPS, and supports 720p/1080p/1440p.

#include <cstdint>
#include <string>
#include <vector>

namespace haocam {

struct Resolution {
    uint32_t width = 0;
    uint32_t height = 0;

    uint64_t area() const { return static_cast<uint64_t>(width) * height; }
    bool operator==(const Resolution& other) const {
        return width == other.width && height == other.height;
    }
};

struct CameraFormatDesc {
    Resolution resolution;
    uint32_t fpsNumerator = 30;
    uint32_t fpsDenominator = 1;
    std::string pixelFormat = "NV12"; // Media Foundation FourCC, e.g. NV12, MJPG

    double fps() const {
        return fpsDenominator ? static_cast<double>(fpsNumerator) / fpsDenominator : 0.0;
    }
    bool operator==(const CameraFormatDesc& other) const {
        return resolution == other.resolution && fpsNumerator == other.fpsNumerator &&
               fpsDenominator == other.fpsDenominator && pixelFormat == other.pixelFormat;
    }
};

struct CameraFormatPreference {
    Resolution resolution{1920, 1080};
    double fps = 60.0;
};

// Scores formats and returns the best candidate (or nullptr for an empty
// list). Preference order:
//   1. Preferred resolution at >= preferred fps (ideally exact fps)
//   2. Preferred resolution at the highest fps
//   3. Same aspect ratio as preference, >= 720p, highest area
//   4. Highest area overall, tie-broken by fps
const CameraFormatDesc* pickBestFormat(const std::vector<CameraFormatDesc>& formats,
                                       const CameraFormatPreference& preference = {});

// Deduplicates modes that share resolution+fps, preferring uncompressed
// (NV12) over MJPG/H264 at equal score.
std::vector<CameraFormatDesc> dedupeFormats(const std::vector<CameraFormatDesc>& formats);

std::string describeFormat(const CameraFormatDesc& format);

} // namespace haocam
