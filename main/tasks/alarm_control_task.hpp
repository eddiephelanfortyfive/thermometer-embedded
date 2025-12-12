#ifndef ALARM_CONTROL_TASK_HPP
#define ALARM_CONTROL_TASK_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/gpio.h>

namespace AlarmControlTask {
    // Create task that listens for AlarmEvent messages on alarm_queue
    // and drives the speaker on speaker_pin (LEDC PWM).
    // active_high determines drive polarity.
    void create(QueueHandle_t alarm_queue, gpio_num_t speaker_pin, bool active_high = true);
}

#endif // ALARM_CONTROL_TASK_HPP


