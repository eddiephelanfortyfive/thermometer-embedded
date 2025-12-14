#ifndef TEMPERATURE_DATA_HPP
#define TEMPERATURE_DATA_HPP

#include <cstdint>

// Fixed-size sample for temperature data
struct TemperatureData {
    float     temp_c; // temperature in Celsius
    uint32_t  ts_ms;  // sample timestamp in milliseconds
};

#endif // TEMPERATURE_DATA_HPP
