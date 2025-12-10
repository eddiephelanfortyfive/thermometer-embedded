#ifndef TEMPERATURE_SENSOR_HPP
#define TEMPERATURE_SENSOR_HPP

#include <cstdint>
#include <driver/gpio.h>
#include <main/config/config.hpp>

class TemperatureSensor {
public:
    explicit TemperatureSensor(gpio_num_t sensor_pin = Config::Hardware::Pins::temp_sensor_gpio);

    // Initialize GPIO for OneWire and verify presence pulse
    bool init();

    // Read temperature in Celsius; returns false if CRC/presence fails
    bool readTemperature(float& out_celsius);

private:
    // OneWire primitives
    bool reset();
    void writeBit(uint8_t bit);
    uint8_t readBit();
    void writeByte(uint8_t byte);
    uint8_t readByte();

    // Utilities
    static uint8_t crc8Dallas(const uint8_t* data, uint8_t len);
    inline void driveLow();
    inline void releaseLine();
    inline int readLine() const;

    gpio_num_t pin;
};

#endif // TEMPERATURE_SENSOR_HPP


