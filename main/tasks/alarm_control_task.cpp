#include <main/tasks/alarm_control_task.hpp>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <main/utils/logger.hpp>
#include <main/hardware/speaker.hpp>
#include <main/models/alarm_event.hpp>
#include <main/config/config.hpp>
#include <main/state/device_state.hpp>
#include <main/utils/watchdog.hpp>

namespace {
    static const char* TAG = "ALARM_TASK";

    static StaticTask_t s_task_tcb;
    static StackType_t s_task_stack[4096 / sizeof(StackType_t)];

    static QueueHandle_t s_alarm_queue = nullptr;
    static Speaker* s_speaker = nullptr;

    enum class Mode : uint8_t { NONE = 0, WARNING = 1, CRITICAL = 2 };
    static Mode s_mode = Mode::NONE;
    static TickType_t s_last_crit_cycle = 0;

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

        // Boot up chime: low to high "doo-do"
        if (s_speaker) {
            LOG_INFO(TAG, "%s", "Boot chime...");
            // Low tone
            (void)s_speaker->setFrequency(600);
            s_speaker->toneOn();
            vTaskDelay(pdMS_TO_TICKS(300));
            s_speaker->toneOff();
            vTaskDelay(pdMS_TO_TICKS(120));
            // High tone
            (void)s_speaker->setFrequency(1200);
            s_speaker->toneOn();
            vTaskDelay(pdMS_TO_TICKS(220));
            s_speaker->toneOff();
        } else {
            LOG_WARN(TAG, "%s", "Speaker not available for testing");
        }

        Watchdog::subscribe();

        // Wait for alarms and act; repeat pattern in CRITICAL mode
        for (;;) {
            Watchdog::feed();
            AlarmEvent evt{};
            // Use a short timeout so we can schedule repeated critical beeps
            if (xQueueReceive(s_alarm_queue, &evt, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Map incoming event types to mode
                if (evt.type == AlarmType::CRITICAL) {
                    s_mode = Mode::CRITICAL;
                    s_last_crit_cycle = 0; // force immediate cycle
                } else if (evt.type == AlarmType::WARNING) {
                    s_mode = Mode::WARNING;
                    // Single short beep on warning event
                    playPattern(0);
                } else {
                    // Clear/unknown alarm
                    s_mode = Mode::NONE;
                }
            }

            // Poll shared device state machine to ensure correctness
            auto ds = DeviceStateMachine::get();
            if (ds != DeviceStateMachine::DeviceState::CRITICAL) {
                // Stop repeating loop when not critical
                s_mode = Mode::NONE;
            } else {
                s_mode = Mode::CRITICAL;
            }

            // Handle continuous beeping in CRITICAL mode
            if (s_mode == Mode::CRITICAL && s_speaker) {
                TickType_t now = xTaskGetTickCount();
                const TickType_t cycle = pdMS_TO_TICKS(Config::Monitoring::crit_cycle_ms);
                if (s_last_crit_cycle == 0 || (now - s_last_crit_cycle) >= cycle) {
                    // Play configured critical pattern
                    s_speaker->pulse(Config::Monitoring::crit_on_ms,
                                     Config::Monitoring::crit_off_ms,
                                     Config::Monitoring::crit_repeat);
                    s_last_crit_cycle = xTaskGetTickCount();
                }
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
                          Config::TaskPriorities::CRITICAL, s_task_stack, &s_task_tcb);
    }
}


