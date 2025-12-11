#ifndef VIBRATION_MODULE_HPP
#define VIBRATION_MODULE_HPP

#include <cstdint>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>

// DFRobot Gravity Vibration Module (DFR0440) controller
// Controls vibration motor via digital GPIO
class VibrationModule {
public:
    // active_high: true if driving GPIO high turns the vibrator ON
    explicit VibrationModule(gpio_num_t vibration_pin, bool active_high = true);

    // Configure GPIO direction; starts OFF
    bool init();

    // Immediate control
    void on();
    void off();
    bool isOn() const;

    // Blocking vibration for a duration (uses vTaskDelay)
    void vibrateMs(uint32_t duration_ms);

    // Blocking pulse pattern: repeat times of on/off
    void pulse(uint32_t on_ms, uint32_t off_ms, uint32_t repeat);

private:
    inline void drive(bool enable);

    gpio_num_t pin;
    bool active_high;
    bool current_on;
};

#endif // VIBRATION_MODULE_HPP
