#include <main/hardware/led_display.hpp>
#include <driver/gpio.h>
#include <cstring>

// Segment bit positions in mask: a=0 b=1 c=2 d=3 e=4 f=5 g=6 dp=7
static constexpr uint8_t SEG_A = 1u << 0;
static constexpr uint8_t SEG_B = 1u << 1;
static constexpr uint8_t SEG_C = 1u << 2;
static constexpr uint8_t SEG_D = 1u << 3;
static constexpr uint8_t SEG_E = 1u << 4;
static constexpr uint8_t SEG_F = 1u << 5;
static constexpr uint8_t SEG_G = 1u << 6;
static constexpr uint8_t SEG_DP = 1u << 7;

LedDisplay::LedDisplay(const Pins& pins, bool common_anode)
    : pins(pins),
      common_anode(common_anode),
      left_mask(0),
      right_mask(0),
      brightness(100),
      pwm_counter(0),
      show_left(true) {}

bool LedDisplay::init() {
    configureGpio(pins.seg_a);
    configureGpio(pins.seg_b);
    configureGpio(pins.seg_c);
    configureGpio(pins.seg_d);
    configureGpio(pins.seg_e);
    configureGpio(pins.seg_f);
    configureGpio(pins.seg_g);
    configureGpio(pins.seg_dp);
    configureGpio(pins.digit_left);
    configureGpio(pins.digit_right);

    // Ensure all off
    setDigitEnable(true, false);
    setDigitEnable(false, false);
    setSegments(0);
    return true;
}

void LedDisplay::configureGpio(gpio_num_t pin) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    (void)gpio_config(&io_conf);
}

void LedDisplay::setDigits(uint8_t left, uint8_t right) {
    left_mask = encodeHex(left);
    right_mask = encodeHex(right);
}

void LedDisplay::setRaw(uint8_t left_mask_in, uint8_t right_mask_in) {
    left_mask = left_mask_in;
    right_mask = right_mask_in;
}

void LedDisplay::setBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    brightness = percent;
}

void LedDisplay::setDigitEnable(bool left, bool enable) {
    gpio_num_t digit_pin = left ? pins.digit_left : pins.digit_right;
    int level = (enable
                 ? (common_anode ? 1 : 0)
                 : (common_anode ? 0 : 1));
    gpio_set_level(digit_pin, level);
}

void LedDisplay::setSegments(uint8_t mask) {
    // For common-anode: segment ON = 0. For common-cathode: segment ON = 1.
    auto seg_level = [&](bool on) -> int {
        return on ? (common_anode ? 0 : 1) : (common_anode ? 1 : 0);
    };
    gpio_set_level(pins.seg_a, seg_level((mask & SEG_A) != 0));
    gpio_set_level(pins.seg_b, seg_level((mask & SEG_B) != 0));
    gpio_set_level(pins.seg_c, seg_level((mask & SEG_C) != 0));
    gpio_set_level(pins.seg_d, seg_level((mask & SEG_D) != 0));
    gpio_set_level(pins.seg_e, seg_level((mask & SEG_E) != 0));
    gpio_set_level(pins.seg_f, seg_level((mask & SEG_F) != 0));
    gpio_set_level(pins.seg_g, seg_level((mask & SEG_G) != 0));
    gpio_set_level(pins.seg_dp, seg_level((mask & SEG_DP) != 0));
}

void LedDisplay::update() {
    // Software PWM duty within 100-slice frame
    bool lit = (pwm_counter < brightness);
    pwm_counter = static_cast<uint8_t>((pwm_counter + 1) % 100);

    // Turn off both digits before changing segments to avoid ghosting
    setDigitEnable(true, false);
    setDigitEnable(false, false);

    if (lit) {
        if (show_left) {
            setSegments(left_mask);
            setDigitEnable(true, true);
        } else {
            setSegments(right_mask);
            setDigitEnable(false, true);
        }
    } else {
        // keep segments off during "off" PWM slices
        setSegments(0);
    }

    // Alternate digit next time
    show_left = !show_left;
}

uint8_t LedDisplay::encodeHex(uint8_t value) const {
    // 0-F segment map (abcdefg), dp off by default
    static constexpr uint8_t map[16] = {
        // a    b    c    d    e    f    g
        SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,              // 0
        SEG_B | SEG_C,                                              // 1
        SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,                      // 2
        SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,                      // 3
        SEG_B | SEG_C | SEG_F | SEG_G,                              // 4
        SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                      // 5
        SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,              // 6
        SEG_A | SEG_B | SEG_C,                                      // 7
        SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,      // 8
        SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,              // 9
        SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,              // A
        SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,                      // b
        SEG_A | SEG_D | SEG_E | SEG_F,                              // C
        SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,                      // d
        SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,                      // E
        SEG_A | SEG_E | SEG_F | SEG_G                               // F
    };
    if (value < 16) {
        return map[value];
    }
    return 0; // blank for unsupported values
}


