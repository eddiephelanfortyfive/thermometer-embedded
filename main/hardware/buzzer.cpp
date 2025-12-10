#include <main/hardware/buzzer.hpp>
#include <driver/gpio.h>
#include <freertos/task.h>

Buzzer::Buzzer(gpio_num_t buzzer_pin, bool active_high_level)
    : pin(buzzer_pin), active_high(active_high_level), current_on(false) {}

bool Buzzer::init() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    if (gpio_config(&io_conf) != ESP_OK) {
        return false;
    }
    off();
    return true;
}

void Buzzer::drive(bool enable) {
    int level = (enable == active_high) ? 1 : 0;
    gpio_set_level(pin, level);
    current_on = enable;
}

void Buzzer::on() {
    drive(true);
}

void Buzzer::off() {
    drive(false);
}

bool Buzzer::isOn() const {
    return current_on;
}

void Buzzer::buzzMs(uint32_t duration_ms) {
    on();
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    off();
}

void Buzzer::pulse(uint32_t on_ms, uint32_t off_ms, uint32_t repeat) {
    for (uint32_t i = 0; i < repeat; ++i) {
        on();
        vTaskDelay(pdMS_TO_TICKS(on_ms));
        off();
        if (i + 1 < repeat) {
            vTaskDelay(pdMS_TO_TICKS(off_ms));
        }
    }
}


