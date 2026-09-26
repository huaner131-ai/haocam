#include "core/settings/AppSettings.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "core/logging/Logger.h"

namespace haocam::core {

namespace {
constexpr const char* kCategory = "settings";

std::filesystem::path envOr(const char* name, const std::filesystem::path& fallback) {
    const char* value = std::getenv(name);
    return (value && *value) ? std::filesystem::path(value) : fallback;
}
} // namespace

std::filesystem::path AppSettings::defaultSettingsDirectory() {
    if (const char* overrideDir = std::getenv("HAOCAM_SETTINGS_DIR");
        overrideDir && *overrideDir) {
        return std::filesystem::path(overrideDir);
    }
#ifdef _WIN32
    return envOr("LOCALAPPDATA", envOr("APPDATA", ".")) / "HaoCam";
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        return std::filesystem::path(xdg) / "haocam";
    }
    return envOr("HOME", ".") / ".config" / "haocam";
#endif
}

AppSettings::AppSettings() : AppSettings(defaultSettingsDirectory()) {}

AppSettings::AppSettings(std::filesystem::path directory)
    : m_directory(std::move(directory)) {
    m_filePath = m_directory / "settings.json";
    m_root = JsonValue::makeObject();
}

bool AppSettings::load() {
    std::error_code ec;
    if (!std::filesystem::exists(m_filePath, ec)) {
        HAOCAM_LOG_INFO(kCategory, "No settings file yet, using defaults: {}", m_filePath.string());
        return false;
    }
    std::ifstream in(m_filePath, std::ios::binary);
    if (!in) {
        HAOCAM_LOG_WARN(kCategory, "Cannot open settings file: {}", m_filePath.string());
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    std::string error;
    JsonValue parsed = JsonValue::parse(buffer.str(), &error);
    if (parsed.isObject()) {
        m_root = std::move(parsed);
        HAOCAM_LOG_INFO(kCategory, "Loaded settings from {}", m_filePath.string());
        return true;
    }
    HAOCAM_LOG_WARN(kCategory, "Settings file is invalid ({}), using defaults", error);
    return false;
}

bool AppSettings::save() const {
    std::error_code ec;
    std::filesystem::create_directories(m_directory, ec);
    std::ofstream out(m_filePath, std::ios::binary | std::ios::trunc);
    if (!out) {
        HAOCAM_LOG_WARN(kCategory, "Cannot write settings file: {}", m_filePath.string());
        return false;
    }
    out << m_root.serialize(2) << '\n';
    return true;
}

std::string AppSettings::getString(std::string_view section, std::string_view key,
                                   std::string fallback) const {
    if (const JsonValue* s = m_root.find(section)) {
        if (const JsonValue* v = s->find(key)) return v->asString(std::move(fallback));
    }
    return fallback;
}

double AppSettings::getNumber(std::string_view section, std::string_view key,
                              double fallback) const {
    if (const JsonValue* s = m_root.find(section)) {
        if (const JsonValue* v = s->find(key)) return v->asNumber(fallback);
    }
    return fallback;
}

bool AppSettings::getBool(std::string_view section, std::string_view key,
                          bool fallback) const {
    if (const JsonValue* s = m_root.find(section)) {
        if (const JsonValue* v = s->find(key)) return v->asBool(fallback);
    }
    return fallback;
}

int AppSettings::getInt(std::string_view section, std::string_view key, int fallback) const {
    if (const JsonValue* s = m_root.find(section)) {
        if (const JsonValue* v = s->find(key)) return v->asInt(fallback);
    }
    return fallback;
}

void AppSettings::setString(std::string_view section, std::string_view key, std::string value) {
    m_root[std::string(section)].set(std::string(key), JsonValue(std::move(value)));
}

void AppSettings::setNumber(std::string_view section, std::string_view key, double value) {
    m_root[std::string(section)].set(std::string(key), JsonValue(value));
}

void AppSettings::setBool(std::string_view section, std::string_view key, bool value) {
    m_root[std::string(section)].set(std::string(key), JsonValue(value));
}

void AppSettings::setInt(std::string_view section, std::string_view key, int value) {
    m_root[std::string(section)].set(std::string(key), JsonValue(value));
}

JsonValue AppSettings::getObject(std::string_view section) const {
    if (const JsonValue* s = m_root.find(section)) return *s;
    return JsonValue::makeObject();
}

void AppSettings::setObject(std::string_view section, JsonValue value) {
    m_root.set(std::string(section), std::move(value));
}

} // namespace haocam::core
