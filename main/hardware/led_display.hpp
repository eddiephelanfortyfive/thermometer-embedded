#ifndef LED_DISPLAY_HPP
#define LED_DISPLAY_HPP

#include <cstdint>
#include <driver/gpio.h>

// Driver for two 1-digit 7-segment LED displays (with decimal point),
// wired in a shared-segment multiplex arrangement with two digit selects.
// Supports both common-anode and common-cathode modules (e.g., F5101BH).
//
// Usage:
//   LedDisplay::Pins pins{ a,b,c,d,e,f,g,dp, digit_left, digit_right };
//   LedDisplay display(pins, /*common_anode=*/true);
//   display.init();
//   display.setDigits(1, 2);
//   In a fast loop/timer (~1 kHz): display.update();
//
class LedDisplay {
public:
    struct Pins {
        gpio_num_t seg_a;
        gpio_num_t seg_b;
        gpio_num_t seg_c;
        gpio_num_t seg_d;
        gpio_num_t seg_e;
        gpio_num_t seg_f;
        gpio_num_t seg_g;
        gpio_num_t seg_dp;
        gpio_num_t digit_left;
        gpio_num_t digit_right;
    };

    LedDisplay(const Pins& pins, bool common_anode);

    bool init();

    // Show hexadecimal digits (0-15) on left/right. Values > 15 will blank.
    void setDigits(uint8_t left, uint8_t right);

    // Raw segment control for left/right; bit0=a ... bit6=g, bit7=dp. 1=on (logical).
    void setRaw(uint8_t left_mask, uint8_t right_mask);

    // Brightness 0..100 (%). Software PWM within update() time slices.
    void setBrightness(uint8_t percent);

    // Must be called periodically (~1 kHz). Handles multiplexing + brightness.
    void update();

private:
    void configureGpio(gpio_num_t pin);
    void setDigitEnable(bool left, bool enable);
    void setSegments(uint8_t mask);
    uint8_t encodeHex(uint8_t value) const;

    Pins pins;
    bool common_anode;         // true for common-anode, false for common-cathode
    uint8_t left_mask;         // desired segments for left digit (logical on bits)
    uint8_t right_mask;        // desired segments for right digit (logical on bits)
    uint8_t brightness;        // 0..100
    uint8_t pwm_counter;       // 0..99
    bool show_left;            // which digit is currently being driven
};

#endif // LED_DISPLAY_HPP


