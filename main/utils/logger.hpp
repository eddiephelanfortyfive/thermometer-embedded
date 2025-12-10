#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <cstdarg>
#include <esp_log.h>

// Fixed-size formatting buffer to avoid heap usage
#ifndef LOGGER_MAX_MESSAGE_LEN
#define LOGGER_MAX_MESSAGE_LEN 256
#endif

enum class LogLevel {
    ERROR = 0,
    WARN  = 1,
    INFO  = 2,
    DEBUG = 3
};

class Logger {
public:
    static void setLevel(LogLevel level);
    static LogLevel getLevel();

    static void error(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    static void warn(const char* tag, const char* fmt, ...)  __attribute__((format(printf, 2, 3)));
    static void info(const char* tag, const char* fmt, ...)  __attribute__((format(printf, 2, 3)));
    static void debug(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

    // Optional helper to align ESP-IDF internal log level for a tag
    static void setEspLogLevel(const char* tag, esp_log_level_t level);

private:
    static void logFormatted(esp_log_level_t esp_level, LogLevel gate_level, const char* tag, const char* fmt, va_list args);
    static LogLevel s_level;
};

// Convenience macros (no heap, fixed buffer)
#define LOG_ERROR(TAG, FMT, ...) Logger::error((TAG), (FMT), ##__VA_ARGS__)
#define LOG_WARN(TAG, FMT, ...)  Logger::warn((TAG),  (FMT), ##__VA_ARGS__)
#define LOG_INFO(TAG, FMT, ...)  Logger::info((TAG),  (FMT), ##__VA_ARGS__)
#define LOG_DEBUG(TAG, FMT, ...) Logger::debug((TAG), (FMT), ##__VA_ARGS__)

#endif // LOGGER_HPP


