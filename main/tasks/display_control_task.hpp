#ifndef DISPLAY_CONTROL_TASK_HPP
#define DISPLAY_CONTROL_TASK_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <main/hardware/led_display.hpp>

namespace DisplayControlTask {
    // Queue item is uint8_t number 0..99 to display. Values >99 are clamped.
    void create(const LedDisplay::Pins& pins,
                bool common_anode,
                QueueHandle_t number_queue,
                uint8_t initial_brightness_percent = 80);
}

#endif // DISPLAY_CONTROL_TASK_HPP


