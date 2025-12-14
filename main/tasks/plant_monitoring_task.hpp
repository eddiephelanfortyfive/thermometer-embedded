#ifndef PLANT_MONITORING_TASK_HPP
#define PLANT_MONITORING_TASK_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace PlantMonitoringTask {
    void create(QueueHandle_t temperature_data_queue,
                QueueHandle_t moisture_data_queue,
                QueueHandle_t alarm_queue,
                QueueHandle_t lcd_queue,
                QueueHandle_t command_queue,
                QueueHandle_t temperature_mqtt_queue,
                QueueHandle_t moisture_mqtt_queue);
}

#endif // PLANT_MONITORING_TASK_HPP


