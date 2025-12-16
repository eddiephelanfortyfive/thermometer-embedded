#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <main/utils/logger.hpp>
#include <main/network/wifi_manager.hpp>
#include <main/network/mqtt_client.hpp>
#include <main/utils/circular_buffer.hpp>
#include <main/config/config.hpp>
#include <main/models/temperature_data.hpp>
#include <main/models/alarm_event.hpp>
#include <main/models/command.hpp>
#include <main/models/moisture_data.hpp>
#include <inttypes.h>
#include <cstdio>
#include <cstring>
#include <main/utils/time_sync.hpp>
#include <main/state/device_state.hpp>
#include <cJSON.h>

static const char* TAG = "CLOUD_TASK";

// Use shared models: SensorData, AlarmEvent, Command

namespace {
    // Static instances (no heap)
    static WiFiManager s_wifi_manager;
    static MqttClient s_mqtt_client;
    static CircularBuffer<TemperatureData, 512> s_telemetry_buffer;
    static CircularBuffer<MoistureData, 512> s_moisture_buffer;
    static bool s_post_connect_pending = false;
    // Cache latest values for alerts
    static float s_last_temp_c = 0.0f;
    static float s_last_moisture_pct = 0.0f;
    static bool  s_have_temp = false;
    static bool  s_have_moist = false;

    // Task static stack and TCB
    static StaticTask_t s_task_tcb;
    static StackType_t s_task_stack[4096 / sizeof(StackType_t)];
    // Telemetry rate-limit
    static TickType_t s_last_temp_emit = 0;
    static TickType_t s_last_moist_emit = 0;

    // Queues provided by main (cloud-forwarded, latest-only)
    static QueueHandle_t s_temperature_mqtt_queue = nullptr;
    static QueueHandle_t s_alarm_queue = nullptr;
    static QueueHandle_t s_command_queue = nullptr;
    static QueueHandle_t s_moisture_mqtt_queue = nullptr;

    // Map threshold name string to command type
    static CommandType parseThresholdName(const char* name) {
        if (std::strcmp(name, "temp_low_warn") == 0) return CommandType::UPDATE_TEMP_LOW_WARN;
        if (std::strcmp(name, "temp_low_crit") == 0) return CommandType::UPDATE_TEMP_LOW_CRIT;
        if (std::strcmp(name, "temp_high_warn") == 0) return CommandType::UPDATE_TEMP_HIGH_WARN;
        if (std::strcmp(name, "temp_high_crit") == 0) return CommandType::UPDATE_TEMP_HIGH_CRIT;
        if (std::strcmp(name, "moisture_low_warn") == 0) return CommandType::UPDATE_MOISTURE_LOW_WARN;
        if (std::strcmp(name, "moisture_low_crit") == 0) return CommandType::UPDATE_MOISTURE_LOW_CRIT;
        if (std::strcmp(name, "moisture_high_warn") == 0) return CommandType::UPDATE_MOISTURE_HIGH_WARN;
        if (std::strcmp(name, "moisture_high_crit") == 0) return CommandType::UPDATE_MOISTURE_HIGH_CRIT;
        return static_cast<CommandType>(0); // Invalid
    }

    // MQTT message callback
    static void onMqttMessage(const char* topic, const uint8_t* payload, int length) {
        (void)topic;
        if (s_command_queue == nullptr || length <= 0 || length > 256) {
            LOG_WARN(TAG, "MQTT RX invalid: topic=%s len=%d", topic, length);
            return;
        }

        // Null-terminate payload for JSON parsing
        char json_buf[257];
        int copy_len = (length < 256) ? length : 256;
        std::memcpy(json_buf, payload, copy_len);
        json_buf[copy_len] = '\0';

        cJSON* json = cJSON_Parse(json_buf);
        if (json == nullptr) {
            LOG_WARN(TAG, "MQTT RX JSON parse failed: topic=%s", topic);
            return;
        }

        // Parse command structure: {"command": "update_threshold", "threshold": "...", "value": ...}
        cJSON* cmd_item = cJSON_GetObjectItem(json, "command");
        if (cmd_item == nullptr || !cJSON_IsString(cmd_item)) {
            cJSON_Delete(json);
            LOG_WARN(TAG, "MQTT RX missing/invalid 'command' field");
            return;
        }

        const char* cmd_str = cJSON_GetStringValue(cmd_item);
        
        // Handle single threshold update: {"command": "update_threshold", "threshold": "...", "value": ...}
        if (std::strcmp(cmd_str, "update_threshold") == 0) {
            cJSON* threshold_item = cJSON_GetObjectItem(json, "threshold");
            cJSON* value_item = cJSON_GetObjectItem(json, "value");
            if (threshold_item == nullptr || !cJSON_IsString(threshold_item) ||
                value_item == nullptr || !cJSON_IsNumber(value_item)) {
                cJSON_Delete(json);
                LOG_WARN(TAG, "MQTT RX missing/invalid 'threshold' or 'value' field");
                return;
            }

            const char* threshold_name = cJSON_GetStringValue(threshold_item);
            float threshold_value = static_cast<float>(cJSON_GetNumberValue(value_item));

            CommandType cmd_type = parseThresholdName(threshold_name);
            if (cmd_type == static_cast<CommandType>(0)) {
                cJSON_Delete(json);
                LOG_WARN(TAG, "MQTT RX unknown threshold: %s", threshold_name);
                return;
            }

            // Enqueue command for command task
            Command cmd{};
            cmd.timestamp_ms = static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
            cmd.type = static_cast<int32_t>(cmd_type);
            cmd.value = threshold_value;

            if (xQueueSend(s_command_queue, &cmd, 0) == pdTRUE) {
                LOG_INFO(TAG, "MQTT RX parsed: threshold=%s value=%.2f", threshold_name, threshold_value);
            } else {
                LOG_WARN(TAG, "MQTT RX queue full, dropped command");
            }
        }
        // Handle multiple threshold updates: {"command": "update_thresholds", "temp_high_crit": 30.0, "temp_high_warn": 25.0, ...}
        else if (std::strcmp(cmd_str, "update_thresholds") == 0) {
            int updated_count = 0;
            int failed_count = 0;
            
            // Iterate through all JSON items (skip "command" field)
            cJSON* item = json->child;
            while (item != nullptr) {
                // Skip the "command" field itself
                if (item->string != nullptr && std::strcmp(item->string, "command") == 0) {
                    item = item->next;
                    continue;
                }
                
                // Each other field should be a threshold name with a numeric value
                if (item->string != nullptr && cJSON_IsNumber(item)) {
                    const char* threshold_name = item->string;
                    float threshold_value = static_cast<float>(cJSON_GetNumberValue(item));
                    
                    CommandType cmd_type = parseThresholdName(threshold_name);
                    if (cmd_type != static_cast<CommandType>(0)) {
                        Command cmd{};
                        cmd.timestamp_ms = static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
                        cmd.type = static_cast<int32_t>(cmd_type);
                        cmd.value = threshold_value;
                        
                        if (xQueueSend(s_command_queue, &cmd, 0) == pdTRUE) {
                            updated_count++;
                            LOG_INFO(TAG, "MQTT RX parsed: threshold=%s value=%.2f", threshold_name, threshold_value);
                        } else {
                            failed_count++;
                            LOG_WARN(TAG, "MQTT RX queue full, dropped command: %s", threshold_name);
                        }
                    } else {
                        failed_count++;
                        LOG_WARN(TAG, "MQTT RX unknown threshold: %s", threshold_name);
                    }
                }
                
                item = item->next;
            }
            
            if (updated_count > 0) {
                LOG_INFO(TAG, "MQTT RX batch update: %d succeeded, %d failed", updated_count, failed_count);
            }
        }
        else {
            cJSON_Delete(json);
            LOG_WARN(TAG, "MQTT RX unknown command: %s", cmd_str);
            return;
        }

        cJSON_Delete(json);
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

        // Time sync flags
        bool time_inited = false;
        bool time_synced_once = false;

        TickType_t last_status_time = xTaskGetTickCount();
        const TickType_t status_period = pdMS_TO_TICKS(Config::Tasks::Cloud::status_period_ms);
        TickType_t last_reconnect_attempt = 0;
        const TickType_t reconnect_interval = pdMS_TO_TICKS(Config::Tasks::Cloud::reconnect_interval_ms);

        TemperatureData datum;
        AlarmEvent alarm_event;
        MoistureData moisture;

        for (;;) {
            TickType_t now = xTaskGetTickCount();
            bool has_ip = s_wifi_manager.hasIp();
            bool mqtt_ok = s_mqtt_client.isConnected();

            // Initialize SNTP when we have IP
            if (has_ip && !time_inited) {
                TimeSync::init();
                time_inited = true;
            }
            // Before first publishes after IP, wait briefly for time sync
            if (has_ip && time_inited && !time_synced_once) {
                (void)TimeSync::waitForSync(10000);
                time_synced_once = true;
            }

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
                TemperatureData buffered;
                while (s_telemetry_buffer.pop(buffered)) {
                    char topic[96];
                    char payload[160];
                    char ts[16];
                    TimeSync::formatFixedTimestamp(ts, sizeof(ts));
                    std::snprintf(topic, sizeof(topic), "thermometer/%s/temperature", Config::Device::id);
                    std::snprintf(payload, sizeof(payload),
                                  "{\"value\":%.2f,\"ts\":\"%s\",\"buffered\":1}",
                                  buffered.temp_c, ts);
                    (void)s_mqtt_client.publish(topic, payload, Config::Mqtt::default_qos, Config::Mqtt::telemetry_retain);
                    LOG_INFO(TAG, "MQTT TX topic=%s payload=%s", topic, payload);
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                // Flush buffered moisture data
                MoistureData mbuf;
                while (s_moisture_buffer.pop(mbuf)) {
                    char topic[96];
                    char payload[192];
                    char ts[16];
                    TimeSync::formatFixedTimestamp(ts, sizeof(ts));
                    std::snprintf(topic, sizeof(topic), "thermometer/%s/moisture", Config::Device::id);
                    std::snprintf(payload, sizeof(payload),
                                  "{\"percent\":%.1f,\"ts\":\"%s\",\"buffered\":1}",
                                  static_cast<double>(mbuf.moisture_percent), ts);
                    (void)s_mqtt_client.publish(topic, payload, Config::Mqtt::default_qos, Config::Mqtt::telemetry_retain);
                    LOG_INFO(TAG, "MQTT TX topic=%s payload=%s", topic, payload);
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                // Emit current alert snapshot once per reconnect
                {
                    auto st = DeviceStateMachine::get();
                    uint8_t rf = DeviceStateMachine::reasons();
                    const char* s_str = (st == DeviceStateMachine::DeviceState::CRITICAL) ? "CRITICAL"
                                       : (st == DeviceStateMachine::DeviceState::WARNING)  ? "WARNING"
                                                                                           : "OK";
                    char topic[96];
                    char payload[224];
                    char ts[16];
                    TimeSync::formatFixedTimestamp(ts, sizeof(ts));
                    std::snprintf(topic, sizeof(topic), "thermometer/%s/alert", Config::Device::id);
                    // Build reasons array string
                    char reasons_str[64] = {0};
                    bool first_reason = true;
                    auto append_reason = [&](const char* name) {
                        size_t cur = strlen(reasons_str);
                        if (!first_reason) {
                            std::snprintf(reasons_str + cur, sizeof(reasons_str) - cur, ",");
                            cur++;
                        }
                        std::snprintf(reasons_str + cur, sizeof(reasons_str) - cur, "\"%s\"", name);
                        first_reason = false;
                    };
                    if (rf & DeviceStateMachine::REASON_TEMP_HIGH)  append_reason("temp_high");
                    if (rf & DeviceStateMachine::REASON_TEMP_LOW)   append_reason("temp_low");
                    if (rf & DeviceStateMachine::REASON_MOIST_LOW)  append_reason("moisture_low");
                    if (rf & DeviceStateMachine::REASON_MOIST_HIGH) append_reason("moisture_high");
                    if (first_reason) {
                        std::snprintf(payload, sizeof(payload),
                                      "{\"state\":\"%s\",\"temp\":%.2f,\"moisture\":%.1f,\"ts\":\"%s\",\"snapshot\":1}",
                                      s_str, s_last_temp_c, s_last_moisture_pct, ts);
                    } else {
                        std::snprintf(payload, sizeof(payload),
                                      "{\"state\":\"%s\",\"reasons\":[%s],\"temp\":%.2f,\"moisture\":%.1f,\"ts\":\"%s\",\"snapshot\":1}",
                                      s_str, reasons_str, s_last_temp_c, s_last_moisture_pct, ts);
                    }
                    (void)s_mqtt_client.publish(topic, payload, Config::Mqtt::default_qos, false);
                    LOG_INFO(TAG, "MQTT TX topic=%s payload=%s", topic, payload);
                }
                s_post_connect_pending = false;
            }

            // Read latest temperature from forward queue (non-blocking)
            if (s_temperature_mqtt_queue != nullptr) {
                if (xQueueReceive(s_temperature_mqtt_queue, &datum, 0) == pdTRUE) {
                    s_last_temp_c = datum.temp_c;
                    s_have_temp = true;
                }
            }
            // Emit temperature at most every telemetry_period_ms
            if (s_have_temp) {
                const TickType_t telemetry_period = pdMS_TO_TICKS(Config::Tasks::Cloud::telemetry_period_ms);
                if ((now - s_last_temp_emit) >= telemetry_period) {
                    if (s_mqtt_client.isConnected()) {
                        char topic[96];
                        char payload[160];
                        char ts[16];
                        TimeSync::formatFixedTimestamp(ts, sizeof(ts));
                        std::snprintf(topic, sizeof(topic), "thermometer/%s/temperature", Config::Device::id);
                        std::snprintf(payload, sizeof(payload),
                                      "{\"value\":%.2f,\"ts\":\"%s\"}",
                                      s_last_temp_c, ts);
                        (void)s_mqtt_client.publish(topic, payload, Config::Mqtt::default_qos, Config::Mqtt::telemetry_retain);
                        LOG_INFO(TAG, "MQTT TX topic=%s payload=%s", topic, payload);
                    } else {
                        // Buffer latest temperature for post-connect flush
                        TemperatureData buffered{};
                        buffered.temp_c = s_last_temp_c;
                        buffered.ts_ms = static_cast<uint32_t>(now * portTICK_PERIOD_MS);
                        (void)s_telemetry_buffer.push(buffered);
                    }
                    s_last_temp_emit = now;
                }
            }

            // Drain alarm queue (no longer publish legacy alarm topic)
            if (s_alarm_queue != nullptr) {
                while (xQueueReceive(s_alarm_queue, &alarm_event, 0) == pdTRUE) {
                    // intentionally ignored for MQTT
                    (void)alarm_event;
                }
            }

            // Read latest moisture from forward queue (non-blocking)
            if (s_moisture_mqtt_queue != nullptr) {
                if (xQueueReceive(s_moisture_mqtt_queue, &moisture, 0) == pdTRUE) {
                    s_last_moisture_pct = moisture.moisture_percent;
                    s_have_moist = true;
                }
            }
            // Emit moisture at most every telemetry_period_ms
            if (s_have_moist) {
                const TickType_t telemetry_period = pdMS_TO_TICKS(Config::Tasks::Cloud::telemetry_period_ms);
                if ((now - s_last_moist_emit) >= telemetry_period) {
                    if (s_mqtt_client.isConnected()) {
                        char topic[96];
                        char payload[160];
                        char ts[16];
                        TimeSync::formatFixedTimestamp(ts, sizeof(ts));
                        std::snprintf(topic, sizeof(topic), "thermometer/%s/moisture", Config::Device::id);
                        std::snprintf(payload, sizeof(payload),
                                      "{\"percent\":%.1f,\"ts\":\"%s\"}",
                                      static_cast<double>(s_last_moisture_pct),
                                      ts);
                        (void)s_mqtt_client.publish(topic, payload, Config::Mqtt::default_qos, Config::Mqtt::telemetry_retain);
                        LOG_INFO(TAG, "MQTT TX topic=%s payload=%s", topic, payload);
                    } else {
                        // Buffer latest moisture for post-connect flush
                        MoistureData buffered{};
                        buffered.moisture_percent = s_last_moisture_pct;
                        buffered.moisture_raw = 0;
                        buffered.ts_ms = static_cast<uint32_t>(now * portTICK_PERIOD_MS);
                        (void)s_moisture_buffer.push(buffered);
                    }
                    s_last_moist_emit = now;
                }
            }

            // Handle internal alert publish requests via command queue:
            // Command.type = state (0 OK, 1 WARNING, 2 CRITICAL), Command.value = reason (0 CLEAR,1 TEMP_HIGH,2 TEMP_LOW,3 MOISTURE_LOW,4 MOISTURE_HIGH)
            // Only process internal commands (type >= 0); external commands (type < 0) are handled by command task
            if (s_command_queue != nullptr && s_mqtt_client.isConnected()) {
                Command cmd{};
                // Peek at queue without removing items, process only internal commands
                // Note: FreeRTOS doesn't have peek, so we receive and put back external commands
                int processed = 0;
                const int max_iterations = 16; // Limit iterations to avoid infinite loop
                while (processed < max_iterations && xQueueReceive(s_command_queue, &cmd, 0) == pdTRUE) {
                    // Skip external commands (negative type values) - put back for command task
                    if (cmd.type < 0) {
                        // Put it back at front so command task (blocking) will get it
                        (void)xQueueSendToFront(s_command_queue, &cmd, 0);
                        break; // Exit to avoid consuming all external commands
                    }
                    // Process internal command
                    int state = cmd.type;
                    int reason = static_cast<int>(cmd.value);
                    const char* s_str = (state == 2) ? "CRITICAL" : (state == 1) ? "WARNING" : "OK";
                    const char* r_str = (reason == 1) ? "temp_high" : (reason == 2) ? "temp_low"
                                         : (reason == 3) ? "moisture_low" : (reason == 4) ? "moisture_high" : "clear";
                    char topic[96];
                    char payload[192];
                    char ts[16];
                    TimeSync::formatFixedTimestamp(ts, sizeof(ts));
                    std::snprintf(topic, sizeof(topic), "thermometer/%s/alert", Config::Device::id);
                    std::snprintf(payload, sizeof(payload),
                                  "{\"state\":\"%s\",\"reason\":\"%s\",\"temp\":%.2f,\"moisture\":%.1f,\"ts\":\"%s\"}",
                                  s_str, r_str, s_last_temp_c, s_last_moisture_pct, ts);
                    (void)s_mqtt_client.publish(topic, payload, Config::Mqtt::default_qos, false);
                    LOG_INFO(TAG, "MQTT TX topic=%s payload=%s", topic, payload);
                    processed++;
                }
            }

            // Periodic status
            if ((now - last_status_time) > status_period && s_mqtt_client.isConnected()) {
                char topic[96];
                char payload[256];
                std::snprintf(topic, sizeof(topic), "thermometer/%s/status", Config::Device::id);
                uint32_t uptime_ms = static_cast<uint32_t>(now * portTICK_PERIOD_MS);
                uint32_t buffered_temp = static_cast<uint32_t>(s_telemetry_buffer.getCount());
                uint32_t buffered_moist = static_cast<uint32_t>(s_moisture_buffer.getCount());
                uint32_t buffered_total = buffered_temp + buffered_moist;
                // Read device state
                auto st = DeviceStateMachine::get();
                const char* st_str = (st == DeviceStateMachine::DeviceState::CRITICAL) ? "CRITICAL"
                                    : (st == DeviceStateMachine::DeviceState::WARNING) ? "WARNING"
                                    : "OK";
                uint8_t rf = DeviceStateMachine::reasons();
                // Build reasons array string (compact)
                char reasons_str[64] = {0};
                bool first = true;
                reasons_str[0] = '\0';
                auto append_reason = [&](const char* name) {
                    size_t cur = strlen(reasons_str);
                    if (!first) {
                        std::snprintf(reasons_str + cur, sizeof(reasons_str) - cur, ",");
                        cur++;
                    }
                    std::snprintf(reasons_str + cur, sizeof(reasons_str) - cur, "\"%s\"", name);
                    first = false;
                };
                if (rf & DeviceStateMachine::REASON_TEMP_HIGH)  append_reason("temp_high");
                if (rf & DeviceStateMachine::REASON_TEMP_LOW)   append_reason("temp_low");
                if (rf & DeviceStateMachine::REASON_MOIST_LOW)  append_reason("moisture_low");
                if (rf & DeviceStateMachine::REASON_MOIST_HIGH) append_reason("moisture_high");
                if (first) {
                    // No reasons
                    reasons_str[0] = '\0';
                }
                if (first) {
                    std::snprintf(payload, sizeof(payload),
                                  "{\"status\":\"online\",\"uptime_ms\":%" PRIu32 ",\"buffered\":%" PRIu32 ",\"buffered_temp\":%" PRIu32 ",\"buffered_moist\":%" PRIu32 ",\"state\":\"%s\"}",
                                  uptime_ms, buffered_total, buffered_temp, buffered_moist, st_str);
                } else {
                    std::snprintf(payload, sizeof(payload),
                                  "{\"status\":\"online\",\"uptime_ms\":%" PRIu32 ",\"buffered\":%" PRIu32 ",\"buffered_temp\":%" PRIu32 ",\"buffered_moist\":%" PRIu32 ",\"state\":\"%s\",\"reasons\":[%s]}",
                                  uptime_ms, buffered_total, buffered_temp, buffered_moist, st_str, reasons_str);
                }
                (void)s_mqtt_client.publish(topic, payload, Config::Mqtt::default_qos, true);
                LOG_INFO(TAG, "MQTT TX topic=%s payload=%s", topic, payload);
                last_status_time = now;
            }

            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
} // namespace

namespace CloudCommunicationTask {
    void create(QueueHandle_t temperature_mqtt_queue,
                QueueHandle_t alarm_queue,
                QueueHandle_t command_queue,
                QueueHandle_t moisture_mqtt_queue) {
        s_temperature_mqtt_queue = temperature_mqtt_queue;
        s_alarm_queue = alarm_queue;
        s_command_queue = command_queue;
        s_moisture_mqtt_queue = moisture_mqtt_queue;
        xTaskCreateStatic(taskFunction,
                          "cloud_comm",
                          sizeof(s_task_stack) / sizeof(StackType_t),
                          nullptr,
                          Config::TaskPriorities::NORMAL,
                          s_task_stack,
                          &s_task_tcb);
    }
}


