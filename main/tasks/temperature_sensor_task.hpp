#ifndef TEMPERATURE_SENSOR_TASK_HPP
#define TEMPERATURE_SENSOR_TASK_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace TemperatureSensorTask {
    // Creates a static FreeRTOS task that periodically reads the temperature sensor
    // and enqueues TemperatureData samples to the provided temperature_data_queue.
    void create(QueueHandle_t temperature_data_queue);
}

#endif // TEMPERATURE_SENSOR_TASK_HPP


