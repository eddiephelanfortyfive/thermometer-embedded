#include <main/tasks/alarm_control_task.hpp>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <main/utils/logger.hpp>
#include <main/hardware/speaker.hpp>
#include <main/models/alarm_event.hpp>

namespace {
    static const char* TAG = "ALARM_TASK";

    static StaticTask_t s_task_tcb;
    static StackType_t s_task_stack[4096 / sizeof(StackType_t)];

    static QueueHandle_t s_alarm_queue = nullptr;
    static Speaker* s_speaker = nullptr;

    static void playPattern(uint8_t type) {
        if (!s_speaker) {
            return;
        }

        switch (type) {
            case 0: // short single
                s_speaker->beepMs(2500 ? 200 : 200); // freq already set in driver
                break;
            case 1: // double short
                s_speaker->pulse(200, 100, 2);
                break;
            case 2: // long
                s_speaker->beepMs(1000);
                break;
            case 3: // triple
                s_speaker->pulse(150, 100, 3);
                break;
            default: // fallback: two long
                s_speaker->pulse(400, 150, 2);
                break;
        }
    }

    static void taskFunction(void* arg) {
        (void)arg;
        LOG_INFO(TAG, "%s", "Alarm Control Task started");

        // Test speaker pattern on startup (for hardware verification)
        //TODO: remove this after we verify it works
        if (s_speaker) {
            LOG_INFO(TAG, "%s", "Testing speaker...");
            // Three 1.5s tones with 300ms gaps
            for (int i = 0; i < 3; ++i) {
                s_speaker->beepMs(1500);
                vTaskDelay(pdMS_TO_TICKS(300));
                LOG_INFO(TAG, "Speaker burst %d/3", i + 1);
            }

            // Follow with fast beeps to ensure audibility
            LOG_INFO(TAG, "%s", "Fast beep pattern...");
            for (int i = 0; i < 8; ++i) {
                s_speaker->beepMs(200);
                vTaskDelay(pdMS_TO_TICKS(150));
            }
            LOG_INFO(TAG, "%s", "Speaker test complete");
        } else {
            LOG_WARN(TAG, "%s", "Speaker not available for testing");
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
    void create(QueueHandle_t alarm_queue, gpio_num_t speaker_pin, bool active_high) {
        s_alarm_queue = alarm_queue;
        
        // Initialize speaker (LEDC PWM)
        static Speaker speaker(speaker_pin, active_high);
        s_speaker = &speaker;
        if (!s_speaker->init()) {
            LOG_WARN("ALARM_TASK", "Speaker init failed on GPIO %d", static_cast<int>(speaker_pin));
            s_speaker = nullptr;
        } else {
            LOG_INFO("ALARM_TASK", "Speaker ready on GPIO %d", static_cast<int>(speaker_pin));
        }

        xTaskCreateStatic(taskFunction, "alarm_control",
                          sizeof(s_task_stack) / sizeof(StackType_t), nullptr,
                          tskIDLE_PRIORITY + 2, s_task_stack, &s_task_tcb);
    }
}


