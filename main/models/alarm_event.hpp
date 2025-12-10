#ifndef ALARM_EVENT_HPP
#define ALARM_EVENT_HPP

#include <cstdint>

// Fixed-size event describing an alarm condition
struct AlarmEvent {
    uint32_t timestamp_ms;   // event time in milliseconds
    float     temperature_c; // temperature at event time
    uint8_t   type;          // implementation-defined alarm type code
};

#endif // ALARM_EVENT_HPP


