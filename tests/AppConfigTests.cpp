// Phase 2: runtime configuration (spec sections 28/29 - credentials never
// logged, missing/malformed config = safe defaults).

#include <filesystem>
#include <string>

#include "core/config/AppConfig.h"
#include "test_main.h"

using namespace haocam;
using namespace haocam::core;

namespace {
int uniqueId() {
    static int id = 0;
    return ++id;
}

std::filesystem::path writeConfig(const std::string& text) {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("haocam-test-cfg-" + std::to_string(uniqueId()));
    std::filesystem::create_directories(dir);
    const auto file = dir / "config.json";
    FILE* f = fopen(file.string().c_str(), "wb");
    if (f) {
        fwrite(text.data(), 1, text.size(), f);
        fclose(f);
    }
    return dir;
}
} // namespace

HAOCAM_TEST(appconfig_defaults_when_file_missing) {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("haocam-test-cfg-missing-" + std::to_string(uniqueId()));
    std::filesystem::remove_all(dir); // ensure absent

    const AppConfig config = AppConfig::load(dir);
    HAOCAM_EXPECT(!config.facebetter.enabled);
    HAOCAM_EXPECT(!config.facebetter.hasCredentials());
    HAOCAM_EXPECT(config.facebetter.appId.empty());
    HAOCAM_EXPECT(config.facebetter.appKey.empty());
    HAOCAM_EXPECT(config.tracking.enabled);
    HAOCAM_EXPECT_EQ(config.tracking.maxFps, 30);
    HAOCAM_EXPECT_EQ(config.beautyMaxProcessFps, 30);
    std::filesystem::remove_all(dir);
}

HAOCAM_TEST(appconfig_parses_credentials_without_touching_them) {
    const auto dir = writeConfig(R"({
        "facebetter": {
            "enabled": true,
            "app_id": "id-123",
            "app_key": "key-456",
            "license_token": "tok-789"
        },
        "tracking": {
            "enabled": false,
            "maxFps": 24,
            "smoothingMinCutoff": 2.0,
            "smoothingBeta": 0.02
        },
        "beauty": {"maxProcessFps": 60}
    })");

    const AppConfig config = AppConfig::load(dir);
    HAOCAM_EXPECT(config.facebetter.enabled);
    HAOCAM_EXPECT(config.facebetter.hasCredentials());
    HAOCAM_EXPECT_EQ(config.facebetter.appId, std::string("id-123"));
    HAOCAM_EXPECT_EQ(config.facebetter.appKey, std::string("key-456"));
    HAOCAM_EXPECT_EQ(config.facebetter.licenseToken, std::string("tok-789"));
    HAOCAM_EXPECT(!config.tracking.enabled);
    HAOCAM_EXPECT_EQ(config.tracking.maxFps, 24);
    HAOCAM_EXPECT(config.tracking.smoothingMinCutoff == 2.0f);
    HAOCAM_EXPECT(config.tracking.smoothingBeta == 0.02f);
    HAOCAM_EXPECT_EQ(config.beautyMaxProcessFps, 60);
    std::filesystem::remove_all(dir);
}

HAOCAM_TEST(appconfig_malformed_file_falls_back_to_defaults) {
    const auto dir = writeConfig("{ not valid json !!");
    const AppConfig config = AppConfig::load(dir);
    HAOCAM_EXPECT(!config.facebetter.enabled);
    HAOCAM_EXPECT(config.tracking.enabled); // defaults survive
    std::filesystem::remove_all(dir);
}

HAOCAM_TEST(appconfig_empty_credentials_do_not_count_as_present) {
    FacebetterSettings settings;
    settings.appId = "";
    settings.appKey = "";
    HAOCAM_EXPECT(!settings.hasCredentials());
    settings.appId = "x";
    HAOCAM_EXPECT(!settings.hasCredentials());
    settings.appKey = "y";
    HAOCAM_EXPECT(settings.hasCredentials());
}
