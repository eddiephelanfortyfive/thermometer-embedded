#ifndef RUNTIME_THRESHOLDS_HPP
#define RUNTIME_THRESHOLDS_HPP

#include <cstdint>

namespace RuntimeThresholds {
    // Initialize runtime thresholds from NVS or use defaults from Config::Monitoring
    void init();

    // Temperature threshold getters
    float getTempLowWarn();
    float getTempLowCrit();
    float getTempHighWarn();
    float getTempHighCrit();

    // Temperature threshold setters (persist to NVS)
    bool setTempLowWarn(float value);
    bool setTempLowCrit(float value);
    bool setTempHighWarn(float value);
    bool setTempHighCrit(float value);

    // Moisture threshold getters
    float getMoistureLowWarn();
    float getMoistureLowCrit();
    float getMoistureHighWarn();
    float getMoistureHighCrit();

    // Moisture threshold setters (persist to NVS)
    bool setMoistureLowWarn(float value);
    bool setMoistureLowCrit(float value);
    bool setMoistureHighWarn(float value);
    bool setMoistureHighCrit(float value);
}

#endif // RUNTIME_THRESHOLDS_HPP
