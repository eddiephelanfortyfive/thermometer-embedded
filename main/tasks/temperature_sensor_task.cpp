#include <main/tasks/temperature_sensor_task.hpp>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_timer.h>
#include <main/utils/logger.hpp>
#include <main/hardware/temperature_sensor.hpp>
#include <main/models/temperature_data.hpp>
#include <main/config/config.hpp>
#include <main/utils/watchdog.hpp>

namespace {
    static const char* TAG = "TEMP_TASK";

    // Static task resources
    static StaticTask_t s_task_tcb;
    static StackType_t s_task_stack[3072 / sizeof(StackType_t)];

    // Runtime state
    static QueueHandle_t s_temperature_data_queue = nullptr;
    static TemperatureSensor s_sensor; // default GPIO per header

    static void taskFunction(void* arg) {
        (void)arg;
        LOG_INFO(TAG, "%s", "Temperature Sensor Task started");
        Watchdog::subscribe();
        bool inited = s_sensor.init();
        if (!inited) {
            LOG_WARN(TAG, "%s", "Sensor init failed; will retry periodically");
        }

        TickType_t last_wake = xTaskGetTickCount();
        const TickType_t period = pdMS_TO_TICKS(Config::Tasks::Temperature::period_ms);

        for (;;) {
            Watchdog::feed();
            if (!inited) {
                inited = s_sensor.init();
                if (!inited) {
                    LOG_WARN(TAG, "%s", "Sensor init retry failed");
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    continue;
                } else {
                    LOG_INFO(TAG, "%s", "Sensor init successful");
                }
            }

            float temp_c = 0.0f;
            if (s_sensor.readTemperature(temp_c)) {
                TemperatureData sample;
                sample.temp_c = temp_c;
                sample.ts_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
                (void)xQueueSend(s_temperature_data_queue, &sample, 0);
            } else {
                LOG_WARN(TAG, "%s", "Temperature read failed");
            }

            vTaskDelayUntil(&last_wake, period);
        }
    }
}

namespace TemperatureSensorTask {
    void create(QueueHandle_t sensor_queue) {
        s_temperature_data_queue = sensor_queue;
        xTaskCreateStatic(taskFunction, "temperature_sensor",
                          sizeof(s_task_stack) / sizeof(StackType_t), nullptr,
                          Config::TaskPriorities::HIGH, s_task_stack, &s_task_tcb);
    }
}


