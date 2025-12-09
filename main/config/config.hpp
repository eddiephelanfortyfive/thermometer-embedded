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
}

#endif // CONFIG_HPP


