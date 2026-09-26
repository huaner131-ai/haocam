#pragma once

// HaoCam logging facility.
//
// Every subsystem reports errors and status through this single logger so
// that failures (including unavailable SDKs) are never silently swallowed.
//
// Log format:
//   [2026-01-01 12:00:00.123] [INFO ] [camera    ] Camera initialized

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace haocam::core {

enum class LogLevel : int {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Off = 5,
};

std::string_view toString(LogLevel level);
LogLevel logLevelFromString(std::string_view name);

class Logger {
public:
    static Logger& instance();

    // Configures sinks. Safe to call once at startup from the main thread.
    void initialize(const std::filesystem::path& logDirectory,
                    LogLevel consoleLevel = LogLevel::Info,
                    LogLevel fileLevel = LogLevel::Debug);
    void shutdown();

    void setLevel(LogLevel level);
    LogLevel level() const { return m_level.load(std::memory_order_relaxed); }

    void log(LogLevel level, std::string_view category, std::string_view message);

    template <typename... Args>
    void log(LogLevel level, std::string_view category, std::string_view fmt, Args&&... args);

    bool logToFileEnabled() const { return m_file.is_open(); }
    const std::filesystem::path& filePath() const { return m_filePath; }

private:
    Logger() = default;
    ~Logger() = default;

    void writeLineLocked(LogLevel level, const std::string& line);

    std::mutex m_mutex;
    std::ofstream m_file;
    std::filesystem::path m_filePath;
    std::atomic<LogLevel> m_level{LogLevel::Info};
    LogLevel m_fileLevel = LogLevel::Debug;
    bool m_initialized = false;
};

// Formatter helpers (minimal, printf-free).
inline void formatMessage(std::string& out, std::string_view fmt) {
    out.append(fmt);
}

template <typename T, typename... Rest>
void formatMessage(std::string& out, std::string_view fmt, T&& value, Rest&&... rest) {
    const auto pos = fmt.find("{}");
    if (pos == std::string_view::npos) {
        out.append(fmt);
        return;
    }
    out.append(fmt.substr(0, pos));
    std::ostringstream oss;
    oss << value;
    out.append(oss.str());
    formatMessage(out, fmt.substr(pos + 2), std::forward<Rest>(rest)...);
}

template <typename... Args>
void Logger::log(LogLevel level, std::string_view category, std::string_view fmt,
                 Args&&... args) {
    const int consoleLevel = static_cast<int>(m_level.load(std::memory_order_relaxed));
    if (static_cast<int>(level) < consoleLevel &&
        static_cast<int>(level) < static_cast<int>(m_fileLevel)) {
        return; // below both the console and file thresholds
    }
    std::string message;
    formatMessage(message, fmt, std::forward<Args>(args)...);
    log(level, category, message);
}

// Convenience macros: HAOCAM_LOG_INFO("camera", "Camera initialized");
#define HAOCAM_LOG_TRACE(category, ...) \
    ::haocam::core::Logger::instance().log(::haocam::core::LogLevel::Trace, category, __VA_ARGS__)
#define HAOCAM_LOG_DEBUG(category, ...) \
    ::haocam::core::Logger::instance().log(::haocam::core::LogLevel::Debug, category, __VA_ARGS__)
#define HAOCAM_LOG_INFO(category, ...) \
    ::haocam::core::Logger::instance().log(::haocam::core::LogLevel::Info, category, __VA_ARGS__)
#define HAOCAM_LOG_WARN(category, ...) \
    ::haocam::core::Logger::instance().log(::haocam::core::LogLevel::Warning, category, __VA_ARGS__)
#define HAOCAM_LOG_ERROR(category, ...) \
    ::haocam::core::Logger::instance().log(::haocam::core::LogLevel::Error, category, __VA_ARGS__)

} // namespace haocam::core
