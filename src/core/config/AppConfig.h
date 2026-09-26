#pragma once

// HaoCam runtime configuration (config.json next to settings.json).
//
// Secrets (Facebetter app_id/app_key/license_token) live ONLY here and are
// never committed, never logged (spec sections 16/39). The repository ships
// config.example.json with empty placeholders.
//
// Example:
// {
//   "facebetter": {
//     "enabled": true,
//     "app_id": "",
//     "app_key": "",
//     "license_token": "",
//     "resource_path": ""
//   },
//   "tracking": { "enabled": true, "maxFps": 30 },
//   "beauty":   { "maxProcessFps": 30 }
// }

#include <filesystem>
#include <string>

#include "core/config/Json.h"

namespace haocam::core {

struct FacebetterSettings {
    bool enabled = false;
    std::string appId;
    std::string appKey;
    std::string licenseToken; // optional; takes priority when non-empty (SDK docs)
    std::string resourcePath; // optional override; default: sdk/facebetter/resource/resource.fbd

    bool hasCredentials() const {
        return !licenseToken.empty() || (!appId.empty() && !appKey.empty());
    }
};

struct TrackingSettings {
    bool enabled = true;
    float maxFps = 30.0f;
    std::string modelPath; // empty = default assets/models/face_landmarker.task
    float smoothingMinCutoff = 1.2f;
    float smoothingBeta = 0.007f;
};

struct AppConfig {
    FacebetterSettings facebetter;
    TrackingSettings tracking;
    float beautyMaxProcessFps = 30.0f;

    // Loads `<settingsDir>/config.json`. Missing file = defaults (Facebetter
    // disabled => honest Unavailable). Malformed file: logged, defaults used.
    static AppConfig load(const std::filesystem::path& settingsDir);

    // Writes a template config (used by config.example.json generation/tests).
    JsonValue toJson() const;
    static AppConfig fromJson(const JsonValue& json);
};

} // namespace haocam::core
