#ifndef ALARM_CONTROL_TASK_HPP
#define ALARM_CONTROL_TASK_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/gpio.h>

namespace AlarmControlTask {
    // Create task that listens for AlarmEvent messages on alarm_queue
    // and drives a DFRobot vibration module on vibration_pin.
    // active_high determines vibration module polarity.
    void create(QueueHandle_t alarm_queue, gpio_num_t vibration_pin, bool vibration_active_high = true);
}

#endif // ALARM_CONTROL_TASK_HPP


