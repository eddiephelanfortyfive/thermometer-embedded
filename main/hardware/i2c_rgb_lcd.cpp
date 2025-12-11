#include <main/hardware/i2c_rgb_lcd.hpp>
#include <main/utils/logger.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

// LCD command helpers (HD44780-like)
static constexpr uint8_t LCD_CMD_CLEAR_DISPLAY = 0x01;
static constexpr uint8_t LCD_CMD_RETURN_HOME   = 0x02;
static constexpr uint8_t LCD_CMD_ENTRY_MODE    = 0x04;
static constexpr uint8_t LCD_CMD_DISPLAY_CTRL  = 0x08;
static constexpr uint8_t LCD_CMD_CURSOR_SHIFT  = 0x10;
static constexpr uint8_t LCD_CMD_FUNCTION_SET  = 0x20;
static constexpr uint8_t LCD_CMD_SET_DDRAM     = 0x80;

// Entry mode flags
static constexpr uint8_t LCD_ENTRY_INCREMENT   = 0x02;
static constexpr uint8_t LCD_ENTRY_SHIFT_OFF   = 0x00;

// Display control flags
static constexpr uint8_t LCD_DISPLAY_ON        = 0x04;
static constexpr uint8_t LCD_CURSOR_ON         = 0x02;
static constexpr uint8_t LCD_BLINK_ON          = 0x01;

// Function set flags: 2-line, 5x8 dots
static constexpr uint8_t LCD_FUNC_2LINE_5x8    = 0x08; // N=1, F=0

// I2C LCD (e.g., ST7032/AIP31068 style) control bytes:
// bit7=Co (1 means another control byte follows), bit6=RS (0=command,1=data)
// For single command writes, Co must be 0 so the next byte is interpreted as data (instruction).
static constexpr uint8_t LCD_CTRL_COMMAND      = 0x00; // Co=0, RS=0 -> one command byte follows
static constexpr uint8_t LCD_CTRL_DATA         = 0x40;

// PCA9633 registers (RGB backlight)
static constexpr uint8_t PCA9633_MODE1         = 0x00;
static constexpr uint8_t PCA9633_MODE2         = 0x01;
static constexpr uint8_t PCA9633_PWM0          = 0x02; // PWM0..PWM3 at 0x02..0x05
static constexpr uint8_t PCA9633_LEDOUT        = 0x08;
// LEDOUT value 0xAA: all 4 channels controlled by individual PWM
static constexpr uint8_t PCA9633_LEDOUT_PWMALL = 0xAA;

static const char* TAG = "I2C_RGB_LCD";

I2cRgbLcd::I2cRgbLcd(i2c_port_t port_in,
                     gpio_num_t sda_in,
                     gpio_num_t scl_in,
                     uint32_t i2c_clk_hz_in,
                     uint8_t lcd_addr_7bit_in,
                     uint8_t rgb_addr_7bit_in)
	: port(port_in),
	  sda(sda_in),
	  scl(scl_in),
	  i2c_clk_hz(i2c_clk_hz_in),
	  lcd_addr(lcd_addr_7bit_in),
	  rgb_addr(rgb_addr_7bit_in),
	  i2c_ready(false),
	  lcd_inited(false),
	  display_on(true),
	  cursor_on(false),
	  blink_on(false) {}

bool I2cRgbLcd::ensureI2cInstalled() {
	if (i2c_ready) return true;
	i2c_config_t cfg{};
	cfg.mode = I2C_MODE_MASTER;
	cfg.sda_io_num = sda;
	cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
	cfg.scl_io_num = scl;
	cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
	cfg.master.clk_speed = i2c_clk_hz;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	cfg.clk_flags = 0;
#endif
	esp_err_t err = i2c_param_config(port, &cfg);
	if (err != ESP_OK) {
		LOG_ERROR(TAG, "i2c_param_config failed: %d", err);
		return false;
	}
	err = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
		LOG_ERROR(TAG, "i2c_driver_install failed: %d", err);
		return false;
	}
	i2c_ready = true;
	return true;
}

bool I2cRgbLcd::i2cWriteBytes(uint8_t addr7, const uint8_t* data, size_t len) {
	// Use a static buffer for the I2C command link to avoid heap allocation.
	static uint8_t link_buffer[128];
	i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(link_buffer, sizeof(link_buffer));
	if (cmd == nullptr) return false;
	esp_err_t err = i2c_master_start(cmd);
	err |= i2c_master_write_byte(cmd, static_cast<uint8_t>((addr7 << 1) | I2C_MASTER_WRITE), true);
	err |= i2c_master_write(cmd, const_cast<uint8_t*>(data), len, true);
	err |= i2c_master_stop(cmd);
	if (err == ESP_OK) {
		err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(100));
	}
#if CONFIG_IDF_TARGET_ESP32 // no-op; ensure compat
#endif
	i2c_cmd_link_delete(cmd);
	if (err != ESP_OK) {
		LOG_WARN(TAG, "I2C write to 0x%02X failed: %d", addr7, err);
		return false;
	}
	return true;
}

void I2cRgbLcd::delayMs(uint32_t ms) {
	vTaskDelay(pdMS_TO_TICKS(ms));
}

bool I2cRgbLcd::lcdCommand(uint8_t cmd) {
	uint8_t buf[2] = { LCD_CTRL_COMMAND, cmd };
	return i2cWriteBytes(lcd_addr, buf, sizeof(buf));
}

bool I2cRgbLcd::lcdData(uint8_t data_byte) {
	uint8_t buf[2] = { LCD_CTRL_DATA, data_byte };
	return i2cWriteBytes(lcd_addr, buf, sizeof(buf));
}

bool I2cRgbLcd::init() {
	if (!ensureI2cInstalled()) return false;

	// LCD init sequence (based on common I2C HD44780 variants)
	delayMs(50);
	if (!lcdCommand(LCD_CMD_FUNCTION_SET | LCD_FUNC_2LINE_5x8)) return false;
	delayMs(5);
	if (!lcdCommand(LCD_CMD_DISPLAY_CTRL | LCD_DISPLAY_ON)) return false; // display on, cursor/blink off
	if (!lcdCommand(LCD_CMD_CLEAR_DISPLAY)) return false;
	delayMs(2);
	if (!lcdCommand(LCD_CMD_ENTRY_MODE | LCD_ENTRY_INCREMENT | LCD_ENTRY_SHIFT_OFF)) return false;

	// RGB backlight init (PCA9633-like)
	{
		uint8_t mode1[2] = { PCA9633_MODE1, 0x00 }; // normal mode
		(void)i2cWriteBytes(rgb_addr, mode1, sizeof(mode1));
		uint8_t mode2[2] = { PCA9633_MODE2, 0x00 }; // default
		(void)i2cWriteBytes(rgb_addr, mode2, sizeof(mode2));
		uint8_t ledout[2] = { PCA9633_LEDOUT, PCA9633_LEDOUT_PWMALL };
		(void)i2cWriteBytes(rgb_addr, ledout, sizeof(ledout));
		// Default backlight: dim white
		(void)setBacklight(128, 128, 128);
	}

	lcd_inited = true;
	display_on = true;
	cursor_on = false;
	blink_on = false;
	LOG_INFO(TAG, "%s", "I2C RGB LCD initialized");
	return true;
}

bool I2cRgbLcd::clear() {
	if (!lcd_inited) return false;
	bool ok = lcdCommand(LCD_CMD_CLEAR_DISPLAY);
	delayMs(2);
	return ok;
}

bool I2cRgbLcd::home() {
	if (!lcd_inited) return false;
	bool ok = lcdCommand(LCD_CMD_RETURN_HOME);
	delayMs(2);
	return ok;
}

bool I2cRgbLcd::setCursor(uint8_t col, uint8_t row) {
	if (!lcd_inited) return false;
	if (row > 1) row = 1;
	uint8_t addr = (row == 0) ? (0x00 + col) : (0x40 + col);
	return lcdCommand(LCD_CMD_SET_DDRAM | addr);
}

bool I2cRgbLcd::writeChar(char c) {
	if (!lcd_inited) return false;
	bool ok = lcdData(static_cast<uint8_t>(c));
	if (ok) {
		// Allow the controller time to process the data write.
		// Typical exec time is ~40 µs; one tick (~1 ms) is safe and simple.
		delayMs(1);
	}
	return ok;
}

bool I2cRgbLcd::writeStr(const char* str) {
	if (!lcd_inited || str == nullptr) return false;
	for (const char* p = str; *p; ++p) {
		if (!writeChar(*p)) return false;
	}
	return true;
}

bool I2cRgbLcd::displayOn(bool on) {
	if (!lcd_inited) return false;
	display_on = on;
	uint8_t ctrl = (display_on ? LCD_DISPLAY_ON : 0) |
	               (cursor_on ? LCD_CURSOR_ON : 0) |
	               (blink_on ? LCD_BLINK_ON : 0);
	return lcdCommand(LCD_CMD_DISPLAY_CTRL | ctrl);
}

bool I2cRgbLcd::cursorOn(bool on) {
	if (!lcd_inited) return false;
	cursor_on = on;
	uint8_t ctrl = (display_on ? LCD_DISPLAY_ON : 0) |
	               (cursor_on ? LCD_CURSOR_ON : 0) |
	               (blink_on ? LCD_BLINK_ON : 0);
	return lcdCommand(LCD_CMD_DISPLAY_CTRL | ctrl);
}

bool I2cRgbLcd::blinkOn(bool on) {
	if (!lcd_inited) return false;
	blink_on = on;
	uint8_t ctrl = (display_on ? LCD_DISPLAY_ON : 0) |
	               (cursor_on ? LCD_CURSOR_ON : 0) |
	               (blink_on ? LCD_BLINK_ON : 0);
	return lcdCommand(LCD_CMD_DISPLAY_CTRL | ctrl);
}

bool I2cRgbLcd::scrollDisplayLeft() {
	if (!lcd_inited) return false;
	return lcdCommand(LCD_CMD_CURSOR_SHIFT | 0x08); // display shift, left
}

bool I2cRgbLcd::scrollDisplayRight() {
	if (!lcd_inited) return false;
	return lcdCommand(LCD_CMD_CURSOR_SHIFT | 0x0C); // display shift, right
}

bool I2cRgbLcd::setBacklight(uint8_t r, uint8_t g, uint8_t b) {
	// Many DFRobot RGB boards wire PWM channels as: PWM0=B, PWM1=G, PWM2=R.
	// Adjust mapping so (r,g,b) yields expected colors.
	uint8_t buf_b[2] = { static_cast<uint8_t>(PCA9633_PWM0 + 0), b };
	uint8_t buf_g[2] = { static_cast<uint8_t>(PCA9633_PWM0 + 1), g };
	uint8_t buf_r[2] = { static_cast<uint8_t>(PCA9633_PWM0 + 2), r };
	bool ok = true;
	ok &= i2cWriteBytes(rgb_addr, buf_b, sizeof(buf_b));
	ok &= i2cWriteBytes(rgb_addr, buf_g, sizeof(buf_g));
	ok &= i2cWriteBytes(rgb_addr, buf_r, sizeof(buf_r));
	return ok;
}


