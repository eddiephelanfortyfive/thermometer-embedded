#include <main/tasks/alarm_control_task.hpp>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <main/utils/logger.hpp>
#include <main/hardware/buzzer.hpp>
#include <main/models/alarm_event.hpp>

namespace {
    static const char* TAG = "ALARM_TASK";

    static StaticTask_t s_task_tcb;
    static StackType_t s_task_stack[2048 / sizeof(StackType_t)];

    static QueueHandle_t s_alarm_queue = nullptr;
    static Buzzer* s_buzzer = nullptr;

    static void playPattern(uint8_t type) {
        switch (type) {
            case 0: // short single
                s_buzzer->buzzMs(200);
                break;
            case 1: // double short
                s_buzzer->pulse(200, 100, 2);
                break;
            case 2: // long
                s_buzzer->buzzMs(1000);
                break;
            case 3: // triple
                s_buzzer->pulse(150, 100, 3);
                break;
            default: // fallback: two long
                s_buzzer->pulse(400, 150, 2);
                break;
        }
    }

    static void taskFunction(void* arg) {
        (void)arg;
        LOG_INFO(TAG, "%s", "Alarm Control Task started");

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
    void create(QueueHandle_t alarm_queue, gpio_num_t buzzer_pin, bool active_high) {
        s_alarm_queue = alarm_queue;
        static Buzzer buzzer(buzzer_pin, active_high); // static lifetime
        s_buzzer = &buzzer;
        if (!s_buzzer->init()) {
            LOG_WARN("ALARM_TASK", "Buzzer init failed on GPIO %d", static_cast<int>(buzzer_pin));
        } else {
            LOG_INFO("ALARM_TASK", "Buzzer ready on GPIO %d", static_cast<int>(buzzer_pin));
        }
        xTaskCreateStatic(taskFunction, "alarm_control",
                          sizeof(s_task_stack) / sizeof(StackType_t), nullptr,
                          tskIDLE_PRIORITY + 2, s_task_stack, &s_task_tcb);
    }
}


