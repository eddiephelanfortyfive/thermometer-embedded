#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <main/utils/logger.hpp>
#include <main/network/wifi_manager.hpp>

extern "C" void app_main(void)
{
    Logger::setLevel(LogLevel::INFO);
    LOG_INFO("MAIN", "%s", "---Didgital thermometer started---");

    WiFiManager wifi_manager;
    (void)wifi_manager.init(); // auto-connect based on Config::Wifi

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t loop_period = pdMS_TO_TICKS(1000);
    bool had_ip = false;

    for (;;) {
        bool has_ip = wifi_manager.hasIp();
        if (has_ip && !had_ip) {
            LOG_INFO("MAIN", "%s", "WiFi connected and got IP");
        } else if (!has_ip && had_ip) {
            LOG_WARN("MAIN", "%s", "WiFi lost IP");
        }
        had_ip = has_ip;
        vTaskDelayUntil(&last_wake_time, loop_period);
    }
}


