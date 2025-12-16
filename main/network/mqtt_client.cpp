#include <main/network/mqtt_client.hpp>
#include <main/utils/logger.hpp>
#include <main/config/config.hpp>
#include <cstring>
#include <cstdio>

static const char* TAG_MQTT = "MqttClient";

MqttClient::MqttClient()
    : client(nullptr),
      host(Config::Mqtt::host),
      port(Config::Mqtt::port),
      client_id(Config::Device::id),
      connected(false),
      on_message(nullptr) {}

MqttClient::MqttClient(const char* host, int port, const char* client_id)
    : client(nullptr),
      host(host),
      port(port),
      client_id(client_id),
      connected(false),
      on_message(nullptr) {}

bool MqttClient::init() {
    // Nothing heavy to do here; actual client is created on connect()
    return true;
}

bool MqttClient::connect() {
    if (client != nullptr) {
        return true;
    }

    esp_mqtt_client_config_t cfg = {};
    // esp-mqtt expects a URI with scheme, e.g. "mqtt://host:1883"
    static char uri[128];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", host, port);
    cfg.broker.address.uri = uri;
    cfg.credentials.client_id = client_id;
    cfg.session.keepalive = Config::Mqtt::keepalive_seconds;
    cfg.session.disable_clean_session = !Config::Mqtt::clean_session;

    LOG_INFO(TAG_MQTT, "Connecting to %s as %s", uri, client_id);

    // LWT: retained "offline" on disconnect; publish "online" on connect
    static char lwt_topic[96];
    if (Config::Mqtt::lwt_enable) {
        snprintf(lwt_topic, sizeof(lwt_topic), Config::Mqtt::Topics::STATUS, client_id);
        cfg.session.last_will.topic = lwt_topic;
        cfg.session.last_will.msg = "offline";
        cfg.session.last_will.qos = Config::Mqtt::default_qos;
        cfg.session.last_will.retain = true;
    }

    client = esp_mqtt_client_init(&cfg);
    if (!client) {
        LOG_ERROR(TAG_MQTT, "%s", "esp_mqtt_client_init failed");
        return false;
    }
    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, &MqttClient::mqttEventHandler, this);
    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        LOG_ERROR(TAG_MQTT, "esp_mqtt_client_start failed: %d", static_cast<int>(err));
        return false;
    }
    return true;
}

void MqttClient::disconnect() {
    if (client) {
        (void)esp_mqtt_client_stop(client);
        (void)esp_mqtt_client_destroy(client);
        client = nullptr;
        connected = false;
    }
}

bool MqttClient::isConnected() const {
    return connected;
}

int MqttClient::publish(const char* topic, const char* payload, int qos, bool retain) {
    if (!client || !connected) {
        LOG_WARN(TAG_MQTT, "Skip publish (not connected) topic=%s", topic);
        return -1;
    }
    int length = static_cast<int>(std::strlen(payload));
    int mid = esp_mqtt_client_publish(client, topic, payload, length, qos, retain ? 1 : 0);
    if (mid >= 0) {
        LOG_INFO(TAG_MQTT, "Publish topic=%s len=%d qos=%d retain=%d mid=%d", topic, length, qos, retain ? 1 : 0, mid);
    } else {
        LOG_ERROR(TAG_MQTT, "Publish failed topic=%s rc=%d", topic, mid);
    }
    return mid;
}

int MqttClient::subscribe(const char* topic, int qos) {
    if (!client || !connected) {
        LOG_WARN(TAG_MQTT, "Skip subscribe (not connected) topic=%s", topic);
        return -1;
    }
    int mid = esp_mqtt_client_subscribe(client, topic, qos);
    if (mid >= 0) {
        LOG_INFO(TAG_MQTT, "Subscribe topic=%s qos=%d mid=%d", topic, qos, mid);
    } else {
        LOG_ERROR(TAG_MQTT, "Subscribe failed topic=%s rc=%d", topic, mid);
    }
    return mid;
}

int MqttClient::unsubscribe(const char* topic) {
    if (!client || !connected) {
        LOG_WARN(TAG_MQTT, "Skip unsubscribe (not connected) topic=%s", topic);
        return -1;
    }
    int mid = esp_mqtt_client_unsubscribe(client, topic);
    if (mid >= 0) {
        LOG_INFO(TAG_MQTT, "Unsubscribe topic=%s mid=%d", topic, mid);
    } else {
        LOG_ERROR(TAG_MQTT, "Unsubscribe failed topic=%s rc=%d", topic, mid);
    }
    return mid;
}

void MqttClient::setMessageHandler(MessageHandler handler) {
    on_message = handler;
}

void MqttClient::mqttEventHandler(void* handler_args, esp_event_base_t, int32_t event_id, void* event_data) {
    auto* self = static_cast<MqttClient*>(handler_args);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    self->handleEvent(event);
}

void MqttClient::handleEvent(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED: {
            connected = true;
            // Publish retained "online" status
            if (Config::Mqtt::lwt_enable) {
                char topic[96];
                snprintf(topic, sizeof(topic), Config::Mqtt::Topics::STATUS, client_id);
                (void)publish(topic, "online", Config::Mqtt::default_qos, true);
            }
            LOG_INFO(TAG_MQTT, "%s", "MQTT connected");
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            connected = false;
            LOG_WARN(TAG_MQTT, "%s", "MQTT disconnected");
            break;
        case MQTT_EVENT_DATA:
            if (on_message) {
                on_message(event->topic, reinterpret_cast<const uint8_t*>(event->data), event->data_len);
            } else {
                LOG_DEBUG(TAG_MQTT, "RX topic=%.*s len=%d", event->topic_len, event->topic, event->data_len);
            }
            break;
        case MQTT_EVENT_ERROR:
            LOG_ERROR(TAG_MQTT, "%s", "MQTT error");
            break;
        default:
            break;
    }
}


