#ifndef I2C_RGB_LCD_HPP
#define I2C_RGB_LCD_HPP

#include <cstdint>
#include <driver/i2c.h>
#include <driver/gpio.h>
#include <main/utils/logger.hpp>

// Driver for DFRobot Gravity I2C 16x2 RGB LCD (DFR0464-class)
// Uses two I2C targets:
//  - LCD controller (HD44780-compatible over I2C), default 0x3E
//  - RGB backlight controller (PCA9633-compatible), default 0x60
//
// Notes:
//  - Static memory only; no dynamic allocation.
//  - The LCD protocol uses a control byte (0x80 for command, 0x40 for data)
//  - Backlight RGB uses PWM registers on PCA9633 (PWM0..2) and LEDOUT config.
class I2cRgbLcd {
public:
	// Construct with explicit pins and I2C addresses
	I2cRgbLcd(i2c_port_t port,
	          gpio_num_t sda,
	          gpio_num_t scl,
	          uint32_t i2c_clk_hz,
	          uint8_t lcd_addr_7bit,
	          uint8_t rgb_addr_7bit);

	// Initialize I2C (if not already) and the LCD + RGB backlight
	bool init();

	// Text output
	bool clear();
	bool home();
	bool setCursor(uint8_t col, uint8_t row); // row: 0..1
	bool writeChar(char c);
	bool writeStr(const char* str);

	// Display control
	bool displayOn(bool on);
	bool cursorOn(bool on);
	bool blinkOn(bool on);

	// Scrolling
	bool scrollDisplayLeft();
	bool scrollDisplayRight();

	// Backlight control (0..255 per channel)
	bool setBacklight(uint8_t r, uint8_t g, uint8_t b);

private:
	// Low-level helpers
	bool ensureI2cInstalled();
	bool lcdCommand(uint8_t cmd);
	bool lcdData(uint8_t data_byte);
	bool i2cWriteBytes(uint8_t addr7, const uint8_t* data, size_t len);
	void delayMs(uint32_t ms);

	// State
	i2c_port_t port;
	gpio_num_t sda;
	gpio_num_t scl;
	uint32_t i2c_clk_hz;
	uint8_t lcd_addr; // 7-bit
	uint8_t rgb_addr; // 7-bit
	bool i2c_ready;
	bool lcd_inited;
	bool display_on;
	bool cursor_on;
	bool blink_on;
};

#endif // I2C_RGB_LCD_HPP


