#include <main/hardware/speaker.hpp>
#include <driver/ledc.h>
#include <freertos/task.h>

Speaker::Speaker(gpio_num_t pin, bool active_high, uint32_t freq_hz, uint8_t duty_percent)
    : pin_(pin), active_high_(active_high), freq_hz_(freq_hz), duty_percent_(duty_percent) {}

bool Speaker::init() {
    // Configure LEDC timer
    ledc_timer_config_t timer_conf = {};
    timer_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    timer_conf.duty_resolution = LEDC_TIMER_10_BIT; // 10-bit duty (0-1023)
    timer_conf.timer_num = timer_;
    timer_conf.freq_hz = freq_hz_;
    timer_conf.clk_cfg = LEDC_AUTO_CLK;
    if (ledc_timer_config(&timer_conf) != ESP_OK) {
        return false;
    }

    // Configure LEDC channel
    ledc_channel_config_t channel_conf = {};
    channel_conf.gpio_num = pin_;
    channel_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    channel_conf.channel = channel_;
    channel_conf.intr_type = LEDC_INTR_DISABLE;
    channel_conf.timer_sel = timer_;
    channel_conf.duty = 0; // start off
    channel_conf.hpoint = 0;
    if (ledc_channel_config(&channel_conf) != ESP_OK) {
        return false;
    }

    toneOff();
    return true;
}

void Speaker::toneOn() {
    const uint32_t max_duty = (1u << LEDC_TIMER_10_BIT) - 1;
    uint32_t duty = (max_duty * duty_percent_) / 100;
    if (!active_high_) {
        duty = max_duty - duty;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_);
}

void Speaker::toneOff() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_);
}

void Speaker::beepMs(uint32_t duration_ms) {
    toneOn();
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    toneOff();
}

void Speaker::pulse(uint32_t on_ms, uint32_t off_ms, uint32_t repeat) {
    for (uint32_t i = 0; i < repeat; ++i) {
        toneOn();
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        toneOff();
        if (i + 1 < repeat) {
            vTaskDelay(pdMS_TO_TICKS(off_ms));
        }
    }
}
