#include <main/state/runtime_thresholds.hpp>
#include <main/config/config.hpp>
#include <main/utils/logger.hpp>
#include <nvs_flash.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <cstring>

static const char* TAG = "RUNTIME_THRESH";
static const char* NVS_NAMESPACE = "thresholds";

namespace {
    struct ThresholdData {
        float temp_low_warn_c;
        float temp_low_crit_c;
        float temp_high_warn_c;
        float temp_high_crit_c;
        float moisture_low_warn_pct;
        float moisture_low_crit_pct;
    };

    static ThresholdData s_data;
    static bool s_initialized = false;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
    static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
#else
    // For dual-core, use regular critical sections
#endif

    static void loadDefaults() {
        using namespace Config::Monitoring;
        s_data.temp_low_warn_c = temp_low_warn_c;
        s_data.temp_low_crit_c = temp_low_crit_c;
        s_data.temp_high_warn_c = temp_high_warn_c;
        s_data.temp_high_crit_c = temp_high_crit_c;
        s_data.moisture_low_warn_pct = moisture_low_warn_pct;
        s_data.moisture_low_crit_pct = moisture_low_crit_pct;
    }

    static bool loadFromNvs() {
        nvs_handle_t handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
        if (err != ESP_OK) {
            return false;
        }

        size_t required_size = sizeof(ThresholdData);
        err = nvs_get_blob(handle, "data", &s_data, &required_size);
        nvs_close(handle);

        if (err == ESP_OK && required_size == sizeof(ThresholdData)) {
            return true;
        }
        return false;
    }

    static bool saveToNvs() {
        nvs_handle_t handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err != ESP_OK) {
            LOG_ERROR(TAG, "NVS open failed: %d", static_cast<int>(err));
            return false;
        }

        err = nvs_set_blob(handle, "data", &s_data, sizeof(ThresholdData));
        if (err != ESP_OK) {
            LOG_ERROR(TAG, "NVS set_blob failed: %d", static_cast<int>(err));
            nvs_close(handle);
            return false;
        }

        err = nvs_commit(handle);
        nvs_close(handle);
        if (err != ESP_OK) {
            LOG_ERROR(TAG, "NVS commit failed: %d", static_cast<int>(err));
            return false;
        }

        return true;
    }
}

namespace RuntimeThresholds {
    void init() {
        if (s_initialized) {
            return;
        }

        loadDefaults();

        if (loadFromNvs()) {
            LOG_INFO(TAG, "Loaded thresholds from NVS");
        } else {
            LOG_INFO(TAG, "Using default thresholds (NVS not found or empty)");
            // Save defaults to NVS for next time
            (void)saveToNvs();
        }

        s_initialized = true;
    }

    float getTempLowWarn() {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        float val = s_data.temp_low_warn_c;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        return val;
    }

    float getTempLowCrit() {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        float val = s_data.temp_low_crit_c;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        return val;
    }

    float getTempHighWarn() {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        float val = s_data.temp_high_warn_c;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        return val;
    }

    float getTempHighCrit() {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        float val = s_data.temp_high_crit_c;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        return val;
    }

    float getMoistureLowWarn() {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        float val = s_data.moisture_low_warn_pct;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        return val;
    }

    float getMoistureLowCrit() {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        float val = s_data.moisture_low_crit_pct;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        return val;
    }

    bool setTempLowWarn(float value) {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        s_data.temp_low_warn_c = value;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        bool ok = saveToNvs();
        if (ok) {
            LOG_INFO(TAG, "Updated temp_low_warn to %.2f", value);
        }
        return ok;
    }

    bool setTempLowCrit(float value) {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        s_data.temp_low_crit_c = value;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        bool ok = saveToNvs();
        if (ok) {
            LOG_INFO(TAG, "Updated temp_low_crit to %.2f", value);
        }
        return ok;
    }

    bool setTempHighWarn(float value) {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        s_data.temp_high_warn_c = value;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        bool ok = saveToNvs();
        if (ok) {
            LOG_INFO(TAG, "Updated temp_high_warn to %.2f", value);
        }
        return ok;
    }

    bool setTempHighCrit(float value) {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        s_data.temp_high_crit_c = value;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        bool ok = saveToNvs();
        if (ok) {
            LOG_INFO(TAG, "Updated temp_high_crit to %.2f", value);
        }
        return ok;
    }

    bool setMoistureLowWarn(float value) {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        s_data.moisture_low_warn_pct = value;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        bool ok = saveToNvs();
        if (ok) {
            LOG_INFO(TAG, "Updated moisture_low_warn to %.1f", value);
        }
        return ok;
    }

    bool setMoistureLowCrit(float value) {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        s_data.moisture_low_crit_pct = value;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        bool ok = saveToNvs();
        if (ok) {
            LOG_INFO(TAG, "Updated moisture_low_crit to %.1f", value);
        }
        return ok;
    }
}
