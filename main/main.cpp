#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <main/utils/logger.hpp>
#include <main/network/wifi_manager.hpp>
#include <main/network/mqtt_client.hpp>
#include <main/config/config.hpp>
#include <esp_timer.h>
#include <inttypes.h>

static void on_mqtt_message(const char* topic, const uint8_t* payload, int length)
{
    LOG_INFO("MAIN", "MQTT message: topic=%s len=%d", topic, length);
}

extern "C" void app_main(void)
{
    Logger::setLevel(LogLevel::INFO);
    LOG_INFO("MAIN", "%s", "---Didgital thermometer started---");

    WiFiManager wifi_manager;
    (void)wifi_manager.init(); // auto-connect based on Config::Wifi

    MqttClient mqtt_client; // uses Config::Mqtt and Config::Device
    (void)mqtt_client.init();
    mqtt_client.setMessageHandler(&on_mqtt_message);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t loop_period = pdMS_TO_TICKS(1000);
    bool had_ip = false;
    static uint32_t test_counter = 0;

    for (;;) {
        bool has_ip = wifi_manager.hasIp();
        if (has_ip && !had_ip) {
            LOG_INFO("MAIN", "%s", "WiFi connected and got IP");
            if (!mqtt_client.isConnected()) {
                if (mqtt_client.connect()) {
                    char cmd_topic[96];
                    snprintf(cmd_topic, sizeof(cmd_topic), "thermometer/%s/cmd", Config::Device::id);
                    (void)mqtt_client.subscribe(cmd_topic, Config::Mqtt::default_qos);
                    LOG_INFO("MAIN", "Subscribed to %s", cmd_topic);
                }
            }
        } else if (!has_ip && had_ip) {
            LOG_WARN("MAIN", "%s", "WiFi lost IP");
            if (mqtt_client.isConnected()) {
                mqtt_client.disconnect();
            }
        }
        had_ip = has_ip;

        // Publish test JSON once per loop if MQTT is connected
        if (mqtt_client.isConnected()) {
            char topic[96];
            snprintf(topic, sizeof(topic), "thermometer/%s/Test_Data", Config::Device::id);
            uint64_t now_us = esp_timer_get_time();
            uint32_t now_ms = static_cast<uint32_t>(now_us / 1000ULL);
            char payload[128];
            snprintf(payload, sizeof(payload), "{\"counter\":%" PRIu32 ",\"ts_ms\":%" PRIu32 "}", test_counter, now_ms);
            (void)mqtt_client.publish(topic, payload, Config::Mqtt::default_qos, Config::Mqtt::telemetry_retain);
            test_counter++;
        }

        vTaskDelayUntil(&last_wake_time, loop_period);
    }
}


