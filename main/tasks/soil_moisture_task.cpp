#include <main/tasks/soil_moisture_task.hpp>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_timer.h>
#include <main/utils/logger.hpp>
#include <main/models/moisture_data.hpp>
#include <main/config/config.hpp>
#include <inttypes.h>

namespace {
    static const char* TAG = "MOISTURE_TASK";

    static StaticTask_t s_task_tcb;
    static StackType_t s_task_stack[3072 / sizeof(StackType_t)];

    static QueueHandle_t s_moisture_queue = nullptr;
    static SoilMoistureSensor s_sensor{ SoilMoistureSensor::Config{
        Config::Hardware::Moisture::unit,
        Config::Hardware::Moisture::channel,
        Config::Hardware::Moisture::attenuation,
        Config::Hardware::Moisture::sample_count,
        Config::Hardware::Moisture::raw_dry,
        Config::Hardware::Moisture::raw_wet
    }};

    static void taskFunction(void* arg) {
        (void)arg;
        LOG_INFO(TAG, "%s", "Soil Moisture Task started");
        bool inited = s_sensor.init();
        if (!inited) {
            LOG_WARN(TAG, "%s", "ADC init failed; will retry");
        }

        TickType_t last_wake = xTaskGetTickCount();
        const TickType_t period = pdMS_TO_TICKS(Config::Tasks::Moisture::period_ms);

        for (;;) {
            if (!inited) {
                inited = s_sensor.init();
                if (!inited) {
                    LOG_WARN(TAG, "%s", "ADC init retry failed");
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    continue;
                } else {
                    LOG_INFO(TAG, "%s", "ADC init successful");
                }
            }

            MoistureData sample{};
            if (s_sensor.read(sample)) {
                sample.ts_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
                (void)xQueueSend(s_moisture_queue, &sample, 0);
            } else {
                LOG_WARN(TAG, "%s", "Moisture read failed");
            }

            vTaskDelayUntil(&last_wake, period);
        }
    }
}

namespace SoilMoistureTask {
    void create(QueueHandle_t moisture_queue, const SoilMoistureSensor::Config& config) {
        s_moisture_queue = moisture_queue;
        s_sensor = SoilMoistureSensor(config);
        xTaskCreateStatic(taskFunction, "soil_moisture",
                          sizeof(s_task_stack) / sizeof(StackType_t), nullptr,
                          Config::TaskPriorities::HIGH, s_task_stack, &s_task_tcb);
    }

    void create(QueueHandle_t moisture_queue) {
        s_moisture_queue = moisture_queue;
        xTaskCreateStatic(taskFunction, "soil_moisture",
                          sizeof(s_task_stack) / sizeof(StackType_t), nullptr,
                          Config::TaskPriorities::HIGH, s_task_stack, &s_task_tcb);
    }
}


