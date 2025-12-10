#ifndef CLOUD_COMMUNICATION_TASK_HPP
#define CLOUD_COMMUNICATION_TASK_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace CloudCommunicationTask {
    void create(QueueHandle_t sensor_queue,
                QueueHandle_t alarm_queue,
                QueueHandle_t command_queue,
                QueueHandle_t moisture_queue);
}

#endif // CLOUD_COMMUNICATION_TASK_HPP


