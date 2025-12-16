#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <cstdint>

// Fixed-size command container for inter-task messaging
struct Command {
    uint32_t timestamp_ms; // time command was created
    int32_t  type;         // implementation-defined command type
    float    value;        // optional numeric value
};

// Command type encoding for inter-task and MQTT commands
// Negative values: external MQTT commands (threshold updates)
// Non-negative values: internal commands (alert publishing)
enum class CommandType : int32_t {
    // Internal commands (handled by cloud task for alert publishing)
    ALERT_OK = 0,
    ALERT_WARNING = 1,
    ALERT_CRITICAL = 2,
    
    // External MQTT commands (handled by command task)
    UPDATE_TEMP_LOW_WARN = -1,
    UPDATE_TEMP_LOW_CRIT = -2,
    UPDATE_TEMP_HIGH_WARN = -3,
    UPDATE_TEMP_HIGH_CRIT = -4,
    UPDATE_MOISTURE_LOW_WARN = -5,
    UPDATE_MOISTURE_LOW_CRIT = -6,
};

#endif // COMMAND_HPP


