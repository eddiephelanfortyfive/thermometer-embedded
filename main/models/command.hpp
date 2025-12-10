#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <cstdint>

// Fixed-size command container for inter-task messaging
struct Command {
    uint32_t timestamp_ms; // time command was created
    int32_t  type;         // implementation-defined command type
    float    value;        // optional numeric value
};

#endif // COMMAND_HPP


