#include <main/tasks/plant_monitoring_task.hpp>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <main/models/temperature_data.hpp>
#include <main/models/moisture_data.hpp>
#include <main/models/alarm_event.hpp>
#include <main/tasks/lcd_display_task.hpp>
#include <main/utils/logger.hpp>
#include <main/config/config.hpp>
#include <main/utils/time_sync.hpp>
#include <cstring>
#include <cstdio>
#include <main/models/command.hpp>
#include <main/state/device_state.hpp>
#include <main/state/runtime_thresholds.hpp>
#include <main/utils/watchdog.hpp>

namespace {
    static const char* TAG = "PLANT_MON";

    static StaticTask_t s_task_tcb;
    static StackType_t s_task_stack[4096 / sizeof(StackType_t)];

    static QueueHandle_t q_temperature_data = nullptr;
    static QueueHandle_t q_moisture_data  = nullptr;
    static QueueHandle_t q_alarm  = nullptr;
    static QueueHandle_t q_lcd    = nullptr;
    static QueueHandle_t q_cmd    = nullptr;
    // Cloud-forward queues (length 1, latest-only)
    static QueueHandle_t q_temperature_mqtt = nullptr;
    static QueueHandle_t q_moisture_mqtt  = nullptr;

    enum class State : uint8_t { OK = 0, WARNING = 1, CRITICAL = 2 };
    enum class Reason : uint8_t { CLEAR = 0, TEMP_HIGH = 1, TEMP_LOW = 2, MOISTURE_LOW = 3 };

    struct LastSamples {
        bool     has_temp = false;
        bool     has_moist = false;
        float    temp_c = 0.0f;
        float    moisture_pct = 0.0f;
        uint32_t temp_ts = 0;
        uint32_t moist_ts = 0;
    };

    // Forward declarations for classifiers used in setLcd
    static State classifyTemp(float t, Reason& r);
    static State classifyMoist(float m, Reason& r);

    static void setLcd(State s, const LastSamples& last, bool flash_phase) {
        if (!q_lcd) return;
        LcdUpdate u{};
        snprintf(u.line1, sizeof(u.line1), "T:%3.1fC M:%2.1f%%", last.temp_c, last.moisture_pct);
        if (s == State::CRITICAL) {
            Reason tr, mr;
            State ts = classifyTemp(last.temp_c, tr);
            State ms = classifyMoist(last.moisture_pct, mr);
            if (ts == State::CRITICAL && ms == State::CRITICAL) {
                snprintf(u.line2, sizeof(u.line2), "Crit: T+M");
            } else if (ts == State::CRITICAL) {
                snprintf(u.line2, sizeof(u.line2), "Crit: T");
            } else if (ms == State::CRITICAL) {
                snprintf(u.line2, sizeof(u.line2), "Crit: M");
            } else {
                snprintf(u.line2, sizeof(u.line2), "Critical");
            }
        } else {
            if (s == State::WARNING) {
                Reason tr, mr;
                State ts = classifyTemp(last.temp_c, tr);
                State ms = classifyMoist(last.moisture_pct, mr);
                bool t_warn = (ts == State::WARNING);
                bool m_warn = (ms == State::WARNING);
                if (t_warn && m_warn) {
                    snprintf(u.line2, sizeof(u.line2), "Warn: T+M");
                } else if (t_warn) {
                    snprintf(u.line2, sizeof(u.line2), "Warn: T");
                } else if (m_warn) {
                    snprintf(u.line2, sizeof(u.line2), "Warn: M");
                } else {
                    snprintf(u.line2, sizeof(u.line2), "Warning");
                }
            } else {
                snprintf(u.line2, sizeof(u.line2), "OK");
            }
        }
        u.set_backlight = 1;
        switch (s) {
            case State::OK:       u.r = 0;   u.g = 255; u.b = 0;   break;
            case State::WARNING:  u.r = 255; u.g = 128; u.b = 0;   break;
            case State::CRITICAL: u.r = flash_phase ? 255 : 20; u.g = 0; u.b = 0; break;
        }
        u.clear_first = 0;
        (void)xQueueSend(q_lcd, &u, 0);
    }

    static State classifyTemp(float t, Reason& r) {
        // Cache thresholds to avoid repeated function calls
        static float cached_temp_low_crit = 0.0f;
        static float cached_temp_high_crit = 0.0f;
        static float cached_temp_low_warn = 0.0f;
        static float cached_temp_high_warn = 0.0f;
        static TickType_t last_cache_update = 0;
        
        TickType_t now = xTaskGetTickCount();
        // Refresh cache every 5 seconds to pick up threshold changes
        if ((now - last_cache_update) > pdMS_TO_TICKS(5000) || last_cache_update == 0) {
            cached_temp_low_crit = RuntimeThresholds::getTempLowCrit();
            cached_temp_high_crit = RuntimeThresholds::getTempHighCrit();
            cached_temp_low_warn = RuntimeThresholds::getTempLowWarn();
            cached_temp_high_warn = RuntimeThresholds::getTempHighWarn();
            last_cache_update = now;
        }

        if (t <= cached_temp_low_crit)  { r = Reason::TEMP_LOW;  return State::CRITICAL; }
        if (t >= cached_temp_high_crit) { r = Reason::TEMP_HIGH; return State::CRITICAL; }
        if (t <= cached_temp_low_warn)  { r = Reason::TEMP_LOW;  return State::WARNING; }
        if (t >= cached_temp_high_warn) { r = Reason::TEMP_HIGH; return State::WARNING; }
        r = Reason::CLEAR; return State::OK;
    }
    static State classifyMoist(float m, Reason& r) {
        // Cache thresholds to avoid repeated function calls
        static float cached_moisture_low_crit = 0.0f;
        static float cached_moisture_low_warn = 0.0f;
        static TickType_t last_cache_update = 0;
        
        TickType_t now = xTaskGetTickCount();
        // Refresh cache every 5 seconds to pick up threshold changes
        if ((now - last_cache_update) > pdMS_TO_TICKS(5000) || last_cache_update == 0) {
            cached_moisture_low_crit = RuntimeThresholds::getMoistureLowCrit();
            cached_moisture_low_warn = RuntimeThresholds::getMoistureLowWarn();
            last_cache_update = now;
        }

        if (m <= cached_moisture_low_crit) { r = Reason::MOISTURE_LOW; return State::CRITICAL; }
        if (m <= cached_moisture_low_warn) { r = Reason::MOISTURE_LOW; return State::WARNING; }
        r = Reason::CLEAR; return State::OK;
    }

    static void sendBuzzer(State s, Reason r) {
        if (!q_alarm) return;
        AlarmEvent evt{};
        evt.timestamp_ms = static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
        evt.temperature_c = 0.0f;
        // Map: warning → type 0 (short), critical → type 3 (triple)
        evt.type = (s == State::CRITICAL) ? 3 : (s == State::WARNING) ? 0 : 0xFF;
        if (evt.type != 0xFF) (void)xQueueSend(q_alarm, &evt, 0);
    }

    // Send raw alarm event type to alarm task
    static void sendAlarmType(uint8_t type) {
        if (!q_alarm) return;
        AlarmEvent evt{};
        evt.timestamp_ms = static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
        evt.temperature_c = 0.0f;
        evt.type = type;
        (void)xQueueSend(q_alarm, &evt, 0);
    }

    // Publish alert via Cloud task by sending a Command with state and reason encoded
    static void requestAlert(State s, Reason r) {
        if (!q_cmd) return;
        Command c{};
        c.timestamp_ms = static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
        // Encode: type = state (0,1,2), value = reason code (0..3)
        c.type  = static_cast<int32_t>(s);
        c.value = static_cast<float>(static_cast<uint32_t>(r));
        (void)xQueueSend(q_cmd, &c, 0);
    }

    static void taskFn(void* arg) {
        (void)arg;
        LOG_INFO(TAG, "%s", "Plant Monitoring Task started");
        Watchdog::subscribe();
        LastSamples last{};
        State current = State::OK;
        Reason cur_reason = Reason::CLEAR;
        TickType_t warn_start = 0, crit_start = 0;
        TickType_t last_lcd_blink = 0;
        bool flash_phase = false;

        for (;;) {
            Watchdog::feed();
            // Drain queues (non-blocking)
            TemperatureData sd{};
            while (q_temperature_data && xQueueReceive(q_temperature_data, &sd, 0) == pdTRUE) {
                last.has_temp = true; last.temp_c = sd.temp_c; last.temp_ts = sd.ts_ms;
            }
            MoistureData md{};
            while (q_moisture_data && xQueueReceive(q_moisture_data, &md, 0) == pdTRUE) {
                last.has_moist = true; last.moisture_pct = md.moisture_percent; last.moist_ts = md.ts_ms;
            }

            // Classify each metric
            Reason tr, mr;
            State ts = last.has_temp ? classifyTemp(last.temp_c, tr) : State::OK;
            State ms = last.has_moist ? classifyMoist(last.moisture_pct, mr) : State::OK;
            // Pick overall = max severity (CRITICAL > WARNING > OK)
            State next = (ts == State::CRITICAL || ms == State::CRITICAL) ? State::CRITICAL
                         : (ts == State::WARNING || ms == State::WARNING) ? State::WARNING
                         : State::OK;
            Reason next_reason = (next == State::CRITICAL) ? (ts == State::CRITICAL ? tr : mr)
                                  : (next == State::WARNING) ? (ts == State::WARNING ? tr : mr)
                                  : Reason::CLEAR;

            TickType_t now = xTaskGetTickCount();
            using namespace Config::Monitoring;

            // Debounce
            bool state_change = false;
            if (next != current) {
                // entering potential state
                if (next == State::WARNING) {
                    if (warn_start == 0) warn_start = now;
                    if ((now - warn_start) * portTICK_PERIOD_MS >= confirm_warn_ms) {
                        current = next; cur_reason = next_reason; state_change = true; warn_start = 0;
                    }
                } else if (next == State::CRITICAL) {
                    if (crit_start == 0) crit_start = now;
                    if ((now - crit_start) * portTICK_PERIOD_MS >= confirm_crit_ms) {
                        current = next; cur_reason = next_reason; state_change = true; crit_start = 0;
                    }
                } else { // back to OK, apply hysteresis by requiring margin; assume debounce satisfied
                    current = next; cur_reason = next_reason; state_change = true;
                    warn_start = crit_start = 0;
                }
            } else {
                // Maintain timers
                warn_start = (next == State::WARNING)  ? (warn_start ? warn_start : now) : 0;
                crit_start = (next == State::CRITICAL) ? (crit_start ? crit_start : now) : 0;
            }

            // Actions on state change
            if (state_change) {
                // Buzzer/alarm task: explicit transitions
                // - Entering CRITICAL: start repeating pattern
                // - Entering WARNING (from non-critical): single short beep
                // - Leaving CRITICAL (to WARNING/OK): send CLEAR to stop loop
                // Determine previous state by inverting 'current' change
                // Note: We had 'current' already updated to 'next' above
                // We approximate previous as: if crit_start/warn_start just cleared due to transition
                // However, simpler: track previous via static in this scope
                static State prev = State::OK;
                if (prev == State::CRITICAL && current != State::CRITICAL) {
                    // Clear critical beeping loop
                    sendAlarmType(255); // CLEAR
                }
                if (current == State::CRITICAL) {
                    sendAlarmType(3);
                } else if (current == State::WARNING && prev != State::CRITICAL) {
                    sendAlarmType(0);
                }
                prev = current;

                // Update shared device state machine with reason flags
                uint8_t flags = DeviceStateMachine::REASON_NONE;
                if (ts == State::CRITICAL || ts == State::WARNING) {
                    if (tr == Reason::TEMP_HIGH) flags |= DeviceStateMachine::REASON_TEMP_HIGH;
                    if (tr == Reason::TEMP_LOW)  flags |= DeviceStateMachine::REASON_TEMP_LOW;
                }
                if (ms == State::CRITICAL || ms == State::WARNING) {
                    flags |= DeviceStateMachine::REASON_MOIST_LOW;
                }
                DeviceStateMachine::DeviceState ds = (current == State::CRITICAL)
                    ? DeviceStateMachine::DeviceState::CRITICAL
                    : (current == State::WARNING ? DeviceStateMachine::DeviceState::WARNING
                                                 : DeviceStateMachine::DeviceState::OK);
                DeviceStateMachine::set(ds, flags);

                // Request alert publish over MQTT via cloud
                requestAlert(current, cur_reason);
            }

            // Forward latest samples to Cloud (latest-only overwrite)
            if (q_temperature_mqtt && last.has_temp) {
                TemperatureData out{};
                out.temp_c = last.temp_c;
                out.ts_ms = last.temp_ts;
                (void)xQueueOverwrite(q_temperature_mqtt, &out);
            }
            if (q_moisture_mqtt && last.has_moist) {
                MoistureData out{};
                out.moisture_raw = 0;
                out.moisture_percent = last.moisture_pct;
                out.ts_ms = last.moist_ts;
                (void)xQueueOverwrite(q_moisture_mqtt, &out);
            }

            // LCD update (periodic; flash critical ~1Hz)
            if (current == State::CRITICAL) {
                if ((now - last_lcd_blink) >= pdMS_TO_TICKS(500)) {
                    flash_phase = !flash_phase;
                    setLcd(current, last, flash_phase);
                    last_lcd_blink = now;
                }
            } else {
                if ((now - last_lcd_blink) >= pdMS_TO_TICKS(1000)) {
                    flash_phase = false;
                    setLcd(current, last, flash_phase);
                    last_lcd_blink = now;
                }
            }

            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

namespace PlantMonitoringTask {
    void create(QueueHandle_t temperature_data_queue,
                QueueHandle_t moisture_data_queue,
                QueueHandle_t alarm_queue,
                QueueHandle_t lcd_queue,
                QueueHandle_t command_queue,
                QueueHandle_t temperature_mqtt_queue,
                QueueHandle_t moisture_mqtt_queue) {
        q_temperature_data = temperature_data_queue;
        q_moisture_data  = moisture_data_queue;
        q_alarm  = alarm_queue;
        q_lcd    = lcd_queue;
        q_cmd    = command_queue;
        q_temperature_mqtt = temperature_mqtt_queue;
        q_moisture_mqtt  = moisture_mqtt_queue;
        xTaskCreateStatic(taskFn, "plant_monitor",
                          sizeof(s_task_stack) / sizeof(StackType_t), nullptr,
                          Config::TaskPriorities::HIGH, s_task_stack, &s_task_tcb);
    }
}


