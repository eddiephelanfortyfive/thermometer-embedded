#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "utils/logger.hpp"

extern "C" void app_main(void)
{
    Logger::setLevel(LogLevel::INFO);
    LOG_INFO("MAIN", "%s", "---Didgital thermometer started---");

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t loop_period = pdMS_TO_TICKS(1000);

    for (;;) {
        vTaskDelayUntil(&last_wake_time, loop_period);
    }
}


