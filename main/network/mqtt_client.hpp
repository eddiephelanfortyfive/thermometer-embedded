#ifndef MQTT_CLIENT_HPP
#define MQTT_CLIENT_HPP

#include <cstdint>
#include <mqtt_client.h>

class MqttClient {
public:
    using MessageHandler = void (*)(const char* topic, const uint8_t* payload, int length);

    // Construct using values from Config::Mqtt and Config::Device
    MqttClient();
    // Optional explicit constructor
    MqttClient(const char* host, int port, const char* client_id);

    bool init();
    bool connect();
    void disconnect();
    bool isConnected() const;

    int publish(const char* topic, const char* payload, int qos = 1, bool retain = false);
    int subscribe(const char* topic, int qos = 1);
    int unsubscribe(const char* topic);

    void setMessageHandler(MessageHandler handler);

private:
    static void mqttEventHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    void handleEvent(esp_mqtt_event_handle_t event);

    esp_mqtt_client_handle_t client;
    const char* host;
    int port;
    const char* client_id;
    bool connected;
    MessageHandler on_message;
};

#endif // MQTT_CLIENT_HPP


