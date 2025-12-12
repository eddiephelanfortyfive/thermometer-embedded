#ifndef PLANT_MONITORING_TASK_HPP
#define PLANT_MONITORING_TASK_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace PlantMonitoringTask {
    void create(QueueHandle_t sensor_queue,
                QueueHandle_t moisture_queue,
                QueueHandle_t alarm_queue,
                QueueHandle_t lcd_queue,
                QueueHandle_t command_queue);
}

#endif // PLANT_MONITORING_TASK_HPP


