#ifndef THRESHOLD_UPDATE_HPP
#define THRESHOLD_UPDATE_HPP

#include <cstdint>

struct ThresholdUpdate {
    uint8_t has_temp_low_warn;    float temp_low_warn_c;
    uint8_t has_temp_low_crit;    float temp_low_crit_c;
    uint8_t has_temp_high_warn;   float temp_high_warn_c;
    uint8_t has_temp_high_crit;   float temp_high_crit_c;
    uint8_t has_moist_low_warn;   float moisture_low_warn_pct;
    uint8_t has_moist_low_crit;   float moisture_low_crit_pct;
};

#endif // THRESHOLD_UPDATE_HPP

