#include <main/tasks/lcd_display_task.hpp>
#include <main/hardware/i2c_rgb_lcd.hpp>
#include <main/config/config.hpp>
#include <main/utils/logger.hpp>
#include <freertos/task.h>
#include <cstring>
#include <driver/i2c.h>

namespace {
	static const char* TAG = "LCD_TASK";

	// Static task objects (no heap)
	static StaticTask_t s_task_tcb;
	static StackType_t s_task_stack[3072 / sizeof(StackType_t)];
	static QueueHandle_t s_lcd_queue = nullptr;
	static bool s_task_created = false;

	static bool installI2cIfNeeded() {
		i2c_config_t cfg{};
		cfg.mode = I2C_MODE_MASTER;
		cfg.sda_io_num = Config::Hardware::Lcd::sda;
		cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
		cfg.scl_io_num = Config::Hardware::Lcd::scl;
		cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
		cfg.master.clk_speed = Config::Hardware::Lcd::clk_hz;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
		cfg.clk_flags = 0;
#endif
		esp_err_t err = i2c_param_config(static_cast<i2c_port_t>(Config::Hardware::Lcd::i2c_port), &cfg);
		if (err != ESP_OK) {
			LOG_ERROR(TAG, "i2c_param_config failed: %d", err);
			return false;
		}
		err = i2c_driver_install(static_cast<i2c_port_t>(Config::Hardware::Lcd::i2c_port), I2C_MODE_MASTER, 0, 0, 0);
		if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
			LOG_ERROR(TAG, "i2c_driver_install failed: %d", err);
			return false;
		}
		return true;
	}

	static void scanI2cBus() {
		if (!installI2cIfNeeded()) return;
		static uint8_t link_buffer[128];
		const i2c_port_t port = static_cast<i2c_port_t>(Config::Hardware::Lcd::i2c_port);
		LOG_INFO(TAG, "Scanning I2C port=%d, SDA=%d, SCL=%d, %lu Hz",
		         static_cast<int>(port),
		         static_cast<int>(Config::Hardware::Lcd::sda),
		         static_cast<int>(Config::Hardware::Lcd::scl),
		         static_cast<unsigned long>(Config::Hardware::Lcd::clk_hz));
		for (uint8_t addr = 0x03; addr <= 0x77; ++addr) {
			i2c_cmd_handle_t cmd = i2c_cmd_link_create_static(link_buffer, sizeof(link_buffer));
			if (cmd == nullptr) {
				LOG_ERROR(TAG, "%s", "I2C static link buffer too small for scan");
				break;
			}
			esp_err_t err = i2c_master_start(cmd);
			err |= i2c_master_write_byte(cmd, static_cast<uint8_t>((addr << 1) | I2C_MASTER_WRITE), true);
			err |= i2c_master_stop(cmd);
			if (err == ESP_OK) {
				err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(50));
			}
			i2c_cmd_link_delete(cmd);
			if (err == ESP_OK) {
				LOG_INFO(TAG, "I2C device ACK at 0x%02X", addr);
			}
		}
		LOG_INFO(TAG, "Expected LCD at 0x%02X, RGB at 0x%02X",
		         static_cast<unsigned>(Config::Hardware::Lcd::lcd_addr),
		         static_cast<unsigned>(Config::Hardware::Lcd::rgb_addr));
		// Free the temp driver so LCD driver can (re)install cleanly
		(void)i2c_driver_delete(port);
	}

	// Static LCD driver instance using Config defaults
	static I2cRgbLcd s_lcd(
		static_cast<i2c_port_t>(Config::Hardware::Lcd::i2c_port),
		Config::Hardware::Lcd::sda,
		Config::Hardware::Lcd::scl,
		Config::Hardware::Lcd::clk_hz,
		Config::Hardware::Lcd::lcd_addr,
		Config::Hardware::Lcd::rgb_addr
	);

	static void safeWriteLine(uint8_t col, uint8_t row, const char* text) {
		if (!s_lcd.setCursor(col, row)) {
			LOG_WARN(TAG, "%s", "setCursor failed");
		}
		// Give the controller a short moment after addressing before data writes
		vTaskDelay(pdMS_TO_TICKS(2));
		// Some I2C LCD backpacks drop the very first data byte after a DDRAM set.
		// Prime with a dummy space, then reposition, to guarantee first real char shows.
		(void)s_lcd.writeChar(' ');
		vTaskDelay(pdMS_TO_TICKS(1));
		(void)s_lcd.setCursor(col, row);
		vTaskDelay(pdMS_TO_TICKS(1));
		// Ensure writing at most 16 chars
		char buf[17];
		std::memset(buf, 0, sizeof(buf));
		if (text != nullptr) {
			std::strncpy(buf, text, 16);
		}
		// Write the text
		if (!s_lcd.writeStr(buf)) {
			LOG_WARN(TAG, "%s", "writeStr failed");
		}
		// Overwrite any leftover characters from previous longer string
		size_t used = 0;
		if (text != nullptr) {
			// strnlen to cap at 16
			const char* t = text;
			while (used < 16 && t[used] != '\0') used++;
		}
		if (used < 16) {
			char spaces[17];
			size_t remain = 16 - used;
			for (size_t i = 0; i < remain; ++i) spaces[i] = ' ';
			spaces[remain] = '\0';
			(void)s_lcd.writeStr(spaces);
		}
	}

	static void taskFunction(void* arg) {
		(void)arg;
		LOG_INFO(TAG, "%s", "LCD Display Task started");

		// Scan bus first to help diagnose wiring/addresses
		scanI2cBus();

		if (!s_lcd.init()) {
			LOG_ERROR(TAG, "%s", "LCD init failed");
			vTaskDelete(nullptr);
			return;
		}

		// Default backlight from config
		(void)s_lcd.setBacklight(
			Config::Hardware::Lcd::backlight_r,
			Config::Hardware::Lcd::backlight_g,
			Config::Hardware::Lcd::backlight_b
		);

		// Show boot message
		(void)s_lcd.clear();
		safeWriteLine(0, 0, "Thermometer");
		safeWriteLine(0, 1, Config::Device::id);

		LcdUpdate update{};
		for (;;) {
			// Wait indefinitely for updates
			if (xQueueReceive(s_lcd_queue, &update, portMAX_DELAY) == pdTRUE) {
				if (update.set_backlight) {
					(void)s_lcd.setBacklight(update.r, update.g, update.b);
				}
				if (update.clear_first) {
					(void)s_lcd.clear();
					// HD44780 requires ~1.5ms after clear/home
					vTaskDelay(pdMS_TO_TICKS(2));
				}
				safeWriteLine(0, 0, update.line1);
				safeWriteLine(0, 1, update.line2);
			}
		}
	}
}

namespace LcdDisplayTask {
	void create(QueueHandle_t lcd_queue) {
		if (s_task_created) {
			LOG_WARN(TAG, "%s", "LCD task already created; ignoring duplicate create()");
			return;
		}
		s_lcd_queue = lcd_queue;
		xTaskCreateStatic(taskFunction,
		                  "lcd_display",
		                  sizeof(s_task_stack) / sizeof(StackType_t),
		                  nullptr,
		                  Config::TaskPriorities::NORMAL,
		                  s_task_stack,
		                  &s_task_tcb);
		s_task_created = true;
	}
}


