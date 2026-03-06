/**
 * @file MqttService.cpp
 * @brief WiFi + Web bootstrap + MQTT queue servis akisi
 */

#include "MqttService.h"
#include "../config/Logger.h"

#include <WiFi.h>
#include <string.h>

namespace {
const char *disconnectReasonToString(AsyncMqttClientDisconnectReason reason) {
  switch (reason) {
  case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
    return "TCP_DISCONNECTED";
  case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
    return "MQTT_UNACCEPTABLE_PROTOCOL_VERSION";
  case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
    return "MQTT_IDENTIFIER_REJECTED";
  case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
    return "MQTT_SERVER_UNAVAILABLE";
  case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
    return "MQTT_MALFORMED_CREDENTIALS";
  case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
    return "MQTT_NOT_AUTHORIZED";
  case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE:
    return "NOT_ENOUGH_SPACE";
  case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
    return "TLS_BAD_FINGERPRINT";
  default:
    return "UNKNOWN";
  }
}

bool topicMatches(const char *filter, const char *topic) {
  if (!filter || !topic) {
    return false;
  }

  const char *f = filter;
  const char *t = topic;

  while (*f != '\0' && *t != '\0') {
    if (*f == '#') {
      return (*(f + 1) == '\0');
    }

    if (*f == '+') {
      while (*t != '\0' && *t != '/') {
        ++t;
      }
      ++f;

      if (*f == '\0' && *t == '\0') {
        return true;
      }
      if (*f == '/' && *t == '/') {
        ++f;
        ++t;
        continue;
      }
      return false;
    }

    if (*f != *t) {
      return false;
    }

    ++f;
    ++t;
  }

  if (*f == '\0' && *t == '\0') {
    return true;
  }

  if (*t == '\0' && *f == '/' && *(f + 1) == '#' && *(f + 2) == '\0') {
    return true;
  }

  if (*t == '\0' && *f == '#' && *(f + 1) == '\0') {
    return true;
  }

  return false;
}
} // namespace

MqttService::MqttService()
    : _webService(nullptr), _running(false), _initialized(false),
      _mqttConnected(false), _wifiConnected(false), _bootstrapReady(false),
      _lastWifiConnectAttemptMs(0), _lastMqttConnectAttemptMs(0),
      _lastWifiDisconnectMs(0), _mqttConnectAllowedAfterMs(0),
      _nextMqttConnectAttemptMs(0), _mqttRetryStepIndex(0),
      _mqttConnectInProgress(false),
      _nextBootstrapAttemptMs(0), _bootstrapBackoffIndex(0),
      _fragmentReceived(0), _fragmentTotal(0),
      _fragmentInProgress(false) {
  memset(&_wifiConfig, 0, sizeof(_wifiConfig));
  memset(&_mqttConfig, 0, sizeof(_mqttConfig));
  memset(&_runtime, 0, sizeof(_runtime));
  memset(&_connectionInfo, 0, sizeof(_connectionInfo));
  memset(_subscriptions, 0, sizeof(_subscriptions));
  memset(_willTopic, 0, sizeof(_willTopic));
  memset(_willPayload, 0, sizeof(_willPayload));
  memset(_fragmentTopic, 0, sizeof(_fragmentTopic));
  memset(_fragmentPayload, 0, sizeof(_fragmentPayload));
}

MqttService::~MqttService() { stop(); }

bool MqttService::init(const WifiConfig *wifiConfig,
                       const MqttServiceConfig *mqttConfig,
                       WebService *webService, const RuntimeValues *runtime) {
  if (!wifiConfig || !mqttConfig || !runtime) {
    LOG_E("MQTT: NULL config/runtime");
    return false;
  }

  memcpy(&_wifiConfig, wifiConfig, sizeof(_wifiConfig));
  memcpy(&_mqttConfig, mqttConfig, sizeof(_mqttConfig));
  memcpy(&_runtime, runtime, sizeof(_runtime));
  _webService = webService;

  if (!_inboundQueue.create(_mqttConfig.queue_size, sizeof(MqttInboundMessage))) {
    LOG_E("MQTT: inbound queue create failed");
    return false;
  }

  _client.setKeepAlive(_mqttConfig.keep_alive_sec);
  _client.setCleanSession(_mqttConfig.clean_session);
  _client.setMaxTopicLength(MQTT_MAX_TOPIC_LENGTH - 1);

  _client.onConnect(
      [this](bool sessionPresent) { this->onMqttConnect(sessionPresent); });
  _client.onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
    this->onMqttDisconnect(reason);
  });
  _client.onMessage([this](char *topic, char *payload,
                           AsyncMqttClientMessageProperties properties,
                           size_t len, size_t index, size_t total) {
    this->onMqttMessage(topic, payload, properties, len, index, total);
  });

  WiFi.mode(WIFI_STA);
  if (_wifiConfig.enabled && _wifiConfig.ssid[0] != '\0') {
    WiFi.begin(_wifiConfig.ssid, _wifiConfig.password);
    _lastWifiConnectAttemptMs = millis();
    LOG_I("MQTT: WiFi begin (%s)", _wifiConfig.ssid);
  } else {
    LOG_W("MQTT: WiFi credentials empty, connect attempts skipped");
  }

  _initialized = true;
  LOG_I("MQTT: Service initialized (loop=%u ms, reconnect=%lu ms)",
        static_cast<unsigned>(_mqttConfig.loop_delay_ms),
        static_cast<unsigned long>(_mqttConfig.reconnect_delay_ms));
  return true;
}

bool MqttService::start() {
  if (!_initialized) {
    LOG_E("MQTT: not initialized");
    return false;
  }

  if (_running) {
    LOG_W("MQTT: already running");
    return false;
  }

  _running = true;
  bool started = _task.start("MqttTask", taskLoop, this, _mqttConfig.task_stack,
                             _mqttConfig.task_priority, 1);
  if (!started) {
    _running = false;
    LOG_E("MQTT: task start failed");
    return false;
  }

  LOG_I("MQTT: task started (stack=%u, priority=%u)",
        static_cast<unsigned>(_mqttConfig.task_stack),
        static_cast<unsigned>(_mqttConfig.task_priority));
  return true;
}

void MqttService::stop() {
  if (!_running) {
    return;
  }

  _running = false;
  if (_mqttConnected) {
    _client.disconnect(true);
    _mqttConnected = false;
  }
  _task.stop();
  LOG_I("MQTT: service stopped");
}

bool MqttService::isRunning() const { return _running; }

bool MqttService::isConnected() const { return _mqttConnected; }

void MqttService::taskLoop(void *param) {
  MqttService *self = static_cast<MqttService *>(param);
  while (self->_running) {
    self->processLoop(millis());
    uint16_t delayMs = self->_mqttConfig.loop_delay_ms;
    if (delayMs < 10) {
      delayMs = 10;
    }
    vTaskDelay(pdMS_TO_TICKS(delayMs));
  }

  vTaskDelete(nullptr);
}

void MqttService::processLoop(uint32_t nowMs) {
  handleWiFi(nowMs);

  if (_wifiConnected) {
    if (ensureBootstrap(nowMs)) {
      ensureMqttConnected(nowMs);
    }
  }

  processInboundQueue();
}

void MqttService::handleWiFi(uint32_t nowMs) {
  bool nowConnected = (WiFi.status() == WL_CONNECTED);
  if (nowConnected) {
    if (!_wifiConnected) {
      _wifiConnected = true;
      LOG_I("MQTT: WiFi connected ip=%s", WiFi.localIP().toString().c_str());
      _nextBootstrapAttemptMs = nowMs;

      if (_lastWifiDisconnectMs != 0) {
        _mqttConnectAllowedAfterMs =
            _lastWifiDisconnectMs + _mqttConfig.reconnect_delay_ms;
      } else {
        _mqttConnectAllowedAfterMs = nowMs;
      }

      _nextMqttConnectAttemptMs = _mqttConnectAllowedAfterMs;
      _mqttRetryStepIndex = 0;
      _mqttConnectInProgress = false;

      if (static_cast<int32_t>(nowMs - _mqttConnectAllowedAfterMs) < 0) {
        uint32_t remainingMs = _mqttConnectAllowedAfterMs - nowMs;
        LOG_I("MQTT: reconnect gate active, first connect in %lu ms",
              static_cast<unsigned long>(remainingMs));
      } else {
        LOG_I("MQTT: reconnect gate passed, connect can start immediately");
      }
    }
    return;
  }

  if (_wifiConnected) {
    LOG_W("MQTT: WiFi disconnected");
    _wifiConnected = false;
    _lastWifiDisconnectMs = nowMs;
    _mqttConnectAllowedAfterMs = nowMs + _mqttConfig.reconnect_delay_ms;
    _nextMqttConnectAttemptMs = _mqttConnectAllowedAfterMs;
    _mqttRetryStepIndex = 0;
    _mqttConnectInProgress = false;
    _lastMqttConnectAttemptMs = 0;

    if (_mqttConnected) {
      _client.disconnect(true);
      _mqttConnected = false;
    }
    resetBootstrapState();
  }

  if (!_wifiConfig.enabled || _wifiConfig.ssid[0] == '\0') {
    return;
  }

  if (_lastWifiConnectAttemptMs != 0 &&
      (nowMs - _lastWifiConnectAttemptMs) < _wifiConfig.reconnect_interval_ms) {
    return;
  }

  _lastWifiConnectAttemptMs = nowMs;
  WiFi.disconnect();
  WiFi.begin(_wifiConfig.ssid, _wifiConfig.password);
  LOG_I("MQTT: WiFi reconnect attempt");
}

bool MqttService::ensureBootstrap(uint32_t nowMs) {
  static constexpr uint32_t kBackoffStepsMs[] = {5000U, 10000U, 30000U, 60000U};
  static constexpr uint8_t kBackoffStepCount =
      sizeof(kBackoffStepsMs) / sizeof(kBackoffStepsMs[0]);

  if (_bootstrapReady) {
    return true;
  }

  if (_nextBootstrapAttemptMs != 0 &&
      static_cast<int32_t>(nowMs - _nextBootstrapAttemptMs) < 0) {
    return false;
  }

  auto scheduleBackoff = [&](const char *reasonLabel) {
    uint8_t stepIndex = _bootstrapBackoffIndex;
    if (stepIndex >= kBackoffStepCount) {
      stepIndex = kBackoffStepCount - 1;
    }

    uint32_t waitMs = kBackoffStepsMs[stepIndex];
    if (_bootstrapBackoffIndex < (kBackoffStepCount - 1)) {
      _bootstrapBackoffIndex++;
    }
    _nextBootstrapAttemptMs = nowMs + waitMs;
    LOG_W("MQTT: bootstrap failed (%s), retry in %lu ms", reasonLabel,
          static_cast<unsigned long>(waitMs));
  };

  bool hasWebBootstrap = (_webService && _webService->isInitialized());
  if (hasWebBootstrap) {
    WebBootstrapResult result = {};
    bool bootstrapOk = _webService->runBootstrap(&result);
    if (!bootstrapOk) {
      scheduleBackoff("web");
      return false;
    }

    if (result.update_available) {
      LOG_W("MQTT: update available, starting OTA");
      if (!_otaService.runUpdateFromUrl(result.update_url)) {
        scheduleBackoff("ota");
      }
      return false;
    }

    if (!result.mqtt.valid) {
      scheduleBackoff("mqtt_credentials");
      return false;
    }

    _connectionInfo = result.mqtt;
    _bootstrapReady = true;
    _bootstrapBackoffIndex = 0;
    _nextBootstrapAttemptMs = 0;
    configureMqttClientFromConnectionInfo();
    LOG_I("MQTT: bootstrap source=web");
    return true;
  }

  if (applyFallbackConnectionInfo()) {
    configureMqttClientFromConnectionInfo();
    _bootstrapReady = true;
    _bootstrapBackoffIndex = 0;
    _nextBootstrapAttemptMs = 0;
    LOG_W("MQTT: bootstrap source=fallback");
    return true;
  }

  scheduleBackoff("fallback");
  return false;
}

void MqttService::ensureMqttConnected(uint32_t nowMs) {
  if (_mqttConnected || !_bootstrapReady || !_wifiConnected) {
    return;
  }

  if (_mqttConnectInProgress) {
    return;
  }

  if (_mqttConnectAllowedAfterMs != 0 &&
      static_cast<int32_t>(nowMs - _mqttConnectAllowedAfterMs) < 0) {
    return;
  }

  if (_nextMqttConnectAttemptMs != 0 &&
      static_cast<int32_t>(nowMs - _nextMqttConnectAttemptMs) < 0) {
    return;
  }

  _lastMqttConnectAttemptMs = nowMs;
  _mqttConnectInProgress = true;

  uint8_t attemptNo = static_cast<uint8_t>(_mqttRetryStepIndex + 1U);
  if (attemptNo > 3U) {
    attemptNo = 3U;
  }
  LOG_I("MQTT: connect attempt %u/3 host=%s port=%u",
        static_cast<unsigned>(attemptNo), _connectionInfo.host,
        static_cast<unsigned>(_connectionInfo.port));
  _client.connect();
}

void MqttService::processInboundQueue(uint8_t maxMessages) {
  MqttInboundMessage msg = {};
  uint8_t processed = 0;

  while (processed < maxMessages && _inboundQueue.receive(&msg, 0)) {
    bool matched = false;

    for (uint8_t i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
      if (!_subscriptions[i].used || !_subscriptions[i].handler) {
        continue;
      }

      if (!topicMatches(_subscriptions[i].topic, msg.topic)) {
        continue;
      }

      _subscriptions[i].handler(msg.topic, msg.payload, msg.payload_len,
                                _subscriptions[i].user_data);
      matched = true;
    }

    if (!matched) {
      LOG_D("MQTT: no handler topic=%s", msg.topic);
    }

    processed++;
  }
}

void MqttService::resetBootstrapState() {
  _bootstrapReady = false;
  _nextBootstrapAttemptMs = 0;
  _bootstrapBackoffIndex = 0;
  memset(&_connectionInfo, 0, sizeof(_connectionInfo));
  _fragmentInProgress = false;
  _fragmentReceived = 0;
  _fragmentTotal = 0;
}

bool MqttService::applyFallbackConnectionInfo() {
  if (_mqttConfig.fallback_host[0] == '\0') {
    return false;
  }

  memset(&_connectionInfo, 0, sizeof(_connectionInfo));
  strncpy(_connectionInfo.host, _mqttConfig.fallback_host,
          sizeof(_connectionInfo.host) - 1);
  _connectionInfo.port =
      (_mqttConfig.fallback_port == 0) ? 1883 : _mqttConfig.fallback_port;

  if (_mqttConfig.fallback_client_id[0] != '\0') {
    strncpy(_connectionInfo.client_id, _mqttConfig.fallback_client_id,
            sizeof(_connectionInfo.client_id) - 1);
  } else {
    strncpy(_connectionInfo.client_id, _runtime.device.device_id,
            sizeof(_connectionInfo.client_id) - 1);
  }

  strncpy(_connectionInfo.username, _mqttConfig.fallback_username,
          sizeof(_connectionInfo.username) - 1);
  strncpy(_connectionInfo.password, _mqttConfig.fallback_password,
          sizeof(_connectionInfo.password) - 1);

  _connectionInfo.use_tls = false;
  _connectionInfo.valid = true;
  return true;
}

void MqttService::configureMqttClientFromConnectionInfo() {
  if (!_connectionInfo.valid || _connectionInfo.host[0] == '\0') {
    return;
  }

  _client.setKeepAlive(_mqttConfig.keep_alive_sec);
  _client.setCleanSession(_mqttConfig.clean_session);
  _client.setServer(_connectionInfo.host, _connectionInfo.port);

  if (_connectionInfo.client_id[0] != '\0') {
    _client.setClientId(_connectionInfo.client_id);
  } else {
    _client.setClientId(_runtime.device.device_id);
  }

  if (_connectionInfo.username[0] != '\0') {
    _client.setCredentials(_connectionInfo.username, _connectionInfo.password);
  } else {
    _client.setCredentials(nullptr, nullptr);
  }

  memset(_willTopic, 0, sizeof(_willTopic));
  if (_mqttConfig.lwt_topic[0] != '\0') {
    strncpy(_willTopic, _mqttConfig.lwt_topic, sizeof(_willTopic) - 1);
  } else {
    snprintf(_willTopic, sizeof(_willTopic), "funtoria/device/%s/status",
             _runtime.device.device_id);
  }

  memset(_willPayload, 0, sizeof(_willPayload));
  if (_mqttConfig.lwt_payload[0] != '\0') {
    strncpy(_willPayload, _mqttConfig.lwt_payload, sizeof(_willPayload) - 1);
  } else {
    strncpy(_willPayload, "offline", sizeof(_willPayload) - 1);
  }

  uint8_t lwtQos = (_mqttConfig.lwt_qos > 2) ? 0 : _mqttConfig.lwt_qos;
  _client.setWill(_willTopic, lwtQos, _mqttConfig.lwt_retain, _willPayload);
  LOG_I("MQTT: LWT topic=%s payload=%s qos=%u retain=%s", _willTopic,
        _willPayload, static_cast<unsigned>(lwtQos),
        _mqttConfig.lwt_retain ? "true" : "false");

#if ASYNC_TCP_SSL_ENABLED
  _client.setSecure(_connectionInfo.use_tls);
#endif
}

void MqttService::subscribeAllTopics() {
  for (uint8_t i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
    if (!_subscriptions[i].used) {
      continue;
    }

    uint16_t packetId = _client.subscribe(_subscriptions[i].topic,
                                          _subscriptions[i].qos);
    LOG_I("MQTT: subscribe topic=%s qos=%u pid=%u", _subscriptions[i].topic,
          static_cast<unsigned>(_subscriptions[i].qos),
          static_cast<unsigned>(packetId));
  }
}

void MqttService::onMqttConnect(bool sessionPresent) {
  _mqttConnected = true;
  _mqttConnectInProgress = false;
  _mqttRetryStepIndex = 0;
  _nextMqttConnectAttemptMs = 0;
  _mqttConnectAllowedAfterMs = millis();
  LOG_I("MQTT: connected session=%s", sessionPresent ? "present" : "new");
  subscribeAllTopics();
}

void MqttService::onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  _mqttConnected = false;
  _mqttConnectInProgress = false;
  LOG_W("MQTT: disconnected reason=%s", disconnectReasonToString(reason));

  if (!_wifiConnected || !_bootstrapReady) {
    return;
  }

  static constexpr uint32_t kMqttRetryStepsMs[] = {5000U, 10000U, 30000U};
  static constexpr uint8_t kMqttRetryStepCount =
      sizeof(kMqttRetryStepsMs) / sizeof(kMqttRetryStepsMs[0]);

  uint8_t stepIndex = _mqttRetryStepIndex;
  if (stepIndex >= kMqttRetryStepCount) {
    stepIndex = kMqttRetryStepCount - 1;
  }

  uint32_t waitMs = kMqttRetryStepsMs[stepIndex];
  if (_mqttRetryStepIndex < (kMqttRetryStepCount - 1)) {
    _mqttRetryStepIndex++;
  } else {
    _mqttRetryStepIndex = 0;
  }

  _nextMqttConnectAttemptMs = millis() + waitMs;
  LOG_W("MQTT: reconnect step wait %lu ms (next step=%u/3)",
        static_cast<unsigned long>(waitMs),
        static_cast<unsigned>(_mqttRetryStepIndex + 1U));
}

void MqttService::onMqttMessage(char *topic, char *payload,
                                AsyncMqttClientMessageProperties properties,
                                size_t len, size_t index, size_t total) {
  if (!topic || !payload || total == 0) {
    return;
  }

  if (total > MQTT_MAX_PAYLOAD_LENGTH) {
    LOG_W("MQTT: payload too large total=%u", static_cast<unsigned>(total));
    _fragmentInProgress = false;
    return;
  }

  if (index == 0) {
    memset(_fragmentTopic, 0, sizeof(_fragmentTopic));
    strncpy(_fragmentTopic, topic, sizeof(_fragmentTopic) - 1);
    _fragmentReceived = 0;
    _fragmentTotal = total;
    _fragmentInProgress = true;
  }

  if (!_fragmentInProgress || strcmp(_fragmentTopic, topic) != 0 ||
      _fragmentTotal != total) {
    LOG_W("MQTT: fragment state mismatch topic=%s", topic);
    _fragmentInProgress = false;
    return;
  }

  if ((index + len) > MQTT_MAX_PAYLOAD_LENGTH || (index + len) > total) {
    LOG_W("MQTT: fragment bounds invalid");
    _fragmentInProgress = false;
    return;
  }

  memcpy(_fragmentPayload + index, payload, len);
  size_t currentEnd = index + len;
  if (currentEnd > _fragmentReceived) {
    _fragmentReceived = currentEnd;
  }

  if (_fragmentReceived < _fragmentTotal) {
    return;
  }

  MqttInboundMessage msg = {};
  strncpy(msg.topic, _fragmentTopic, sizeof(msg.topic) - 1);
  memcpy(msg.payload, _fragmentPayload, _fragmentTotal);
  msg.payload_len = static_cast<uint16_t>(_fragmentTotal);
  msg.qos = properties.qos;
  msg.retain = properties.retain;

  if (!_inboundQueue.send(&msg, 0)) {
    LOG_W("MQTT: inbound queue full, message dropped topic=%s", msg.topic);
  }

  _fragmentInProgress = false;
}

bool MqttService::addTopicSubscription(const char *topic, uint8_t qos,
                                       MqttTopicMessageHandler handler,
                                       void *userData) {
  if (!topic || topic[0] == '\0') {
    return false;
  }

  uint8_t normalizedQos = (qos > 2) ? 0 : qos;

  for (uint8_t i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
    if (_subscriptions[i].used && strcmp(_subscriptions[i].topic, topic) == 0) {
      _subscriptions[i].qos = normalizedQos;
      _subscriptions[i].handler = handler;
      _subscriptions[i].user_data = userData;
      if (_mqttConnected) {
        _client.subscribe(_subscriptions[i].topic, _subscriptions[i].qos);
      }
      return true;
    }
  }

  for (uint8_t i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
    if (_subscriptions[i].used) {
      continue;
    }

    memset(&_subscriptions[i], 0, sizeof(_subscriptions[i]));
    _subscriptions[i].used = true;
    _subscriptions[i].qos = normalizedQos;
    _subscriptions[i].handler = handler;
    _subscriptions[i].user_data = userData;
    strncpy(_subscriptions[i].topic, topic, sizeof(_subscriptions[i].topic) - 1);

    if (_mqttConnected) {
      _client.subscribe(_subscriptions[i].topic, _subscriptions[i].qos);
    }
    return true;
  }

  LOG_E("MQTT: subscription table full");
  return false;
}

bool MqttService::publish(const char *topic, const uint8_t *payload,
                          size_t payloadLen, uint8_t qos, bool retain) {
  if (!topic || topic[0] == '\0') {
    return false;
  }

  if (!_mqttConnected) {
    return false;
  }

  const char *rawPayload =
      (payload && payloadLen > 0) ? reinterpret_cast<const char *>(payload)
                                  : nullptr;
  uint16_t packetId =
      _client.publish(topic, (qos > 2) ? 0 : qos, retain, rawPayload, payloadLen);
  return packetId != 0;
}

bool MqttService::publishText(const char *topic, const char *payload, uint8_t qos,
                              bool retain) {
  if (!payload) {
    payload = "";
  }
  return publish(topic, reinterpret_cast<const uint8_t *>(payload),
                 strlen(payload), qos, retain);
}

UBaseType_t MqttService::getStackHighWaterMark() const {
  return _task.getStackHighWaterMark();
}
