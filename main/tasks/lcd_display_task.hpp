#ifndef LCD_DISPLAY_TASK_HPP
#define LCD_DISPLAY_TASK_HPP

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Queue item for updating the LCD.
// All fields are fixed-size to avoid dynamic allocation.
struct LcdUpdate {
	char     line1[17];         // null-terminated if shorter; will be truncated to 16
	char     line2[17];         // null-terminated if shorter; will be truncated to 16
	uint8_t  r;                 // backlight red   (0..255)
	uint8_t  g;                 // backlight green (0..255)
	uint8_t  b;                 // backlight blue  (0..255)
	uint8_t  set_backlight;     // 0=no change, nonzero=apply r/g/b
	uint8_t  clear_first;       // 0=write over, nonzero=clear display before writing
};

namespace LcdDisplayTask {
	// Create a static FreeRTOS task that initializes the I2C RGB LCD and
	// processes LcdUpdate messages from the provided queue.
	// The task uses I2C pins/addresses from Config::Hardware::Lcd.
	void create(QueueHandle_t lcd_queue);
}

#endif // LCD_DISPLAY_TASK_HPP


