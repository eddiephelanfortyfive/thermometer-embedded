#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <main/utils/logger.hpp>
#include <main/config/config.hpp>
#include <main/tasks/cloud_communication_task.hpp>
#include <main/tasks/temperature_sensor_task.hpp>
#include <main/tasks/soil_moisture_task.hpp>
#include <main/tasks/alarm_control_task.hpp>
#include <main/models/sensor_data.hpp>
#include <main/models/alarm_event.hpp>
#include <main/models/command.hpp>
#include <main/models/moisture_data.hpp>
#include <freertos/queue.h>
#include <main/hardware/buzzer.hpp>

extern "C" void app_main(void)
{
    Logger::setLevel(LogLevel::INFO);
    LOG_INFO("MAIN", "%s", "---Didgital thermometer started---");

    // Create static queues
    static uint8_t sensor_queue_storage[32 * sizeof(SensorData)];
    static StaticQueue_t sensor_queue_tcb;
    QueueHandle_t sensor_queue = xQueueCreateStatic(
        32, sizeof(SensorData), sensor_queue_storage, &sensor_queue_tcb);

    static uint8_t alarm_queue_storage[16 * sizeof(AlarmEvent)];
    static StaticQueue_t alarm_queue_tcb;
    QueueHandle_t alarm_queue = xQueueCreateStatic(
        16, sizeof(AlarmEvent), alarm_queue_storage, &alarm_queue_tcb);

    static uint8_t command_queue_storage[16 * sizeof(Command)];
    static StaticQueue_t command_queue_tcb;
    QueueHandle_t command_queue = xQueueCreateStatic(
        16, sizeof(Command), command_queue_storage, &command_queue_tcb);

    // Create moisture queue
    static uint8_t moisture_queue_storage[16 * sizeof(MoistureData)];
    static StaticQueue_t moisture_queue_tcb;
    QueueHandle_t moisture_queue = xQueueCreateStatic(
        16, sizeof(MoistureData), moisture_queue_storage, &moisture_queue_tcb);
    // Start tasks (honor feature toggles)
    if (Config::Features::enable_cloud_comm) {
        CloudCommunicationTask::create(sensor_queue, alarm_queue, command_queue, moisture_queue);
    }
    if (Config::Features::enable_temperature_task) {
        TemperatureSensorTask::create(sensor_queue);
    }
    if (Config::Features::enable_moisture_task) {
        SoilMoistureTask::create(moisture_queue);
    }
    if (Config::Features::enable_alarm_task) {
        // Start Alarm Control Task on buzzer GPIO 26, active high
        AlarmControlTask::create(alarm_queue, Config::Hardware::Pins::buzzer_gpio, true);
    }

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t loop_period = pdMS_TO_TICKS(1000);
    for (;;) {
        vTaskDelayUntil(&last_wake_time, loop_period);
    }
}


