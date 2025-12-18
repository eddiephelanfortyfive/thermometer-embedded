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
#include <main/tasks/command_task.hpp>
#include <main/models/temperature_data.hpp>
#include <main/models/alarm_event.hpp>
#include <main/models/command.hpp>
#include <main/models/moisture_data.hpp>
#include <main/models/cloud_publish_request.hpp>
#include <main/state/runtime_thresholds.hpp>
#include <main/utils/watchdog.hpp>
#include <nvs_flash.h>
#include <freertos/queue.h>
#include <cstring>

extern "C" void app_main(void)
{
    Logger::setLevel(LogLevel::INFO);
    LOG_INFO("MAIN", "%s", "---Digital thermometer started---");

    // Initialize NVS (required before runtime thresholds can use it)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        LOG_ERROR("MAIN", "NVS init failed: %d", static_cast<int>(err));
    }

    // Initialize runtime thresholds (load from NVS or use defaults)
    RuntimeThresholds::init();

    // Initialize Task Watchdog Timer for safety-critical tasks
    Watchdog::init();

    // Create static queues
    static uint8_t temperature_data_queue_storage[32 * sizeof(TemperatureData)];
    static StaticQueue_t temperature_data_queue_tcb;
    QueueHandle_t temperature_data_queue = xQueueCreateStatic(
        32, sizeof(TemperatureData), temperature_data_queue_storage, &temperature_data_queue_tcb);

    static uint8_t alarm_queue_storage[16 * sizeof(AlarmEvent)];
    static StaticQueue_t alarm_queue_tcb;
    QueueHandle_t alarm_queue = xQueueCreateStatic(
        16, sizeof(AlarmEvent), alarm_queue_storage, &alarm_queue_tcb);

    static uint8_t command_queue_storage[16 * sizeof(Command)];
    static StaticQueue_t command_queue_tcb;
    QueueHandle_t command_queue = xQueueCreateStatic(
        16, sizeof(Command), command_queue_storage, &command_queue_tcb);

    static uint8_t moisture_data_queue_storage[16 * sizeof(MoistureData)];
    static StaticQueue_t moisture_data_queue_tcb;
    QueueHandle_t moisture_data_queue = xQueueCreateStatic(
        16, sizeof(MoistureData), moisture_data_queue_storage, &moisture_data_queue_tcb);
        
    static uint8_t lcd_queue_storage[8 * sizeof(LcdUpdate)];
    static StaticQueue_t lcd_queue_tcb;
    QueueHandle_t lcd_queue = xQueueCreateStatic(
        8, sizeof(LcdUpdate), lcd_queue_storage, &lcd_queue_tcb);

    static uint8_t temperature_mqtt_queue_storage[1 * sizeof(TemperatureData)];
    static StaticQueue_t temperature_mqtt_queue_tcb;
    QueueHandle_t temperature_mqtt_queue = xQueueCreateStatic(
        1, sizeof(TemperatureData), temperature_mqtt_queue_storage, &temperature_mqtt_queue_tcb);

    static uint8_t moisture_mqtt_queue_storage[1 * sizeof(MoistureData)];
    static StaticQueue_t moisture_mqtt_queue_tcb;
    QueueHandle_t moisture_mqtt_queue = xQueueCreateStatic(
        1, sizeof(MoistureData), moisture_mqtt_queue_storage, &moisture_mqtt_queue_tcb);

    // Thresholds-changed queue for consolidated ACKs
    static uint8_t thresholds_changed_queue_storage[4 * sizeof(CloudPublishRequest)];
    static StaticQueue_t thresholds_changed_queue_tcb;
    QueueHandle_t thresholds_changed_queue = xQueueCreateStatic(
        4, sizeof(CloudPublishRequest), thresholds_changed_queue_storage, &thresholds_changed_queue_tcb);

    // Start tasks (honor feature toggles)
    if (Config::Features::enable_cloud_comm) {
        CloudCommunicationTask::create(temperature_mqtt_queue, alarm_queue, command_queue, moisture_mqtt_queue, thresholds_changed_queue);
    }
    // Create command task to handle incoming MQTT commands
    CommandTask::create(command_queue, thresholds_changed_queue);
    
    if (Config::Features::enable_temperature_task) {
        TemperatureSensorTask::create(temperature_data_queue);
    }
    if (Config::Features::enable_moisture_task) {
        SoilMoistureTask::create(moisture_data_queue);
    }
    if (Config::Features::enable_alarm_task) {
        AlarmControlTask::create(alarm_queue, Config::Hardware::Pins::vibration_module_gpio, true);
    }
    // Start monitoring task after producers/consumers are running
    PlantMonitoringTask::create(temperature_data_queue, moisture_data_queue, alarm_queue, lcd_queue, command_queue,
                                temperature_mqtt_queue, moisture_mqtt_queue);
    if (Config::Features::enable_lcd_task) {
        LcdDisplayTask::create(lcd_queue);
       
    }

    // Main task has nothing to do after initialization - block forever
    // This yields CPU to all other tasks and keeps the task alive
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}


