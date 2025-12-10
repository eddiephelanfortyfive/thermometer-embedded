#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdint>
#include <main/secrets.hpp>

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


