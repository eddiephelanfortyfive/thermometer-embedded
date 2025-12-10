#ifndef SOIL_MOISTURE_SENSOR_HPP
#define SOIL_MOISTURE_SENSOR_HPP

#include <cstdint>
#include <hal/adc_types.h>
#include <esp_adc/adc_oneshot.h>
#include <main/models/moisture_data.hpp>

// DFRobot soil moisture (analog) sensor reader using ADC one-shot mode.
// Reads multiple samples, averages to get 'moisture_raw', and converts to
// percentage using user-provided calibration endpoints (dry/wet).
class SoilMoistureSensor {
public:
    struct Config {
        adc_unit_t   unit;          // ADC_UNIT_1 or ADC_UNIT_2 (ESP32 typically ADC_UNIT_1)
        adc_channel_t channel;      // ADC channel (e.g., ADC_CHANNEL_6 == GPIO34)
        adc_atten_t  attenuation;   // ADC_ATTEN_DB_0/2_5/6/11
        uint8_t      sample_count;  // number of samples to average (>=1)
        uint16_t     raw_dry;       // calibration raw value in air/dry
        uint16_t     raw_wet;       // calibration raw value in water/saturated
    };

    explicit SoilMoistureSensor(const Config& cfg);

    // Initialize ADC unit and channel. Should be called once at startup.
    bool init();

    // Update calibration endpoints
    void setCalibration(uint16_t raw_dry, uint16_t raw_wet);

    // Perform a blocking read: average N samples and compute percentage.
    // Returns false if ADC read fails.
    bool read(MoistureData& out_data);

private:
    float convertToPercent(int raw) const;

    Config cfg;
    adc_oneshot_unit_handle_t adc_handle;
};

#endif // SOIL_MOISTURE_SENSOR_HPP


