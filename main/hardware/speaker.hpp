#ifndef SPEAKER_HPP
#define SPEAKER_HPP

#include <cstdint>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>

// Simple LEDC-based speaker driver for on/off tones
class Speaker {
public:
    // active_high: true if driving GPIO high turns the speaker ON
    Speaker(gpio_num_t pin, bool active_high = true,
            uint32_t freq_hz = 2500, uint8_t duty_percent = 50);

    bool init();

    void toneOn();
    void toneOff();
    void beepMs(uint32_t duration_ms);
    void pulse(uint32_t on_ms, uint32_t off_ms, uint32_t repeat);

private:
    gpio_num_t pin_;
    bool active_high_;
    uint32_t freq_hz_;
    uint8_t duty_percent_;
    ledc_timer_t timer_ = LEDC_TIMER_0;
    ledc_channel_t channel_ = LEDC_CHANNEL_0;
};

#endif // SPEAKER_HPP
