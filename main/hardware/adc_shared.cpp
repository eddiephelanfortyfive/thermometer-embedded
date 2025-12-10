#include <main/hardware/adc_shared.hpp>
#include <esp_adc/adc_oneshot.h>

namespace AdcShared {
    static adc_oneshot_unit_handle_t s_adc1_handle = nullptr;
    
    adc_oneshot_unit_handle_t getAdc1Handle() {
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
}

