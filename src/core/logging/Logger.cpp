#include "core/logging/Logger.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <ctime>
#include <iomanip>

namespace haocam::core {

namespace {

LogLevel sanitizeLevel(LogLevel level) {
    if (level < LogLevel::Trace) return LogLevel::Trace;
    if (level > LogLevel::Off) return LogLevel::Off;
    return level;
}

std::string timestampString() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t t = clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()).count() % 1000;

    std::tm tmValue{};
#ifdef _WIN32
    localtime_s(&tmValue, &t);
#else
    localtime_r(&t, &tmValue);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmValue, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3)
        << std::setfill('0') << ms;
    return oss.str();
}

} // namespace

std::string_view toString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Off: return "OFF  ";
    }
    return "?????";
}

LogLevel logLevelFromString(std::string_view name) {
    if (name == "trace" || name == "TRACE") return LogLevel::Trace;
    if (name == "debug" || name == "DEBUG") return LogLevel::Debug;
    if (name == "info" || name == "INFO") return LogLevel::Info;
    if (name == "warning" || name == "warn" || name == "WARN") return LogLevel::Warning;
    if (name == "error" || name == "ERROR") return LogLevel::Error;
    if (name == "off" || name == "OFF") return LogLevel::Off;
    return LogLevel::Info;
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::initialize(const std::filesystem::path& logDirectory,
                        LogLevel consoleLevel, LogLevel fileLevel) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_level.store(sanitizeLevel(consoleLevel), std::memory_order_relaxed);
    m_fileLevel = sanitizeLevel(fileLevel);

    std::error_code ec;
    std::filesystem::create_directories(logDirectory, ec);
    m_filePath = logDirectory / "haocam.log";
    m_file.open(m_filePath, std::ios::out | std::ios::app);
    m_initialized = true;

    std::string line;
    line.reserve(128);
    line += '[';
    line += timestampString();
    line += "] [INFO ] [log       ] HaoCam logger initialized";
    if (!m_file) {
        line += " (file logging unavailable: ";
        line += m_filePath.string();
        line += ")";
    } else {
        line += " -> ";
        line += m_filePath.string();
    }
    writeLineLocked(LogLevel::Info, line);
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }
    m_initialized = false;
}

void Logger::setLevel(LogLevel level) {
    m_level.store(sanitizeLevel(level), std::memory_order_relaxed);
}

void Logger::log(LogLevel level, std::string_view category, std::string_view message) {
    if (level == LogLevel::Off) return;
    const LogLevel current = m_level.load(std::memory_order_relaxed);
    if (static_cast<int>(level) < static_cast<int>(current)) return;

    std::string line;
    line.reserve(message.size() + 64);
    line += '[';
    line += timestampString();
    line += "] [";
    line += toString(level);
    line += "] [";
    if (category.size() >= 10) {
        line.append(category.substr(0, 10));
    } else {
        line.append(category);
        line.append(10 - category.size(), ' ');
    }
    line += "] ";
    line.append(message);

    std::lock_guard<std::mutex> lock(m_mutex);
    writeLineLocked(level, line);
}

void Logger::writeLineLocked(LogLevel level, const std::string& line) {
#ifdef _WIN32
    OutputDebugStringA((line + "\n").c_str());
#endif
    // Console output for debugging sessions (harmless in windowed apps).
    std::fputs((line + "\n").c_str(),
               static_cast<int>(level) >= static_cast<int>(LogLevel::Warning) ? stderr
                                                                              : stdout);
    if (m_file.is_open() &&
        static_cast<int>(level) >= static_cast<int>(m_fileLevel)) {
        m_file << line << '\n';
        m_file.flush();
    }
}

} // namespace haocam::core
