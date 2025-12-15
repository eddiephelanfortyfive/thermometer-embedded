#include <main/hardware/adc_shared.hpp>
#include <esp_adc/adc_oneshot.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace AdcShared {
    static adc_oneshot_unit_handle_t s_adc1_handle = nullptr;
    static StaticSemaphore_t s_mutex_buffer;
    static SemaphoreHandle_t s_mutex = nullptr;
    
    adc_oneshot_unit_handle_t getAdc1Handle() {
        // Initialize mutex on first call
        if (s_mutex == nullptr) {
            s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
        }
        
        if (s_adc1_handle != nullptr) {
            return s_adc1_handle;
        }
        
        adc_oneshot_unit_init_cfg_t init_config = {};
        init_config.unit_id = ADC_UNIT_1;
        init_config.ulp_mode = ADC_ULP_MODE_DISABLE;
        
        esp_err_t ret = adc_oneshot_new_unit(&init_config, &s_adc1_handle);
        if (ret != ESP_OK) {
            return nullptr;
        }
        
        return s_adc1_handle;
    }
    
    void releaseAdc1Handle() {
        if (s_adc1_handle != nullptr) {
            adc_oneshot_del_unit(s_adc1_handle);
            s_adc1_handle = nullptr;
        }
    }
    
    void lock() {
        if (s_mutex != nullptr) {
            xSemaphoreTake(s_mutex, portMAX_DELAY);
        }
    }
    
    void unlock() {
        if (s_mutex != nullptr) {
            xSemaphoreGive(s_mutex);
        }
    }
}

