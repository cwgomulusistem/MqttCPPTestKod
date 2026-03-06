/**
 * @file MqttService.h
 * @brief WiFi + Web bootstrap + MQTT queue servis akisi
 */

#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <AsyncMqttClient.h>

#include "../config/Config.h"
#include "../system/OsWrappers.h"
#include "MqttTypes.h"
#include "OtaService.h"
#include "WebService.h"

class MqttService {
private:
  Task _task;
  Queue _inboundQueue;
  AsyncMqttClient _client;
  OtaService _otaService;

  WifiConfig _wifiConfig;
  MqttServiceConfig _mqttConfig;
  RuntimeValues _runtime;
  WebService *_webService;

  volatile bool _running;
  bool _initialized;
  volatile bool _mqttConnected;
  bool _wifiConnected;
  bool _bootstrapReady;

  uint32_t _lastWifiConnectAttemptMs;
  uint32_t _lastMqttConnectAttemptMs;
  uint32_t _lastWifiDisconnectMs;
  uint32_t _mqttConnectAllowedAfterMs;
  uint32_t _nextMqttConnectAttemptMs;
  uint8_t _mqttRetryStepIndex;
  bool _mqttConnectInProgress;
  uint32_t _nextBootstrapAttemptMs;
  uint8_t _bootstrapBackoffIndex;

  MqttConnectionInfo _connectionInfo;
  MqttTopicSubscription _subscriptions[MQTT_MAX_SUBSCRIPTIONS];
  char _willTopic[128];
  char _willPayload[64];

  char _fragmentTopic[MQTT_MAX_TOPIC_LENGTH];
  uint8_t _fragmentPayload[MQTT_MAX_PAYLOAD_LENGTH];
  size_t _fragmentReceived;
  size_t _fragmentTotal;
  bool _fragmentInProgress;

  static void taskLoop(void *param);
  void processLoop(uint32_t nowMs);
  void handleWiFi(uint32_t nowMs);
  bool ensureBootstrap(uint32_t nowMs);
  void ensureMqttConnected(uint32_t nowMs);
  void processInboundQueue(uint8_t maxMessages = 8);

  void resetBootstrapState();
  bool applyFallbackConnectionInfo();
  void configureMqttClientFromConnectionInfo();
  void subscribeAllTopics();

  void onMqttConnect(bool sessionPresent);
  void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
  void onMqttMessage(char *topic, char *payload,
                     AsyncMqttClientMessageProperties properties, size_t len,
                     size_t index, size_t total);

public:
  MqttService();
  ~MqttService();

  MqttService(const MqttService &) = delete;
  MqttService &operator=(const MqttService &) = delete;

  bool init(const WifiConfig *wifiConfig, const MqttServiceConfig *mqttConfig,
            WebService *webService, const RuntimeValues *runtime);
  bool start();
  void stop();

  bool isRunning() const;
  bool isConnected() const;

  bool addTopicSubscription(const char *topic, uint8_t qos,
                            MqttTopicMessageHandler handler,
                            void *userData = nullptr);
  bool publish(const char *topic, const uint8_t *payload, size_t payloadLen,
               uint8_t qos = 0, bool retain = false);
  bool publishText(const char *topic, const char *payload, uint8_t qos = 0,
                   bool retain = false);

  UBaseType_t getStackHighWaterMark() const;
};

#endif // MQTT_SERVICE_H
