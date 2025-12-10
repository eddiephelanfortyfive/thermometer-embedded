#include <main/tasks/display_control_task.hpp>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <main/utils/logger.hpp>
#include <main/config/config.hpp>
#include <cstring>

namespace {
    static const char* TAG = "DISPLAY_TASK";

    static StaticTask_t s_task_tcb;
    static StackType_t s_task_stack[2048 / sizeof(StackType_t)];

    static QueueHandle_t s_number_queue = nullptr;
    static LedDisplay* s_display = nullptr;
    static uint8_t s_current_value = 0;

    static void taskFunction(void* arg) {
        (void)arg;
        LOG_INFO(TAG, "%s", "Display Control Task started");

        // Initialize display instance (constructed in create)
        if (!s_display || !s_display->init()) {
            LOG_WARN(TAG, "%s", "LED display init failed");
        }

        // Start with showing current value
        uint8_t tens = static_cast<uint8_t>(s_current_value / 10);
        uint8_t ones = static_cast<uint8_t>(s_current_value % 10);
        s_display->setDigits(tens, ones);

        // Refresh for multiplexing + brightness PWM
        TickType_t last_wake = xTaskGetTickCount();
        const TickType_t tick_1ms = pdMS_TO_TICKS(Config::Tasks::Display::refresh_slice_ms);

        for (;;) {
            // Non-blocking update from queue
            uint8_t new_value = 0;
            if (xQueueReceive(s_number_queue, &new_value, 0) == pdTRUE) {
                if (new_value > 99) new_value = 99;
                s_current_value = new_value;
                tens = static_cast<uint8_t>(s_current_value / 10);
                ones = static_cast<uint8_t>(s_current_value % 10);
                s_display->setDigits(tens, ones);
            }

            // Perform one multiplex/PWM slice
            s_display->update();
            vTaskDelayUntil(&last_wake, tick_1ms);
        }
    }
}

namespace DisplayControlTask {
    void create(const LedDisplay::Pins& pins,
                bool common_anode,
                QueueHandle_t number_queue,
                uint8_t initial_brightness_percent) {
        s_number_queue = number_queue;
        static LedDisplay display(pins, common_anode); // static lifetime; no heap
        s_display = &display;
        s_display->setBrightness(initial_brightness_percent);
        s_current_value = 0;
        xTaskCreateStatic(taskFunction, "display_control",
                          sizeof(s_task_stack) / sizeof(StackType_t), nullptr,
                          tskIDLE_PRIORITY + 2, s_task_stack, &s_task_tcb);
    }
}


