#ifndef COMMAND_TASK_HPP
#define COMMAND_TASK_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace CommandTask {
    // Create task that receives commands from command_queue and executes them
    // (e.g., updating runtime thresholds)
    void create(QueueHandle_t command_queue, QueueHandle_t thresholds_changed_queue);
}

#endif // COMMAND_TASK_HPP
