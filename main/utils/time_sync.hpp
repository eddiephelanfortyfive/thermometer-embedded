// Lightweight SNTP time sync helper with fixed-length timestamp formatting.
#ifndef TIME_SYNC_HPP
#define TIME_SYNC_HPP

#include <cstddef>

namespace TimeSync {
    // Initialize SNTP once (idempotent). Safe to call repeatedly.
    void init();

    // Returns true if system time is considered valid (SNTP synced or RTC set).
    bool isSynced();

    // Block until time is synced or timeout_ms elapses. Returns true if synced.
    bool waitForSync(unsigned int timeout_ms);

    // Writes a fixed-length timestamp string into out buffer.
    // Format: YYYYMMDDHHMMSS (14 chars, UTC), e.g. "20251211123045".
    // Always null-terminates when out_size > 0.
    void formatFixedTimestamp(char* out, std::size_t out_size);
}

#endif // TIME_SYNC_HPP


