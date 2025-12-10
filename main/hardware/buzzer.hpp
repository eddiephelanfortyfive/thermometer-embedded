#ifndef BUZZER_HPP
#define BUZZER_HPP

#include <cstdint>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>

class Buzzer {
public:
    // active_high: true if driving GPIO high turns the vibrator ON
    explicit Buzzer(gpio_num_t buzzer_pin = GPIO_NUM_26, bool active_high = true);

    // Configure GPIO direction; starts OFF
    bool init();

    // Immediate control
    void on();
    void off();
    bool isOn() const;

    // Blocking buzz for a duration (uses vTaskDelay)
    void buzzMs(uint32_t duration_ms);

    // Blocking pulse pattern: repeat times of on/off
    void pulse(uint32_t on_ms, uint32_t off_ms, uint32_t repeat);

private:
    inline void drive(bool enable);

    gpio_num_t pin;
    bool active_high;
    bool current_on;
};

#endif // BUZZER_HPP


