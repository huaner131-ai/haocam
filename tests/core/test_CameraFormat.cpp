#include <tuple>
#include <vector>

#include "camera/CameraFormat.h"
#include "test_main.h"

using haocam::CameraFormatDesc;
using haocam::CameraFormatPreference;
using haocam::pickBestFormat;

std::vector<CameraFormatDesc> modes(std::initializer_list<std::tuple<uint32_t, uint32_t, double, const char*>> list) {
    std::vector<CameraFormatDesc> result;
    for (const auto& [w, h, fps, pf] : list) {
        CameraFormatDesc f;
        f.resolution = {w, h};
        f.fpsNumerator = static_cast<uint32_t>(fps);
        f.fpsDenominator = 1;
        f.pixelFormat = pf;
        result.push_back(f);
    }
    return result;
}

HAOCAM_TEST(format_prefers_1080p60_nv12) {
    const auto formats = modes({
        {640, 480, 30.0, "YUY2"},
        {1280, 720, 30.0, "NV12"},
        {1280, 720, 60.0, "NV12"},
        {1920, 1080, 30.0, "NV12"},
        {1920, 1080, 60.0, "MJPG"},
        {1920, 1080, 60.0, "NV12"},
        {2560, 1440, 30.0, "NV12"},
    });
    const auto* best = pickBestFormat(formats);
    HAOCAM_EXPECT(best != nullptr);
    HAOCAM_EXPECT_EQ(best->resolution.width, 1920u);
    HAOCAM_EXPECT_EQ(best->resolution.height, 1080u);
    HAOCAM_EXPECT_EQ(best->fpsNumerator, 60u);
    HAOCAM_EXPECT_EQ(best->pixelFormat, std::string("NV12"));
}

HAOCAM_TEST(format_falls_back_to_720p60_when_no_1080p) {
    const auto formats = modes({
        {640, 480, 30.0, "NV12"},
        {1280, 720, 30.0, "NV12"},
        {1280, 720, 60.0, "NV12"},
    });
    const auto* best = pickBestFormat(formats);
    HAOCAM_EXPECT(best != nullptr);
    HAOCAM_EXPECT_EQ(best->resolution.height, 720u);
    HAOCAM_EXPECT_EQ(best->fpsNumerator, 60u);
}

HAOCAM_TEST(format_prefers_higher_area_when_no_common_mode) {
    const auto formats = modes({
        {640, 480, 60.0, "NV12"},
        {1600, 1200, 30.0, "NV12"},
        {320, 240, 30.0, "NV12"},
    });
    const auto* best = pickBestFormat(formats);
    HAOCAM_EXPECT(best != nullptr);
    HAOCAM_EXPECT_EQ(best->resolution.width, 1600u);
}

HAOCAM_TEST(format_empty_list_returns_null) {
    const std::vector<CameraFormatDesc> empty;
    HAOCAM_EXPECT(pickBestFormat(empty) == nullptr);
}

HAOCAM_TEST(format_dedupe_prefers_nv12) {
    const auto formats = modes({
        {1920, 1080, 60.0, "MJPG"},
        {1920, 1080, 60.0, "NV12"},
        {1280, 720, 30.0, "NV12"},
    });
    const auto deduped = haocam::dedupeFormats(formats);
    HAOCAM_EXPECT_EQ(deduped.size(), 2u);
    bool hasNv121080 = false;
    for (const auto& f : deduped) {
        if (f.resolution == haocam::Resolution{1920, 1080}) {
            hasNv121080 = f.pixelFormat == "NV12";
        }
    }
    HAOCAM_EXPECT(hasNv121080);
}

HAOCAM_TEST(format_description) {
    CameraFormatDesc f;
    f.resolution = {1920, 1080};
    f.fpsNumerator = 60;
    f.pixelFormat = "NV12";
    HAOCAM_EXPECT_EQ(haocam::describeFormat(f), std::string("1920x1080 @ 60 fps (NV12)"));
}
