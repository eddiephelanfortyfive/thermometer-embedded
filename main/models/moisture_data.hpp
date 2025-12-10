#ifndef MOISTURE_DATA_HPP
#define MOISTURE_DATA_HPP

#include <cstdint>

// Fixed-size soil moisture sample
struct MoistureData {
    uint16_t  moisture_raw;     // raw ADC reading
    float     moisture_percent; // 0.0..100.0 computed percentage
    uint32_t  ts_ms;            // sample timestamp in milliseconds
};

#endif // MOISTURE_DATA_HPP


