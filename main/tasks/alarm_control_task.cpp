#include <main/tasks/alarm_control_task.hpp>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <main/utils/logger.hpp>
#include <main/hardware/vibration_module.hpp>
#include <main/models/alarm_event.hpp>

namespace {
    static const char* TAG = "ALARM_TASK";

    static StaticTask_t s_task_tcb;
    static StackType_t s_task_stack[4096 / sizeof(StackType_t)];

    static QueueHandle_t s_alarm_queue = nullptr;
    static VibrationModule* s_vibration = nullptr;

    static void playPattern(uint8_t type) {
        if (!s_vibration) {
            return;
        }

        switch (type) {
            case 0: // short single
                s_vibration->vibrateMs(200);
                break;
            case 1: // double short
                s_vibration->pulse(200, 100, 2);
                break;
            case 2: // long
                s_vibration->vibrateMs(1000);
                break;
            case 3: // triple
                s_vibration->pulse(150, 100, 3);
                break;
            default: // fallback: two long
                s_vibration->pulse(400, 150, 2);
                break;
        }
    }

    static void taskFunction(void* arg) {
        (void)arg;
        LOG_INFO(TAG, "%s", "Alarm Control Task started");

        // Test vibration pattern on startup (for hardware verification)
        //TODO: remove this after we verify it works
        if (s_vibration) {
            LOG_INFO(TAG, "%s", "Testing vibration module...");
            // Test: Try continuous vibration for 2 seconds to verify hardware
            LOG_INFO(TAG, "%s", "Continuous vibration test (2 seconds)...");
            s_vibration->on();
            vTaskDelay(pdMS_TO_TICKS(2000));
            s_vibration->off();
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // Then try pulse pattern
            LOG_INFO(TAG, "%s", "Pulse pattern test...");
            for (int i = 0; i < 5; ++i) {
                s_vibration->vibrateMs(500);
                vTaskDelay(pdMS_TO_TICKS(200));
                LOG_INFO(TAG, "Vibration pulse %d/5", i + 1);
            }
            LOG_INFO(TAG, "%s", "Vibration test complete");
        } else {
            LOG_WARN(TAG, "%s", "Vibration module not available for testing");
        }

        // Wait for alarms and act immediately
        for (;;) {
            AlarmEvent evt{};
            if (xQueueReceive(s_alarm_queue, &evt, portMAX_DELAY) == pdTRUE) {
                playPattern(evt.type);
            }
        }
    }
}

namespace AlarmControlTask {
    void create(QueueHandle_t alarm_queue, gpio_num_t vibration_pin, bool vibration_active_high) {
        s_alarm_queue = alarm_queue;
        
        // Initialize vibration module
        static VibrationModule vibration(vibration_pin, vibration_active_high);
        s_vibration = &vibration;
        if (!s_vibration->init()) {
            LOG_WARN("ALARM_TASK", "Vibration module init failed on GPIO %d", static_cast<int>(vibration_pin));
            s_vibration = nullptr;
        } else {
            LOG_INFO("ALARM_TASK", "Vibration module ready on GPIO %d", static_cast<int>(vibration_pin));
        }

        xTaskCreateStatic(taskFunction, "alarm_control",
                          sizeof(s_task_stack) / sizeof(StackType_t), nullptr,
                          tskIDLE_PRIORITY + 2, s_task_stack, &s_task_tcb);
    }
}


