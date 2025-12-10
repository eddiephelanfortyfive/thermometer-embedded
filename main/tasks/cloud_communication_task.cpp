#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <main/utils/logger.hpp>
#include <main/network/wifi_manager.hpp>
#include <main/network/mqtt_client.hpp>
#include <main/utils/circular_buffer.hpp>
#include <main/config/config.hpp>
#include <main/models/sensor_data.hpp>
#include <main/models/alarm_event.hpp>
#include <main/models/command.hpp>
#include <main/models/moisture_data.hpp>
#include <inttypes.h>
#include <cstdio>

static const char* TAG = "CLOUD_TASK";

// Use shared models: SensorData, AlarmEvent, Command

namespace {
    // Static instances (no heap)
    static WiFiManager s_wifi_manager;
    static MqttClient s_mqtt_client;
    static CircularBuffer<SensorData, 512> s_telemetry_buffer;
    static bool s_post_connect_pending = false;

    // Task static stack and TCB
    static StaticTask_t s_task_tcb;
    static StackType_t s_task_stack[4096 / sizeof(StackType_t)];

    // Queues provided by main
    static QueueHandle_t s_sensor_queue = nullptr;
    static QueueHandle_t s_alarm_queue = nullptr;
    static QueueHandle_t s_command_queue = nullptr;
    static QueueHandle_t s_moisture_queue = nullptr;

    // MQTT message callback
    static void onMqttMessage(const char* topic, const uint8_t* payload, int length) {
        (void)topic;
        // Minimal forwarding: enqueue a placeholder Command for handler task
        if (s_command_queue != nullptr) {
            Command cmd{};
            cmd.timestamp_ms = static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
            cmd.type = 0;  // TODO: parse from JSON
            cmd.value = 0; // TODO: parse from JSON
            (void)xQueueSend(s_command_queue, &cmd, 0);
        }
        LOG_INFO(TAG, "MQTT RX topic=%s len=%d (forwarded)", topic, length);
    }

    static void taskFunction(void* parameters) {
        (void)parameters;
        LOG_INFO(TAG, "%s", "Cloud Communication Task started");

        // WiFi init and optional auto-connect handled in init()
        if (!s_wifi_manager.init()) {
            LOG_ERROR(TAG, "%s", "WiFi init failed");
            vTaskDelete(nullptr);
            return;
        }

        // Prepare MQTT
        (void)s_mqtt_client.init();
        s_mqtt_client.setMessageHandler(&onMqttMessage);

        TickType_t last_status_time = xTaskGetTickCount();
        const TickType_t status_period = pdMS_TO_TICKS(Config::Tasks::Cloud::status_period_ms);
        TickType_t last_reconnect_attempt = 0;
        const TickType_t reconnect_interval = pdMS_TO_TICKS(Config::Tasks::Cloud::reconnect_interval_ms);

        SensorData datum;
        AlarmEvent alarm_event;
        MoistureData moisture;

        for (;;) {
            TickType_t now = xTaskGetTickCount();
            bool has_ip = s_wifi_manager.hasIp();
            bool mqtt_ok = s_mqtt_client.isConnected();

            // WiFi reconnect
            if (!has_ip) {
                if ((now - last_reconnect_attempt) > reconnect_interval) {
                    (void)s_wifi_manager.reconnect();
                    last_reconnect_attempt = now;
                }
            }

            // MQTT connect (on IP)
            if (has_ip && !mqtt_ok) {
                if (s_mqtt_client.connect()) {
                    s_post_connect_pending = true; // wait until MQTT_EVENT_CONNECTED
                }
            }

            // Perform post-connect actions once connected
            if (s_mqtt_client.isConnected() && s_post_connect_pending) {
                char cmd_topic[96];
                std::snprintf(cmd_topic, sizeof(cmd_topic), "thermometer/%s/cmd", Config::Device::id);
                (void)s_mqtt_client.subscribe(cmd_topic, Config::Mqtt::default_qos);

                // Flush buffered data
                SensorData buffered;
                while (s_telemetry_buffer.pop(buffered)) {
                    char topic[96];
                    char payload[128];
                    std::snprintf(topic, sizeof(topic), "thermometer/%s/data", Config::Device::id);
                    std::snprintf(payload, sizeof(payload),
                                  "{\"tempC\":%.2f,\"ts_ms\":%" PRIu32 ",\"buffered\":1}",
                                  buffered.temp_c, buffered.ts_ms);
                    (void)s_mqtt_client.publish(topic, payload, Config::Mqtt::default_qos, Config::Mqtt::telemetry_retain);
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                s_post_connect_pending = false;
            }

            // Publish queued sensor data
            if (s_sensor_queue != nullptr) {
                if (xQueueReceive(s_sensor_queue, &datum, 0) == pdTRUE) {
                    if (s_mqtt_client.isConnected()) {
                        char topic[96];
                        char payload[128];
                        std::snprintf(topic, sizeof(topic), "thermometer/%s/data", Config::Device::id);
                        std::snprintf(payload, sizeof(payload),
                                      "{\"tempC\":%.2f,\"ts_ms\":%" PRIu32 "}",
                                      datum.temp_c, datum.ts_ms);
                        (void)s_mqtt_client.publish(topic, payload, Config::Mqtt::default_qos, Config::Mqtt::telemetry_retain);
                    } else {
                        (void)s_telemetry_buffer.push(datum);
                    }
                }
            }

            // Publish queued alarms
            if (s_alarm_queue != nullptr) {
                if (xQueueReceive(s_alarm_queue, &alarm_event, 0) == pdTRUE) {
                    if (s_mqtt_client.isConnected()) {
                        char topic[96];
                        char payload[160];
                        const char* type_str = "CLEARED";
                        if (alarm_event.type == 1) type_str = "HIGH_TEMPERATURE";
                        else if (alarm_event.type == 2) type_str = "LOW_TEMPERATURE";
                        std::snprintf(topic, sizeof(topic), "thermometer/%s/alarm", Config::Device::id);
                        std::snprintf(payload, sizeof(payload),
                                      "{\"temperature\":%.2f,\"timestamp\":%" PRIu32 ",\"type\":\"%s\"}",
                                      alarm_event.temperature_c, alarm_event.timestamp_ms, type_str);
                        (void)s_mqtt_client.publish(topic, payload, 1, false);
                    } else {
                        // TODO: optionally buffer alarms
                    }
                }
            }

            // Publish queued moisture data
            if (s_moisture_queue != nullptr) {
                if (xQueueReceive(s_moisture_queue, &moisture, 0) == pdTRUE) {
                    if (s_mqtt_client.isConnected()) {
                        char topic[96];
                        char payload[160];
                        std::snprintf(topic, sizeof(topic), "thermometer/%s/moisture", Config::Device::id);
                        std::snprintf(payload, sizeof(payload),
                                      "{\"raw\":%u,\"percent\":%.1f,\"ts_ms\":%" PRIu32 "}",
                                      static_cast<unsigned>(moisture.moisture_raw),
                                      static_cast<double>(moisture.moisture_percent),
                                      moisture.ts_ms);
                        (void)s_mqtt_client.publish(topic, payload, Config::Mqtt::default_qos, Config::Mqtt::telemetry_retain);
                    }
                }
            }

            // Periodic status
            if ((now - last_status_time) > status_period && s_mqtt_client.isConnected()) {
                char topic[96];
                char payload[160];
                std::snprintf(topic, sizeof(topic), "thermometer/%s/status", Config::Device::id);
                uint32_t uptime_ms = static_cast<uint32_t>(now * portTICK_PERIOD_MS);
                uint32_t buffered = static_cast<uint32_t>(s_telemetry_buffer.getCount());
                std::snprintf(payload, sizeof(payload),
                              "{\"status\":\"online\",\"uptime_ms\":%" PRIu32 ",\"buffered\":%" PRIu32 "}",
                              uptime_ms, buffered);
                (void)s_mqtt_client.publish(topic, payload, Config::Mqtt::default_qos, true);
                last_status_time = now;
            }

            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
} // namespace

namespace CloudCommunicationTask {
    void create(QueueHandle_t sensor_queue,
                QueueHandle_t alarm_queue,
                QueueHandle_t command_queue,
                QueueHandle_t moisture_queue) {
        s_sensor_queue = sensor_queue;
        s_alarm_queue = alarm_queue;
        s_command_queue = command_queue;
        s_moisture_queue = moisture_queue;
        xTaskCreateStatic(taskFunction,
                          "cloud_comm",
                          sizeof(s_task_stack) / sizeof(StackType_t),
                          nullptr,
                          1, // Low priority
                          s_task_stack,
                          &s_task_tcb);
    }
}


