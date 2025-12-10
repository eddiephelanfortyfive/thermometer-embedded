#ifndef SOIL_MOISTURE_TASK_HPP
#define SOIL_MOISTURE_TASK_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <main/hardware/soil_moisture_sensor.hpp>

namespace SoilMoistureTask {
    // Create task with explicit sensor ADC configuration
    void create(QueueHandle_t moisture_queue, const SoilMoistureSensor::Config& config);

    // Convenience overload with reasonable defaults (ADC1, GPIO34, 11dB, 8 samples)
    void create(QueueHandle_t moisture_queue);
}

#endif // SOIL_MOISTURE_TASK_HPP


