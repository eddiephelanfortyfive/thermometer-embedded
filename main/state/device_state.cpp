#include <main/state/device_state.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/portmacro.h>

namespace {
    struct StateData {
        DeviceStateMachine::DeviceState state;
        uint8_t reasons;
        uint32_t last_change_ms;
    };
    static StateData s_data { DeviceStateMachine::DeviceState::OK, 0, 0 };
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
    static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
#endif
}

namespace DeviceStateMachine {
    void init() {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        s_data.state = DeviceState::OK;
        s_data.reasons = 0;
        s_data.last_change_ms = 0;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
    }

    void set(DeviceState state, uint8_t reasons) {
        uint32_t now_ms = static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        s_data.state = state;
        s_data.reasons = reasons;
        s_data.last_change_ms = now_ms;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
    }

    DeviceState get() {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        DeviceState st = s_data.state;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        return st;
    }

    uint8_t reasons() {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        uint8_t r = s_data.reasons;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        return r;
    }

    uint32_t lastChangeMs() {
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskENTER_CRITICAL(&s_mux);
#else
        taskENTER_CRITICAL();
#endif
        uint32_t t = s_data.last_change_ms;
#if defined(CONFIG_FREERTOS_UNICORE) || defined(portMUX_INITIALIZER_UNLOCKED)
        taskEXIT_CRITICAL(&s_mux);
#else
        taskEXIT_CRITICAL();
#endif
        return t;
    }
}


