#ifndef TEMPERATURE_SENSOR_HPP
#define TEMPERATURE_SENSOR_HPP

#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <main/config/config.hpp>

class TemperatureSensor {
public:
    // Default to GPIO from config (GPIO 36 - ADC1_CH0 for LM35)
    explicit TemperatureSensor(gpio_num_t sensor_pin = Config::Hardware::Pins::temp_sensor_gpio);

    // Initialize ADC for LM35 analog temperature sensor
    bool init();

    // Read temperature in Celsius from LM35 (10mV per degree C)
    // Returns false on ADC read failure
    bool readTemperature(float& out_celsius);

private:
    gpio_num_t pin;
    adc_channel_t adc_channel;
    adc_oneshot_unit_handle_t adc_handle;
    bool initialized;
};

#endif // TEMPERATURE_SENSOR_HPP
