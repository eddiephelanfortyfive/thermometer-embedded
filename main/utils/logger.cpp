#include <main/utils/logger.hpp>
#include <cstdio>

// Default log level
LogLevel Logger::s_level = LogLevel::INFO;

void Logger::setLevel(LogLevel level) {
    s_level = level;
}

LogLevel Logger::getLevel() {
    return s_level;
}

void Logger::setEspLogLevel(const char* tag, esp_log_level_t level) {
    esp_log_level_set(tag, level);
}

void Logger::logFormatted(esp_log_level_t esp_level, LogLevel gate_level, const char* tag, const char* fmt, va_list args) {
    if (static_cast<int>(s_level) < static_cast<int>(gate_level)) {
        return;
    }
    char buffer[LOGGER_MAX_MESSAGE_LEN];
    int n = vsnprintf(buffer, sizeof(buffer), fmt, args);
    if (n < 0) {
        // Formatting error; emit a minimal message without heap
        switch (esp_level) {
            case ESP_LOG_ERROR: ESP_LOGE(tag, "%s", "formatting error"); break;
            case ESP_LOG_WARN:  ESP_LOGW(tag, "%s", "formatting error"); break;
            case ESP_LOG_INFO:  ESP_LOGI(tag, "%s", "formatting error"); break;
            case ESP_LOG_DEBUG: ESP_LOGD(tag, "%s", "formatting error"); break;
            default:            ESP_LOGI(tag, "%s", "formatting error"); break;
        }
        return;
    }
    // Ensure null-terminated within fixed buffer
    buffer[sizeof(buffer) - 1] = '\0';

    switch (esp_level) {
        case ESP_LOG_ERROR: ESP_LOGE(tag, "%s", buffer); break;
        case ESP_LOG_WARN:  ESP_LOGW(tag, "%s", buffer); break;
        case ESP_LOG_INFO:  ESP_LOGI(tag, "%s", buffer); break;
        case ESP_LOG_DEBUG: ESP_LOGD(tag, "%s", buffer); break;
        default:            ESP_LOGI(tag, "%s", buffer); break;
    }
}

void Logger::error(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logFormatted(ESP_LOG_ERROR, LogLevel::ERROR, tag, fmt, args);
    va_end(args);
}

void Logger::warn(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logFormatted(ESP_LOG_WARN, LogLevel::WARN, tag, fmt, args);
    va_end(args);
}

void Logger::info(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logFormatted(ESP_LOG_INFO, LogLevel::INFO, tag, fmt, args);
    va_end(args);
}

void Logger::debug(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logFormatted(ESP_LOG_DEBUG, LogLevel::DEBUG, tag, fmt, args);
    va_end(args);
}


