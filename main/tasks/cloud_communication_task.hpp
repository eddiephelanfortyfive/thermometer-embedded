#ifndef CLOUD_COMMUNICATION_TASK_HPP
#define CLOUD_COMMUNICATION_TASK_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace CloudCommunicationTask {
    void create(QueueHandle_t temperature_mqtt_queue,
                QueueHandle_t alarm_queue,
                QueueHandle_t command_queue,
                QueueHandle_t moisture_mqtt_queue,
                QueueHandle_t thresholds_changed_queue);
}

#endif // CLOUD_COMMUNICATION_TASK_HPP


