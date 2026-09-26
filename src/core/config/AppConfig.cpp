#include "core/config/AppConfig.h"

#include <fstream>

#include "core/logging/Logger.h"

namespace haocam::core {

namespace {
constexpr const char* kCategory = "config";
} // namespace

JsonValue AppConfig::toJson() const {
    JsonValue root = JsonValue::makeObject();

    JsonValue fb = JsonValue::makeObject();
    fb.set("enabled", JsonValue(facebetter.enabled));
    fb.set("app_id", JsonValue(facebetter.appId));
    fb.set("app_key", JsonValue(facebetter.appKey));
    fb.set("license_token", JsonValue(facebetter.licenseToken));
    fb.set("resource_path", JsonValue(facebetter.resourcePath));
    root.set("facebetter", std::move(fb));

    JsonValue trackingJson = JsonValue::makeObject();
    trackingJson.set("enabled", JsonValue(tracking.enabled));
    trackingJson.set("maxFps", JsonValue(static_cast<double>(tracking.maxFps)));
    trackingJson.set("modelPath", JsonValue(tracking.modelPath));
    trackingJson.set("smoothingMinCutoff",
                     JsonValue(static_cast<double>(tracking.smoothingMinCutoff)));
    trackingJson.set("smoothingBeta", JsonValue(static_cast<double>(tracking.smoothingBeta)));
    root.set("tracking", std::move(trackingJson));

    JsonValue beauty = JsonValue::makeObject();
    beauty.set("maxProcessFps", JsonValue(static_cast<double>(beautyMaxProcessFps)));
    root.set("beauty", std::move(beauty));

    return root;
}

AppConfig AppConfig::fromJson(const JsonValue& json) {
    AppConfig config;
    const JsonValue& fb = json.at("facebetter");
    config.facebetter.enabled = fb.at("enabled").asBool(false);
    config.facebetter.appId = fb.at("app_id").asString();
    config.facebetter.appKey = fb.at("app_key").asString();
    config.facebetter.licenseToken = fb.at("license_token").asString();
    config.facebetter.resourcePath = fb.at("resource_path").asString();

    const JsonValue& tracking = json.at("tracking");
    config.tracking.enabled = tracking.at("enabled").asBool(true);
    config.tracking.maxFps = static_cast<float>(tracking.at("maxFps").asNumber(30.0));
    config.tracking.modelPath = tracking.at("modelPath").asString();
    config.tracking.smoothingMinCutoff =
        static_cast<float>(tracking.at("smoothingMinCutoff").asNumber(1.2));
    config.tracking.smoothingBeta =
        static_cast<float>(tracking.at("smoothingBeta").asNumber(0.007));

    config.beautyMaxProcessFps =
        static_cast<float>(json.at("beauty").at("maxProcessFps").asNumber(30.0));
    return config;
}

AppConfig AppConfig::load(const std::filesystem::path& settingsDir) {
    AppConfig config;
    const auto path = settingsDir / "config.json";
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        HAOCAM_LOG_INFO(kCategory,
                        "No config.json ({}); optional SDK integrations disabled", path.string());
        return config;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        HAOCAM_LOG_WARN(kCategory, "Cannot open config.json: {}", path.string());
        return config;
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string error;
    JsonValue json = JsonValue::parse(text, &error);
    if (!json.isObject()) {
        HAOCAM_LOG_WARN(kCategory, "config.json invalid ({}); using defaults", error);
        return config;
    }
    config = fromJson(json);
    HAOCAM_LOG_INFO(kCategory,
                    "Loaded config.json (facebetter.enabled={}, credentials={}, "
                    "tracking.enabled={})",
                    config.facebetter.enabled,
                    config.facebetter.hasCredentials() ? "present" : "missing",
                    config.tracking.enabled);
    return config;
}

} // namespace haocam::core
