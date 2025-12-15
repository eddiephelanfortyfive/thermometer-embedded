#include <main/tasks/command_task.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <main/utils/logger.hpp>
#include <main/models/command.hpp>
#include <main/config/config.hpp>
#include <main/state/runtime_thresholds.hpp>
#include <inttypes.h>
#include <cstring>

static const char* TAG = "CMD_TASK";

namespace {
    // Command type encoding: negative values for external MQTT commands
    enum class CommandType : int32_t {
        UPDATE_TEMP_LOW_WARN = -1,
        UPDATE_TEMP_LOW_CRIT = -2,
        UPDATE_TEMP_HIGH_WARN = -3,
        UPDATE_TEMP_HIGH_CRIT = -4,
        UPDATE_MOISTURE_LOW_WARN = -5,
        UPDATE_MOISTURE_LOW_CRIT = -6,
    };

    static StaticTask_t s_task_tcb;
    static StackType_t s_task_stack[4096 / sizeof(StackType_t)];

    static QueueHandle_t s_command_queue = nullptr;

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

    static void taskFunction(void* arg) {
        (void)arg;
        LOG_INFO(TAG, "%s", "Command Task started");

        Command cmd;
        for (;;) {
            // Block waiting for commands
            if (xQueueReceive(s_command_queue, &cmd, portMAX_DELAY) == pdTRUE) {
                handleCommand(cmd);
            }
        }
    }
}

namespace CommandTask {
    void create(QueueHandle_t command_queue) {
        s_command_queue = command_queue;
        xTaskCreateStatic(taskFunction,
                          "cmd_task",
                          sizeof(s_task_stack) / sizeof(StackType_t),
                          nullptr,
                          Config::TaskPriorities::NORMAL,
                          s_task_stack,
                          &s_task_tcb);
    }
}
