#ifndef DEVICE_STATE_HPP
#define DEVICE_STATE_HPP

#include <cstdint>

namespace DeviceStateMachine {
    enum class DeviceState : uint8_t { OK = 0, WARNING = 1, CRITICAL = 2 };

    enum ReasonFlags : uint8_t {
        REASON_NONE        = 0,
        REASON_TEMP_HIGH   = 1 << 0,
        REASON_TEMP_LOW    = 1 << 1,
        REASON_MOIST_LOW   = 1 << 2,
        REASON_MOIST_HIGH  = 1 << 3,
    };

    void init();
    void set(DeviceState state, uint8_t reasons);
    DeviceState get();
    uint8_t reasons();
    uint32_t lastChangeMs();
}

#endif // DEVICE_STATE_HPP


