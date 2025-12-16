#ifndef ALARM_EVENT_HPP
#define ALARM_EVENT_HPP

#include <cstdint>

// Strongly-typed alarm event types
enum class AlarmType : uint8_t {
    WARNING = 0,   // Warning level alarm (single short beep)
    CRITICAL = 3,  // Critical level alarm (repeating triple beep)
    CLEAR = 255    // Clear/stop alarm
};

// Fixed-size event describing an alarm condition
struct AlarmEvent {
    uint32_t timestamp_ms;   // event time in milliseconds
    float     temperature_c; // temperature at event time
    AlarmType type;          // alarm type (WARNING, CRITICAL, or CLEAR)
};

#endif // ALARM_EVENT_HPP


