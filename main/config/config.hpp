#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdint>
#include <main/secrets.hpp>
#include <driver/gpio.h>
#include <hal/adc_types.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace Config {
namespace Wifi {
    // Network credentials sourced from secrets.hpp (git-ignored)
    static constexpr const char* ssid = Secrets::WIFI_SSID;
    static constexpr const char* password = Secrets::WIFI_PASSWORD;

    // Behavior
    static constexpr bool auto_connect_on_start = true;
    static constexpr int max_retry_count = 5;           // Number of reconnect attempts before giving up (will still retry on next trigger)
    static constexpr uint32_t reconnect_backoff_ms = 1000; // Backoff between retries (basic)
}

namespace Device {
    static constexpr const char* id = Secrets::DEVICE_ID;
}

namespace Hardware {
namespace Pins {
    // GPIO assignments
    // LM35 uses ADC - GPIO 36 is ADC1_CH0 (input-only, perfect for analog sensor)
    static constexpr gpio_num_t temp_sensor_gpio = GPIO_NUM_36;
    // Alarm output (speaker) on a free GPIO
    static constexpr gpio_num_t vibration_module_gpio = GPIO_NUM_19;
} // namespace Pins

// Temperature sensor configuration (LM35/TMP36 compatibility)
namespace Temperature {
    // Gain (°C per mV) and additional °C offset after scaling.
    // Defaults: 0.1 °C/mV (LM35/TMP36), 0.0 °C offset.
    static constexpr float gain_c_per_mv = 0.1f;
}

// I2C 16x2 RGB LCD defaults (DFRobot Gravity DFR0464 class)
namespace Lcd {
    // Store as plain int to avoid pulling I2C headers into all translation units
    static constexpr int i2c_port = 0; // I2C_NUM_0
    static constexpr gpio_num_t sda = GPIO_NUM_21;
    static constexpr gpio_num_t scl = GPIO_NUM_22;
    static constexpr uint32_t clk_hz = 100000; // 100 kHz
    // Default I2C 7-bit addresses commonly used by DFRobot RGB LCD
    static constexpr uint8_t lcd_addr = 0x3E;  // LCD controller
    static constexpr uint8_t rgb_addr = 0x60;  // RGB backlight (PCA9633)
    static constexpr uint8_t backlight_r = 128;
    static constexpr uint8_t backlight_g = 128;
    static constexpr uint8_t backlight_b = 128;
}

namespace Moisture {
    // ADC configuration for soil moisture sensor
    static constexpr adc_unit_t unit = ADC_UNIT_1;
    static constexpr adc_channel_t channel = ADC_CHANNEL_6;   // GPIO34
    static constexpr adc_atten_t attenuation = ADC_ATTEN_DB_12; // up to ~3.3V (IDF v6)
    static constexpr uint8_t sample_count = 8;
    // Calibration endpoints (adjust in field)
    static constexpr uint16_t raw_dry = 0;
    static constexpr uint16_t raw_wet = 2700;
}
}

namespace Monitoring {
    // Temperature thresholds (Celsius)
    static constexpr float temp_low_warn_c   = 10.0f;
    static constexpr float temp_low_crit_c   = 5.0f;
    static constexpr float temp_high_warn_c  = 28.0f;
    static constexpr float temp_high_crit_c  = 32.0f;

    // Moisture thresholds (percent)
    static constexpr float moisture_low_warn_pct   = 35.0f;
    static constexpr float moisture_low_crit_pct   = 20.0f;
    static constexpr float moisture_high_warn_pct  = 80.0f;
    static constexpr float moisture_high_crit_pct  = 90.0f;

    // Debounce and hysteresis
    static constexpr uint32_t confirm_warn_ms = 5000;
    static constexpr uint32_t confirm_crit_ms = 3000;
    static constexpr float    clear_hysteresis_c   = 1.0f;
    static constexpr float    clear_hysteresis_pct = 2.0f;

    // Buzzer patterns
    static constexpr uint32_t warn_beep_ms  = 120;
    static constexpr uint32_t crit_on_ms    = 200;
    static constexpr uint32_t crit_off_ms   = 150;
    static constexpr uint32_t crit_repeat   = 3;
    static constexpr uint32_t crit_cycle_ms = 2000;
}

namespace Tasks {
namespace Temperature {
    static constexpr uint32_t period_ms = 1000;
}
namespace Moisture {
    static constexpr uint32_t period_ms = 1000;
}
namespace Cloud {
    static constexpr uint32_t status_period_ms = 5000;
    static constexpr uint32_t reconnect_interval_ms = 30000;
    // Telemetry throttling period (publish latest values at most this often)
    static constexpr uint32_t telemetry_period_ms = 5000;
}
}

// Feature toggles to enable/disable subsystems at build time
namespace Features {
    // Toggle tasks/subsystems on or off for focused testing
    static constexpr bool enable_cloud_comm      = true;
    static constexpr bool enable_temperature_task = true; // disabled for moisture-only testing
    static constexpr bool enable_moisture_task    = true;
    static constexpr bool enable_alarm_task       = true;
    static constexpr bool enable_lcd_task         = true; // off by default until wired on hardware
}

// Task priority levels (higher number = higher priority, can preempt lower)
namespace TaskPriorities {
    // Safety-critical: alarm must preempt all other tasks to respond immediately
    static constexpr UBaseType_t CRITICAL = tskIDLE_PRIORITY + 3;

    // Real-time: sensor sampling and control logic require deterministic timing
    static constexpr UBaseType_t HIGH     = tskIDLE_PRIORITY + 2;

    // Non-critical: UI feedback and network I/O can tolerate latency
    static constexpr UBaseType_t NORMAL   = tskIDLE_PRIORITY + 1;
}

namespace Mqtt {
    // Broker endpoint (from secrets)
    static constexpr const char* host = Secrets::MQTT_HOST;
    static constexpr int port = Secrets::MQTT_PORT;

    // Session behavior
    static constexpr bool clean_session = true;
    static constexpr uint16_t keepalive_seconds = 60;
    static constexpr int default_qos = 1;
    static constexpr bool telemetry_retain = false;

    // LWT
    static constexpr bool lwt_enable = true;
    static constexpr const char* lwt_prefix = "thermometer";

    // MQTT Topic Templates (use with device ID via snprintf)
    namespace Topics {
        static constexpr const char* TEMPERATURE = "thermometer/%s/temperature";
        static constexpr const char* MOISTURE = "thermometer/%s/moisture";
        static constexpr const char* ALERT = "thermometer/%s/alert";
        static constexpr const char* STATUS = "thermometer/%s/status";
        static constexpr const char* CMD = "thermometer/%s/cmd";
        static constexpr const char* THRESHOLDS_ACK = "thermometer/%s/thresholds-changed";
    }
}
}

#endif // CONFIG_HPP


