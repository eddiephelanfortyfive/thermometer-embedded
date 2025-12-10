#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <main/utils/logger.hpp>
#include <main/config/config.hpp>
#include <main/tasks/cloud_communication_task.hpp>
#include <freertos/queue.h>
#include <main/hardware/buzzer.hpp>

// Queue item shapes
struct SensorDatum { float temp_c; uint32_t ts_ms; };
struct AlarmEvent  { float temperature; uint32_t timestamp_ms; int type; };
struct Command     { uint32_t timestamp_ms; int type; float value; };

extern "C" void app_main(void)
{
    Logger::setLevel(LogLevel::INFO);
    LOG_INFO("MAIN", "%s", "---Didgital thermometer started---");

    // Create static queues
    static uint8_t sensor_queue_storage[32 * sizeof(SensorDatum)];
    static StaticQueue_t sensor_queue_tcb;
    QueueHandle_t sensor_queue = xQueueCreateStatic(
        32, sizeof(SensorDatum), sensor_queue_storage, &sensor_queue_tcb);

    static uint8_t alarm_queue_storage[16 * sizeof(AlarmEvent)];
    static StaticQueue_t alarm_queue_tcb;
    QueueHandle_t alarm_queue = xQueueCreateStatic(
        16, sizeof(AlarmEvent), alarm_queue_storage, &alarm_queue_tcb);

    static uint8_t command_queue_storage[16 * sizeof(Command)];
    static StaticQueue_t command_queue_tcb;
    QueueHandle_t command_queue = xQueueCreateStatic(
        16, sizeof(Command), command_queue_storage, &command_queue_tcb);

    // Start Cloud Communication Task
    CloudCommunicationTask::create(sensor_queue, alarm_queue, command_queue);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t loop_period = pdMS_TO_TICKS(1000);
    static Buzzer buzzer(GPIO_NUM_26, true);
    if (!buzzer.init()) {
        LOG_WARN("MAIN", "%s", "Buzzer init failed on GPIO 26");
    } else {
        LOG_INFO("MAIN", "%s", "Buzzer configured on GPIO 26");
        LOG_INFO("MAIN", "%s", "Buzzer test: double pulse");
        buzzer.pulse(500, 250, 2);
    }
    for (;;) {
        // Periodic buzzer check to make debugging easier (every 5s)
        static uint32_t loop_count = 0;
        if ((++loop_count % 5) == 0) {
            LOG_INFO("MAIN", "%s", "Buzzer periodic pulse");
            buzzer.pulse(200, 0, 1);
        }

        vTaskDelayUntil(&last_wake_time, loop_period);
    }
}


