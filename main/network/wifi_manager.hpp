#ifndef WIFI_MANAGER_HPP
#define WIFI_MANAGER_HPP

#include <cstdint>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>

class WiFiManager {
public:
    WiFiManager();

    bool init();
    bool connect();
    void disconnect();
    bool reconnect();

    bool isConnected() const { return connected; }
    bool hasIp() const { return got_ip; }

private:
    static void wifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void ipEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    bool initialized;
    bool connected;
    bool got_ip;
    int retry_count;

    esp_event_handler_instance_t wifi_any_id_instance;
    esp_event_handler_instance_t ip_got_ip_instance;
};

#endif // WIFI_MANAGER_HPP


