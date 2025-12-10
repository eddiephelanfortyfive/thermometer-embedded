#ifndef ALARM_CONTROL_TASK_HPP
#define ALARM_CONTROL_TASK_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/gpio.h>

namespace AlarmControlTask {
    // Create task that listens for AlarmEvent messages on alarm_queue
    // and drives a buzzer on buzzer_pin. active_high determines
    // buzzer polarity.
    void create(QueueHandle_t alarm_queue, gpio_num_t buzzer_pin, bool active_high);
}

#endif // ALARM_CONTROL_TASK_HPP


