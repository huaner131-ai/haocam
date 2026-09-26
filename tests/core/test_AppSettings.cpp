#include <filesystem>
#include <fstream>

#include "core/settings/AppSettings.h"
#include <chrono>
#include "test_main.h"

namespace {
std::filesystem::path tempDir() {
    auto dir = std::filesystem::temp_directory_path() /
               ("haocam-tests-" + std::to_string(std::chrono::steady_clock::now()
                                                     .time_since_epoch()
                                                     .count()));
    std::filesystem::create_directories(dir);
    return dir;
}
} // namespace

HAOCAM_TEST(settings_roundtrip) {
    const auto dir = tempDir();
    {
        haocam::core::AppSettings settings(dir);
        settings.setString("camera", "deviceId", "usb#vid_1234");
        settings.setBool("camera", "mirror", true);
        settings.setNumber("filters", "brightness", 0.25);
        settings.setInt("general", "logLevel", 2);
        HAOCAM_EXPECT(settings.save());
    }
    {
        haocam::core::AppSettings loaded(dir);
        HAOCAM_EXPECT(loaded.load());
        HAOCAM_EXPECT_EQ(loaded.getString("camera", "deviceId"), "usb#vid_1234");
        HAOCAM_EXPECT(loaded.getBool("camera", "mirror"));
        HAOCAM_EXPECT(loaded.getNumber("filters", "brightness") > 0.24);
        HAOCAM_EXPECT_EQ(loaded.getInt("general", "logLevel"), 2);
        HAOCAM_EXPECT_EQ(loaded.getString("missing", "key", "fallback"), "fallback");
    }
    std::filesystem::remove_all(dir);
}

HAOCAM_TEST(settings_missing_file_uses_defaults) {
    const auto dir = tempDir();
    haocam::core::AppSettings settings(dir);
    HAOCAM_EXPECT(!settings.load()); // not present yet
    HAOCAM_EXPECT_EQ(settings.getBool("diagnostics", "overlayVisible", true), true);
    std::filesystem::remove_all(dir);
}

HAOCAM_TEST(settings_corrupt_file_falls_back) {
    const auto dir = tempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream out(dir / "settings.json", std::ios::binary);
        out << "{ this is not json";
    }
    haocam::core::AppSettings settings(dir);
    HAOCAM_EXPECT(!settings.load());
    HAOCAM_EXPECT_EQ(settings.getString("camera", "deviceId", "default"), "default");
    std::filesystem::remove_all(dir);
}
