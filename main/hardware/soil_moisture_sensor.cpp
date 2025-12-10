#include <main/hardware/soil_moisture_sensor.hpp>
#include <main/hardware/adc_shared.hpp>
#include <algorithm>

SoilMoistureSensor::SoilMoistureSensor(const Config& cfg_in)
    : cfg(cfg_in), adc_handle(nullptr) {}

bool SoilMoistureSensor::init() {
    // Use shared ADC1 handle if using ADC_UNIT_1, otherwise create new handle
    if (cfg.unit == ADC_UNIT_1) {
        adc_handle = AdcShared::getAdc1Handle();
        if (adc_handle == nullptr) {
            return false;
        }
    } else {
        // For ADC_UNIT_2, create a new handle (not shared)
        adc_oneshot_unit_init_cfg_t unit_cfg = {};
        unit_cfg.unit_id = cfg.unit;
        unit_cfg.ulp_mode = ADC_ULP_MODE_DISABLE;
        if (adc_oneshot_new_unit(&unit_cfg, &adc_handle) != ESP_OK) {
            return false;
        }
    }
    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    chan_cfg.atten = cfg.attenuation;
    if (adc_oneshot_config_channel(adc_handle, cfg.channel, &chan_cfg) != ESP_OK) {
        return false;
    }
    if (cfg.sample_count == 0) {
        cfg.sample_count = 1;
    }
    return true;
}

void SoilMoistureSensor::setCalibration(uint16_t raw_dry, uint16_t raw_wet) {
    cfg.raw_dry = raw_dry;
    cfg.raw_wet = raw_wet;
}

bool SoilMoistureSensor::read(MoistureData& out_data) {
    if (!adc_handle) {
        return false;
    }
    uint32_t sum = 0;
    for (uint8_t i = 0; i < cfg.sample_count; ++i) {
        int raw = 0;
        if (adc_oneshot_read(adc_handle, cfg.channel, &raw) != ESP_OK) {
            return false;
        }
        sum += static_cast<uint32_t>(raw);
    }
    uint16_t avg = static_cast<uint16_t>(sum / cfg.sample_count);
    out_data.moisture_raw = avg;
    out_data.moisture_percent = convertToPercent(avg);
    // Timestamp should be supplied by caller; set 0 here
    out_data.ts_ms = 0;
    return true;
}

float SoilMoistureSensor::convertToPercent(int raw) const {
    // Map raw to 0..100 using calibration endpoints.
    // Many capacitive sensors report LOWER raw when wet. We handle both orders.
    const int dry = static_cast<int>(cfg.raw_dry);
    const int wet = static_cast<int>(cfg.raw_wet);
    if (dry == wet) {
        return 0.0f;
    }
    float percent = 0.0f;
    if (dry > wet) {
        // lower raw = wetter
        percent = 100.0f * (static_cast<float>(dry - raw) / static_cast<float>(dry - wet));
    } else {
        // higher raw = wetter
        percent = 100.0f * (static_cast<float>(raw - dry) / static_cast<float>(wet - dry));
    }
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;
    return percent;
}


