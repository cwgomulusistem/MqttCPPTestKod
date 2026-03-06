/**
 * @file MqttTypes.h
 * @brief MQTT servis tipleri
 */

#ifndef MQTT_TYPES_H
#define MQTT_TYPES_H

#include <stddef.h>
#include <stdint.h>

static constexpr uint8_t MQTT_MAX_SUBSCRIPTIONS = 16;
static constexpr size_t MQTT_MAX_TOPIC_LENGTH = 128;
static constexpr size_t MQTT_MAX_PAYLOAD_LENGTH = 256;

typedef void (*MqttTopicMessageHandler)(const char *topic, const uint8_t *payload,
                                        size_t len, void *userData);

typedef struct {
  bool valid;
  bool use_tls;
  char host[64];
  uint16_t port;
  char client_id[48];
  char username[64];
  char password[64];
} MqttConnectionInfo;

typedef struct {
  bool used;
  char topic[MQTT_MAX_TOPIC_LENGTH];
  uint8_t qos;
  MqttTopicMessageHandler handler;
  void *user_data;
} MqttTopicSubscription;

typedef struct {
  char topic[MQTT_MAX_TOPIC_LENGTH];
  uint8_t payload[MQTT_MAX_PAYLOAD_LENGTH];
  uint16_t payload_len;
  uint8_t qos;
  bool retain;
} MqttInboundMessage;

#endif // MQTT_TYPES_H
