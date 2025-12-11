#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdint>
#include <main/secrets.hpp>
#include <driver/gpio.h>
#include <hal/adc_types.h>

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
    // DFRobot Vibration Module - GPIO 21 is available on ESP32 Firebeetle
    static constexpr gpio_num_t vibration_module_gpio = GPIO_NUM_21;

    // 7-segment LED display (shared segments)
    static constexpr gpio_num_t led_seg_a = GPIO_NUM_14;
    static constexpr gpio_num_t led_seg_b = GPIO_NUM_27;
    static constexpr gpio_num_t led_seg_c = GPIO_NUM_26;
    static constexpr gpio_num_t led_seg_d = GPIO_NUM_25;
    static constexpr gpio_num_t led_seg_e = GPIO_NUM_33;
    static constexpr gpio_num_t led_seg_f = GPIO_NUM_32;
    static constexpr gpio_num_t led_seg_g = GPIO_NUM_13;
    static constexpr gpio_num_t led_seg_dp = GPIO_NUM_12;
    static constexpr gpio_num_t led_digit_left = GPIO_NUM_4;
    static constexpr gpio_num_t led_digit_right = GPIO_NUM_16;
    static constexpr bool led_common_anode = true;
    static constexpr uint8_t led_initial_brightness_percent = 80;
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

namespace Tasks {
namespace Temperature {
    static constexpr uint32_t period_ms = 1000;
}
namespace Moisture {
    static constexpr uint32_t period_ms = 1000;
}
namespace Display {
    static constexpr uint32_t refresh_slice_ms = 1; // multiplex update cadence
}
namespace Cloud {
    static constexpr uint32_t status_period_ms = 5000;
    static constexpr uint32_t reconnect_interval_ms = 30000;
}
}

// Feature toggles to enable/disable subsystems at build time
namespace Features {
    // Toggle tasks/subsystems on or off for focused testing
    static constexpr bool enable_cloud_comm      = true;
    static constexpr bool enable_temperature_task = true; // disabled for moisture-only testing
    static constexpr bool enable_moisture_task    = true;
    static constexpr bool enable_display_task     = false;
    static constexpr bool enable_alarm_task       = true;
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
}
}

#endif // CONFIG_HPP


