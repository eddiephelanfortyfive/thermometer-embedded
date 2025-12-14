#ifndef PLANT_MONITORING_TASK_HPP
#define PLANT_MONITORING_TASK_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace PlantMonitoringTask {
    void create(QueueHandle_t sensor_queue,
                QueueHandle_t moisture_queue,
                QueueHandle_t alarm_queue,
                QueueHandle_t lcd_queue,
                QueueHandle_t command_queue,
                QueueHandle_t config_queue);

    // Get current runtime thresholds (for ACK publishing)
    struct ThresholdSnapshot {
        float temp_low_warn_c;
        float temp_low_crit_c;
        float temp_high_warn_c;
        float temp_high_crit_c;
        float moisture_low_warn_pct;
        float moisture_low_crit_pct;
    };
    ThresholdSnapshot getCurrentThresholds();
}

#endif // PLANT_MONITORING_TASK_HPP


