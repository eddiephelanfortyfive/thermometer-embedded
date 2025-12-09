idf_component_register(SRCS "main.cpp"
                               "utils/logger.cpp"
                               "network/wifi_manager.cpp"
                    INCLUDE_DIRS "."
                                  ".."
                                  "utils"
                                  "network"
                                  "config"
                    REQUIRES esp_wifi esp_event nvs_flash esp_netif)
