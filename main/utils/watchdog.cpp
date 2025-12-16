#include <main/utils/watchdog.hpp>
#include <main/utils/logger.hpp>
#include <esp_task_wdt.h>

namespace {
    static const char* TAG = "WATCHDOG";
    static constexpr uint32_t TIMEOUT_MS = 8000;
}

namespace Watchdog {
    void init() {
        esp_task_wdt_config_t config = {
            .timeout_ms = TIMEOUT_MS,
            .idle_core_mask = 0,  // Don't monitor idle tasks
            .trigger_panic = true
        };
        esp_err_t err = esp_task_wdt_reconfigure(&config);
        if (err == ESP_OK) {
            LOG_INFO(TAG, "TWDT configured: %lu ms timeout", static_cast<unsigned long>(TIMEOUT_MS));
        } else {
            LOG_ERROR(TAG, "TWDT config failed: %d", static_cast<int>(err));
        }
    }

    void subscribe() {
        esp_err_t err = esp_task_wdt_add(nullptr);
        if (err != ESP_OK) {
            LOG_WARN(TAG, "TWDT subscribe failed: %d", static_cast<int>(err));
        }
    }

    void feed() {
        (void)esp_task_wdt_reset();
    }
}
