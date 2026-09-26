#pragma once

// Persistent application settings backed by a portable JSON file.
//
// Default location:
//   Windows : %LOCALAPPDATA%/HaoCam/settings.json
//   Other   : $XDG_CONFIG_HOME/haocam/settings.json or ~/.config/haocam/...
// Override for tests / portable installs with HAOCAM_SETTINGS_DIR.

#include <filesystem>
#include <optional>
#include <string>

#include "core/config/Json.h"

namespace haocam::core {

class AppSettings {
public:
    AppSettings();

    // Explicit directory override (used by tests and portable mode).
    explicit AppSettings(std::filesystem::path directory);

    // Loads the settings file if present. Missing/corrupt files are reported
    // through the logger and replaced by defaults; load never throws.
    bool load();
    bool save() const;

    const std::filesystem::path& filePath() const { return m_filePath; }

    // Typed access to values stored under a section, e.g.
    //   settings.getString("camera", "deviceId", "")
    std::string getString(std::string_view section, std::string_view key,
                          std::string fallback = {}) const;
    double getNumber(std::string_view section, std::string_view key,
                     double fallback = 0.0) const;
    bool getBool(std::string_view section, std::string_view key,
                 bool fallback = false) const;
    int getInt(std::string_view section, std::string_view key, int fallback = 0) const;

    void setString(std::string_view section, std::string_view key, std::string value);
    void setNumber(std::string_view section, std::string_view key, double value);
    void setBool(std::string_view section, std::string_view key, bool value);
    void setInt(std::string_view section, std::string_view key, int value);

    // Sub-object replacement (e.g. a whole preset blob).
    JsonValue getObject(std::string_view section) const;
    void setObject(std::string_view section, JsonValue value);

    static std::filesystem::path defaultSettingsDirectory();

private:
    std::filesystem::path m_directory;
    std::filesystem::path m_filePath;
    JsonValue m_root;
};

} // namespace haocam::core
