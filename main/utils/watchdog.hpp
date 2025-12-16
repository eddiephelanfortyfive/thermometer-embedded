#ifndef WATCHDOG_HPP
#define WATCHDOG_HPP

#include <esp_task_wdt.h>

namespace Watchdog {
    // Initialize TWDT (call once from app_main before tasks start)
    void init();
    // Subscribe calling task to TWDT
    void subscribe();
    // Feed the watchdog (reset timer) - call in task loop
    void feed();
}

#endif // WATCHDOG_HPP
