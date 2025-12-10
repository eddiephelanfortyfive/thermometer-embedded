#ifndef SENSOR_DATA_HPP
#define SENSOR_DATA_HPP

#include <cstdint>

// Fixed-size sensor sample for temperature
struct SensorData {
    float     temp_c; // temperature in Celsius
    uint32_t  ts_ms;  // sample timestamp in milliseconds
};

#endif // SENSOR_DATA_HPP


