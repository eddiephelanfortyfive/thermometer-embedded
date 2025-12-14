#include <main/hardware/temperature_sensor.hpp>
#include <main/utils/logger.hpp>
#include <main/hardware/adc_shared.hpp>
#include <esp_adc/adc_oneshot.h>

static const char* TAG_SENSOR = "TempSensor";

// ESP32 ADC reference voltage (typically 3.3V)
// For ADC_ATTEN_DB_0 on ESP32, effective full-scale is ~1.1V
static constexpr float ADC_REF_VOLTAGE = 1.1f;
// ADC resolution (12-bit = 4095)
static constexpr int ADC_MAX_VALUE = 4095;
// Number of samples for averaging (reduces noise)
static constexpr int ADC_SAMPLES = 16;

TemperatureSensor::TemperatureSensor(gpio_num_t sensor_pin)
    : pin(sensor_pin),
      adc_channel(ADC_CHANNEL_0),
      adc_handle(nullptr),
      initialized(false) {
    // Map GPIO to ADC channel for ADC1
    // GPIO 32-39 are ADC1 channels on ESP32
    if (pin == GPIO_NUM_32) adc_channel = ADC_CHANNEL_4;
    else if (pin == GPIO_NUM_33) adc_channel = ADC_CHANNEL_5;
    else if (pin == GPIO_NUM_34) adc_channel = ADC_CHANNEL_6;
    else if (pin == GPIO_NUM_35) adc_channel = ADC_CHANNEL_7;
    else if (pin == GPIO_NUM_36) adc_channel = ADC_CHANNEL_0;
    else if (pin == GPIO_NUM_37) adc_channel = ADC_CHANNEL_1;
    else if (pin == GPIO_NUM_38) adc_channel = ADC_CHANNEL_2;
    else if (pin == GPIO_NUM_39) adc_channel = ADC_CHANNEL_3;
    else {
        // Default to channel 0 if pin not recognized
        adc_channel = ADC_CHANNEL_0;
        LOG_WARN(TAG_SENSOR, "GPIO %d not a valid ADC1 pin, defaulting to ADC_CHANNEL_0", pin);
    }
}

bool TemperatureSensor::init() {
    // Get shared ADC1 handle
    adc_handle = AdcShared::getAdc1Handle();
    if (adc_handle == nullptr) {
        LOG_ERROR(TAG_SENSOR, "Failed to get shared ADC1 handle");
        return false;
    }

    // Configure ADC channel
    adc_oneshot_chan_cfg_t config = {};
    config.bitwidth = ADC_BITWIDTH_12;
    // Use 0 dB attenuation for best resolution in the ~0.2–0.4V range typical of room temperatures
    config.atten = ADC_ATTEN_DB_0;
    
    esp_err_t ret = adc_oneshot_config_channel(adc_handle, adc_channel, &config);
    if (ret != ESP_OK) {
        LOG_ERROR(TAG_SENSOR, "Failed to configure ADC channel: %d", ret);
        return false;
    }

    initialized = true;
    LOG_INFO(TAG_SENSOR, "LM35 initialized on GPIO %d (ADC1_CH%d)", pin, adc_channel);
    return true;
}

bool TemperatureSensor::readTemperature(float& out_celsius) {
    if (!initialized || adc_handle == nullptr) {
        return false;
    }

    // Read multiple samples and average for noise reduction
    int32_t adc_sum = 0;
    int valid_samples = 0;
    
    for (int i = 0; i < ADC_SAMPLES; ++i) {
        int adc_raw = 0;
        esp_err_t ret = adc_oneshot_read(adc_handle, adc_channel, &adc_raw);
        if (ret == ESP_OK && adc_raw >= 0) {
            adc_sum += adc_raw;
            valid_samples++;
        }
    }

    if (valid_samples == 0) {
        LOG_ERROR(TAG_SENSOR, "Failed to read ADC");
        return false;
    }

    // Calculate average ADC value
    float adc_avg = static_cast<float>(adc_sum) / static_cast<float>(valid_samples);
    
    // Suppress per-read debug logging to reduce noise
    
    // Convert ADC reading to voltage (millivolts)
    // Voltage = (ADC_value / ADC_max) * Reference_voltage
    float voltage_mv = (adc_avg / static_cast<float>(ADC_MAX_VALUE)) * ADC_REF_VOLTAGE * 1000.0f;
    
    // Convert voltage to temperature using configurable gain/offset (no mV offset)
    out_celsius = (Config::Hardware::Temperature::gain_c_per_mv * voltage_mv);
    
    // Suppress per-read debug logging to reduce noise
    
    return true;
}
