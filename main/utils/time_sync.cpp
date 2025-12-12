#include <main/utils/time_sync.hpp>

#include <ctime>
#include <cstring>
#include <sys/time.h>
#include <esp_sntp.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
    static const char* TAG = "TIME_SYNC";
    static bool s_inited = false;

    static bool timeIsReasonable() {
        time_t now = 0;
        time(&now);
        // Consider any time on/after 2025-12-11 00:00:00 UTC as "reasonable"
        // Epoch: 1765411200
        return now >= 1765411200;
    }
}

namespace TimeSync {
    void init() {
        if (s_inited) {
            return;
        }
        // Basic SNTP config; poll mode with default pool servers
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        // Set explicit servers to improve reliability
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.google.com");
        esp_sntp_set_time_sync_notification_cb([](struct timeval*){
            ESP_LOGI(TAG, "SNTP time synchronized");
        });
        esp_sntp_init();
        s_inited = true;
        ESP_LOGI(TAG, "SNTP initialized");
    }

    bool isSynced() {
        if (!s_inited) {
            return false;
        }
        if (timeIsReasonable()) {
            return true;
        }
        sntp_sync_status_t st = sntp_get_sync_status();
        return st == SNTP_SYNC_STATUS_COMPLETED;
    }

    bool waitForSync(unsigned int timeout_ms) {
        if (!s_inited) {
            init();
        }
        ESP_LOGI(TAG, "Waiting for time sync (up to %u ms)...", timeout_ms);
        const unsigned int interval_ms = 100;
        unsigned int waited = 0;
        while (waited < timeout_ms) {
            if (isSynced()) {
                ESP_LOGI(TAG, "%s", "Time sync OK");
                return true;
            }
            vTaskDelay(pdMS_TO_TICKS(interval_ms));
            waited += interval_ms;
        }
        bool ok = isSynced();
        if (!ok) {
            ESP_LOGW(TAG, "%s", "Time sync timeout; timestamps may be zero until sync completes");
        }
        return ok;
    }

    void formatFixedTimestamp(char* out, std::size_t out_size) {
        if (out_size == 0) {
            return;
        }
        // Default to zeros if not synced (14 chars)
        if (!isSynced()) {
            const char* zero_ts = "00000000000000";
            std::strncpy(out, zero_ts, out_size - 1);
            out[out_size - 1] = '\0';
            return;
        }
        time_t now = 0;
        time(&now);
        struct tm tm_utc;
        gmtime_r(&now, &tm_utc);
        // Format as YYYYMMDDHHMMSS (14 chars)
        // strftime ensures null-termination if space permits
        (void)strftime(out, out_size, "%Y%m%d%H%M%S", &tm_utc);
    }
}


