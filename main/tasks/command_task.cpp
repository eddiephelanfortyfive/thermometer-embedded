#include <main/tasks/command_task.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <main/utils/logger.hpp>
#include <main/models/command.hpp>
#include <main/config/config.hpp>
#include <main/state/runtime_thresholds.hpp>
#include <main/models/cloud_publish_request.hpp>
#include <main/utils/time_sync.hpp>
#include <main/utils/third-party/mjson.h>
#include <inttypes.h>
#include <cstring>
#include <cstdio>

static const char* TAG = "CMD_TASK";

namespace {
    static StaticTask_t s_task_tcb;
    static StackType_t s_task_stack[4096 / sizeof(StackType_t)];

    static QueueHandle_t s_command_queue = nullptr;
    static QueueHandle_t s_thresholds_changed_queue = nullptr;

    // Validate threshold value ranges
    static bool validateTempThreshold(float value) {
        // Reasonable range: -50 to 100 degrees Celsius
        return (value >= -50.0f && value <= 100.0f);
    }

    static bool validateMoistureThreshold(float value) {
        // Reasonable range: 0 to 100 percent
        return (value >= 0.0f && value <= 100.0f);
    }

    static void handleCommand(const Command& cmd) {
        CommandType cmd_type = static_cast<CommandType>(cmd.type);

        // Only process external commands (negative type values)
        // Internal commands (non-negative) are handled by cloud task
        if (cmd.type >= 0) {
            return; // Not our command, ignore
        }

        bool success = false;
        const char* threshold_name = nullptr;

        switch (cmd_type) {
            case CommandType::UPDATE_TEMP_LOW_WARN:
                if (validateTempThreshold(cmd.value)) {
                    success = RuntimeThresholds::setTempLowWarn(cmd.value);
                    threshold_name = "temp_low_warn";
                } else {
                    LOG_ERROR(TAG, "Invalid temp_low_warn value: %.2f", cmd.value);
                }
                break;

            case CommandType::UPDATE_TEMP_LOW_CRIT:
                if (validateTempThreshold(cmd.value)) {
                    success = RuntimeThresholds::setTempLowCrit(cmd.value);
                    threshold_name = "temp_low_crit";
                } else {
                    LOG_ERROR(TAG, "Invalid temp_low_crit value: %.2f", cmd.value);
                }
                break;

            case CommandType::UPDATE_TEMP_HIGH_WARN:
                if (validateTempThreshold(cmd.value)) {
                    success = RuntimeThresholds::setTempHighWarn(cmd.value);
                    threshold_name = "temp_high_warn";
                } else {
                    LOG_ERROR(TAG, "Invalid temp_high_warn value: %.2f", cmd.value);
                }
                break;

            case CommandType::UPDATE_TEMP_HIGH_CRIT:
                if (validateTempThreshold(cmd.value)) {
                    success = RuntimeThresholds::setTempHighCrit(cmd.value);
                    threshold_name = "temp_high_crit";
                } else {
                    LOG_ERROR(TAG, "Invalid temp_high_crit value: %.2f", cmd.value);
                }
                break;

            case CommandType::UPDATE_MOISTURE_LOW_WARN:
                if (validateMoistureThreshold(cmd.value)) {
                    success = RuntimeThresholds::setMoistureLowWarn(cmd.value);
                    threshold_name = "moisture_low_warn";
                } else {
                    LOG_ERROR(TAG, "Invalid moisture_low_warn value: %.2f", cmd.value);
                }
                break;

            case CommandType::UPDATE_MOISTURE_LOW_CRIT:
                if (validateMoistureThreshold(cmd.value)) {
                    success = RuntimeThresholds::setMoistureLowCrit(cmd.value);
                    threshold_name = "moisture_low_crit";
                } else {
                    LOG_ERROR(TAG, "Invalid moisture_low_crit value: %.2f", cmd.value);
                }
                break;

            case CommandType::UPDATE_MOISTURE_HIGH_WARN:
                if (validateMoistureThreshold(cmd.value)) {
                    success = RuntimeThresholds::setMoistureHighWarn(cmd.value);
                    threshold_name = "moisture_high_warn";
                } else {
                    LOG_ERROR(TAG, "Invalid moisture_high_warn value: %.2f", cmd.value);
                }
                break;

            case CommandType::UPDATE_MOISTURE_HIGH_CRIT:
                if (validateMoistureThreshold(cmd.value)) {
                    success = RuntimeThresholds::setMoistureHighCrit(cmd.value);
                    threshold_name = "moisture_high_crit";
                } else {
                    LOG_ERROR(TAG, "Invalid moisture_high_crit value: %.2f", cmd.value);
                }
                break;

            default:
                LOG_WARN(TAG, "Unknown command type: %" PRId32, cmd.type);
                return;
        }

        if (success && threshold_name != nullptr) {
            LOG_INFO(TAG, "Command executed: %s = %.2f", threshold_name, cmd.value);
        } else if (threshold_name != nullptr) {
            LOG_ERROR(TAG, "Command failed: %s = %.2f", threshold_name, cmd.value);
        }
    }

    struct ThresholdChanges {
        bool temp_low_warn = false;
        bool temp_low_crit = false;
        bool temp_high_warn = false;
        bool temp_high_crit = false;
        bool moisture_low_warn = false;
        bool moisture_low_crit = false;
        bool moisture_high_warn = false;
        bool moisture_high_crit = false;

        float v_temp_low_warn = 0.0f;
        float v_temp_low_crit = 0.0f;
        float v_temp_high_warn = 0.0f;
        float v_temp_high_crit = 0.0f;
        float v_moisture_low_warn = 0.0f;
        float v_moisture_low_crit = 0.0f;
        float v_moisture_high_warn = 0.0f;
        float v_moisture_high_crit = 0.0f;
    };

    static bool applyAndRecordChange(const Command& cmd, ThresholdChanges& changes) {
        CommandType t = static_cast<CommandType>(cmd.type);
        bool ok = false;
        switch (t) {
            case CommandType::UPDATE_TEMP_LOW_WARN:
                if (validateTempThreshold(cmd.value)) {
                    ok = RuntimeThresholds::setTempLowWarn(cmd.value);
                    if (ok) { changes.temp_low_warn = true; changes.v_temp_low_warn = cmd.value; }
                } else {
                    LOG_ERROR(TAG, "Invalid temp_low_warn value: %.2f", cmd.value);
                }
                break;
            case CommandType::UPDATE_TEMP_LOW_CRIT:
                if (validateTempThreshold(cmd.value)) {
                    ok = RuntimeThresholds::setTempLowCrit(cmd.value);
                    if (ok) { changes.temp_low_crit = true; changes.v_temp_low_crit = cmd.value; }
                } else {
                    LOG_ERROR(TAG, "Invalid temp_low_crit value: %.2f", cmd.value);
                }
                break;
            case CommandType::UPDATE_TEMP_HIGH_WARN:
                if (validateTempThreshold(cmd.value)) {
                    ok = RuntimeThresholds::setTempHighWarn(cmd.value);
                    if (ok) { changes.temp_high_warn = true; changes.v_temp_high_warn = cmd.value; }
                } else {
                    LOG_ERROR(TAG, "Invalid temp_high_warn value: %.2f", cmd.value);
                }
                break;
            case CommandType::UPDATE_TEMP_HIGH_CRIT:
                if (validateTempThreshold(cmd.value)) {
                    ok = RuntimeThresholds::setTempHighCrit(cmd.value);
                    if (ok) { changes.temp_high_crit = true; changes.v_temp_high_crit = cmd.value; }
                } else {
                    LOG_ERROR(TAG, "Invalid temp_high_crit value: %.2f", cmd.value);
                }
                break;
            case CommandType::UPDATE_MOISTURE_LOW_WARN:
                if (validateMoistureThreshold(cmd.value)) {
                    ok = RuntimeThresholds::setMoistureLowWarn(cmd.value);
                    if (ok) { changes.moisture_low_warn = true; changes.v_moisture_low_warn = cmd.value; }
                } else {
                    LOG_ERROR(TAG, "Invalid moisture_low_warn value: %.2f", cmd.value);
                }
                break;
            case CommandType::UPDATE_MOISTURE_LOW_CRIT:
                if (validateMoistureThreshold(cmd.value)) {
                    ok = RuntimeThresholds::setMoistureLowCrit(cmd.value);
                    if (ok) { changes.moisture_low_crit = true; changes.v_moisture_low_crit = cmd.value; }
                } else {
                    LOG_ERROR(TAG, "Invalid moisture_low_crit value: %.2f", cmd.value);
                }
                break;
            case CommandType::UPDATE_MOISTURE_HIGH_WARN:
                if (validateMoistureThreshold(cmd.value)) {
                    ok = RuntimeThresholds::setMoistureHighWarn(cmd.value);
                    if (ok) { changes.moisture_high_warn = true; changes.v_moisture_high_warn = cmd.value; }
                } else {
                    LOG_ERROR(TAG, "Invalid moisture_high_warn value: %.2f", cmd.value);
                }
                break;
            case CommandType::UPDATE_MOISTURE_HIGH_CRIT:
                if (validateMoistureThreshold(cmd.value)) {
                    ok = RuntimeThresholds::setMoistureHighCrit(cmd.value);
                    if (ok) { changes.moisture_high_crit = true; changes.v_moisture_high_crit = cmd.value; }
                } else {
                    LOG_ERROR(TAG, "Invalid moisture_high_crit value: %.2f", cmd.value);
                }
                break;
            default:
                // Ignore non-threshold or internal commands
                break;
        }
        return ok;
    }

    static void publishAckIfAny(const ThresholdChanges& changes) {
        if (s_thresholds_changed_queue == nullptr) {
            return;
        }
        // Count changes
        int count = (changes.temp_low_warn ? 1 : 0) +
                    (changes.temp_low_crit ? 1 : 0) +
                    (changes.temp_high_warn ? 1 : 0) +
                    (changes.temp_high_crit ? 1 : 0) +
                    (changes.moisture_low_warn ? 1 : 0) +
                    (changes.moisture_low_crit ? 1 : 0) +
                    (changes.moisture_high_warn ? 1 : 0) +
                    (changes.moisture_high_crit ? 1 : 0);
        if (count == 0) {
            return;
        }

        CloudPublishRequest req{};
        std::snprintf(req.topic, sizeof(req.topic),
                      Config::Mqtt::Topics::THRESHOLDS_ACK, Config::Device::id);

        char ts[16];
        TimeSync::formatFixedTimestamp(ts, sizeof(ts));

        // Build changes object content (static buffer, zero allocation)
        char changes_json[256];
        int off = 0;
        bool first = true;
        
        auto append_field = [&](const char* name, float value, const char* fmt) {
            if (off < static_cast<int>(sizeof(changes_json))) {
                off += std::snprintf(changes_json + off, sizeof(changes_json) - off,
                                    "%s\"%s\":", first ? "" : ",", name);
                off += std::snprintf(changes_json + off, sizeof(changes_json) - off,
                                    fmt, static_cast<double>(value));
                first = false;
            }
        };
        
        if (changes.temp_low_warn)  append_field("temp_low_warn",  changes.v_temp_low_warn,  "%.2f");
        if (changes.temp_low_crit)  append_field("temp_low_crit",  changes.v_temp_low_crit,  "%.2f");
        if (changes.temp_high_warn) append_field("temp_high_warn", changes.v_temp_high_warn, "%.2f");
        if (changes.temp_high_crit) append_field("temp_high_crit", changes.v_temp_high_crit, "%.2f");
        if (changes.moisture_low_warn)  append_field("moisture_low_warn",  changes.v_moisture_low_warn,  "%.1f");
        if (changes.moisture_low_crit)  append_field("moisture_low_crit",  changes.v_moisture_low_crit,  "%.1f");
        if (changes.moisture_high_warn) append_field("moisture_high_warn", changes.v_moisture_high_warn, "%.1f");
        if (changes.moisture_high_crit) append_field("moisture_high_crit", changes.v_moisture_high_crit, "%.1f");

        // Assemble final JSON using mjson_snprintf (zero allocation)
        mjson_snprintf(req.payload, sizeof(req.payload),
                       "{\"changes\":{%s},\"ts\":\"%s\",\"status\":\"ok\"}",
                       changes_json, ts);

        if (xQueueSend(s_thresholds_changed_queue, &req, 0) != pdTRUE) {
            LOG_WARN(TAG, "%s", "thresholds_changed_queue full, dropped thresholds-changed ACK");
        } else {
            LOG_INFO(TAG, "Enqueued thresholds-changed ACK: %s", req.payload);
        }
    }

    static void taskFunction(void* arg) {
        (void)arg;
        LOG_INFO(TAG, "%s", "Command Task started");

        Command cmd;
        for (;;) {
            // Block waiting for commands
            if (xQueueReceive(s_command_queue, &cmd, portMAX_DELAY) != pdTRUE) {
                continue;
            }
            // Only handle external threshold updates here
            if (cmd.type >= 0) {
                continue;
            }

            // Start batch: apply first command and then accumulate changes for a brief window
            ThresholdChanges changes{};
            (void)applyAndRecordChange(cmd, changes);

            TickType_t start = xTaskGetTickCount();
            const TickType_t window = pdMS_TO_TICKS(50);
            for (;;) {
                // If window elapsed, stop accumulating
                if ((xTaskGetTickCount() - start) > window) {
                    break;
                }
                Command next{};
                if (xQueueReceive(s_command_queue, &next, 0) == pdTRUE) {
                    if (next.type < 0) {
                        (void)applyAndRecordChange(next, changes);
                    } else {
                        // Not ours; push back to front for the other consumer
                        (void)xQueueSendToFront(s_command_queue, &next, 0);
                        break;
                    }
                } else {
                    // brief sleep within window to allow queue to fill
                    vTaskDelay(pdMS_TO_TICKS(5));
                }
            }

            publishAckIfAny(changes);
        }
    }
}

namespace CommandTask {
    void create(QueueHandle_t command_queue, QueueHandle_t thresholds_changed_queue) {
        s_command_queue = command_queue;
        s_thresholds_changed_queue = thresholds_changed_queue;
        xTaskCreateStatic(taskFunction,
                          "cmd_task",
                          sizeof(s_task_stack) / sizeof(StackType_t),
                          nullptr,
                          Config::TaskPriorities::NORMAL,
                          s_task_stack,
                          &s_task_tcb);
    }
}
