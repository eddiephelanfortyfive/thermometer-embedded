// Simple fixed-size container for MQTT publish requests sent
// from the command task to the cloud communication task.
#ifndef CLOUD_PUBLISH_REQUEST_HPP
#define CLOUD_PUBLISH_REQUEST_HPP

#include <cstdint>

struct CloudPublishRequest {
    char topic[96];
    char payload[320];
};

#endif // CLOUD_PUBLISH_REQUEST_HPP

