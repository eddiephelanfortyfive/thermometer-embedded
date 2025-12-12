#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <main/utils/logger.hpp>
#include <main/config/config.hpp>
#include <main/tasks/cloud_communication_task.hpp>
#include <main/tasks/temperature_sensor_task.hpp>
#include <main/tasks/soil_moisture_task.hpp>
#include <main/tasks/alarm_control_task.hpp>
#include <main/tasks/plant_monitoring_task.hpp>
#include <main/tasks/lcd_display_task.hpp>
#include <main/models/sensor_data.hpp>
#include <main/models/alarm_event.hpp>
#include <main/models/command.hpp>
#include <main/models/moisture_data.hpp>
#include <freertos/queue.h>
#include <cstring>

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

    static uint8_t moisture_queue_storage[16 * sizeof(MoistureData)];
    static StaticQueue_t moisture_queue_tcb;
    QueueHandle_t moisture_queue = xQueueCreateStatic(
        16, sizeof(MoistureData), moisture_queue_storage, &moisture_queue_tcb);
        
    static uint8_t lcd_queue_storage[8 * sizeof(LcdUpdate)];
    static StaticQueue_t lcd_queue_tcb;
    QueueHandle_t lcd_queue = xQueueCreateStatic(
        8, sizeof(LcdUpdate), lcd_queue_storage, &lcd_queue_tcb);

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
        AlarmControlTask::create(alarm_queue, Config::Hardware::Pins::vibration_module_gpio, true);
    }
    // Start monitoring task after producers/consumers are running
    PlantMonitoringTask::create(sensor_queue, moisture_queue, alarm_queue, lcd_queue, command_queue);
    if (Config::Features::enable_lcd_task) {
        LcdDisplayTask::create(lcd_queue);
       
    }

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t loop_period = pdMS_TO_TICKS(1000);
    for (;;) {
        vTaskDelayUntil(&last_wake_time, loop_period);
    }
}


