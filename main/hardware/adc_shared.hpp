#ifndef ADC_SHARED_HPP
#define ADC_SHARED_HPP

#include <esp_adc/adc_oneshot.h>
#include <hal/adc_types.h>

// Shared ADC1 handle manager for multiple sensors
// ESP-IDF only allows one ADC1 handle, so sensors must share it
namespace AdcShared {
    // Get or create the shared ADC1 handle
    // Returns nullptr on failure
    adc_oneshot_unit_handle_t getAdc1Handle();
    
    // Release the shared ADC1 handle (call when done, though typically never needed)
    void releaseAdc1Handle();
}

#endif // ADC_SHARED_HPP

