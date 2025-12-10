#include <main/network/wifi_manager.hpp>
#include <main/utils/logger.hpp>
#include <main/config/config.hpp>

#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_err.h>
#include <cstdio>

static const char* TAG = "WiFiManager";

WiFiManager::WiFiManager()
    : initialized(false),
      connected(false),
      got_ip(false),
      retry_count(0),
      wifi_any_id_instance(nullptr),
      ip_got_ip_instance(nullptr) {}

bool WiFiManager::init() {
    if (initialized) {
        return true;
    }

    // NVS init (required by WiFi)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "NVS init failed: %d", static_cast<int>(err));
        return false;
    }

    // Netif and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {
        LOG_ERROR(TAG, "Event loop create failed: %d", static_cast<int>(loop_err));
        return false;
    }
    (void)loop_err;

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &WiFiManager::wifiEventHandler,
        this,
        &wifi_any_id_instance
    ));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &WiFiManager::ipEventHandler,
        this,
        &ip_got_ip_instance
    ));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Configure STA credentials
    wifi_config_t wifi_config = {};
    // Safe copy SSID/PASS (IDF expects zero-terminated)
    snprintf(reinterpret_cast<char*>(wifi_config.sta.ssid),
                  sizeof(wifi_config.sta.ssid), "%s", Config::Wifi::ssid);
    snprintf(reinterpret_cast<char*>(wifi_config.sta.password),
                  sizeof(wifi_config.sta.password), "%s", Config::Wifi::password);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    initialized = true;

    if (Config::Wifi::auto_connect_on_start) {
        return connect();
    }
    return true;
}

bool WiFiManager::connect() {
    if (!initialized) {
        if (!init()) {
            return false;
        }
    }
    retry_count = 0;
    got_ip = false;
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "esp_wifi_connect failed: %d", static_cast<int>(err));
        return false;
    }
    LOG_INFO(TAG, "Connecting to SSID: %s", Config::Wifi::ssid);
    return true;
}

void WiFiManager::disconnect() {
    (void)esp_wifi_disconnect();
    connected = false;
    got_ip = false;
}

bool WiFiManager::reconnect() {
    disconnect();
    return connect();
}

void WiFiManager::wifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    WiFiManager* self = static_cast<WiFiManager*>(arg);
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            LOG_INFO(TAG, "WIFI_EVENT_STA_START");
            if (Config::Wifi::auto_connect_on_start) {
                (void)esp_wifi_connect();
            }
            break;
        case WIFI_EVENT_STA_CONNECTED:
            LOG_INFO(TAG, "WIFI_EVENT_STA_CONNECTED");
            self->connected = true;
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            LOG_WARN(TAG, "WIFI_EVENT_STA_DISCONNECTED");
            self->connected = false;
            self->got_ip = false;
            if (self->retry_count < Config::Wifi::max_retry_count) {
                self->retry_count++;
                LOG_INFO(TAG, "Retrying WiFi (%d/%d)", self->retry_count, Config::Wifi::max_retry_count);
                (void)esp_wifi_connect();
            } else {
                LOG_ERROR(TAG, "WiFi connect failed after %d retries", Config::Wifi::max_retry_count);
            }
            break;
        case WIFI_EVENT_STA_STOP:
            LOG_INFO(TAG, "WIFI_EVENT_STA_STOP");
            self->connected = false;
            self->got_ip = false;
            break;
        default:
            break;
    }
}

void WiFiManager::ipEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    WiFiManager* self = static_cast<WiFiManager*>(arg);
    if (event_id == IP_EVENT_STA_GOT_IP) {
        self->got_ip = true;
        self->connected = true;
        LOG_INFO(TAG, "Got IP address");
        self->retry_count = 0;
    }
}


