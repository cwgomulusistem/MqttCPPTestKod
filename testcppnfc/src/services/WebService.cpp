/**
 * @file WebService.cpp
 * @brief Cihaz acilis bootstrap API servisi
 */

#include "WebService.h"
#include "../config/Logger.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <string.h>

WebService::WebService() : _initialized(false) {
  memset(&_config, 0, sizeof(_config));
  memset(&_runtime, 0, sizeof(_runtime));
}

bool WebService::init(const WebApiConfig *config, const RuntimeValues *runtime) {
  if (!config || !runtime) {
    LOG_E("WEB: NULL config/runtime");
    return false;
  }

  memcpy(&_config, config, sizeof(_config));
  memcpy(&_runtime, runtime, sizeof(_runtime));
  _initialized = true;

  LOG_I("WEB: Service initialized (base=%s)", _config.base_url);
  return true;
}

bool WebService::isInitialized() const { return _initialized; }

bool WebService::buildUrl(const char *path, char *out, size_t outLen) const {
  if (!out || outLen == 0 || !_initialized) {
    return false;
  }

  if (_config.base_url[0] == '\0' || !path || path[0] == '\0') {
    return false;
  }

  bool needsSlash = (_config.base_url[strlen(_config.base_url) - 1] != '/') &&
                    (path[0] != '/');
  snprintf(out, outLen, "%s%s%s", _config.base_url, needsSlash ? "/" : "", path);
  out[outLen - 1] = '\0';
  return true;
}

bool WebService::postJson(const char *url, const char *jsonBody, String &response,
                          int &statusCode) const {
  if (!url || url[0] == '\0' || !jsonBody) {
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    LOG_W("WEB: WiFi not connected, POST skipped");
    return false;
  }

  HTTPClient http;
  http.setTimeout(_config.request_timeout_ms);
  if (!http.begin(url)) {
    LOG_E("WEB: HTTP begin failed (%s)", url);
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  statusCode =
      http.POST(reinterpret_cast<uint8_t *>(const_cast<char *>(jsonBody)),
                strlen(jsonBody));

  if (statusCode > 0) {
    response = http.getString();
  } else {
    response = "";
  }

  http.end();

  if (statusCode <= 0) {
    LOG_E("WEB: POST failed code=%d url=%s", statusCode, url);
    return false;
  }

  return true;
}

bool WebService::performVersionCheck(WebBootstrapResult *outResult) {
  if (!outResult) {
    return false;
  }

  outResult->update_available = false;
  outResult->update_url[0] = '\0';

  char url[192] = {0};
  if (!buildUrl(_config.version_check_path, url, sizeof(url))) {
    LOG_E("WEB: version URL build failed");
    return false;
  }

  char body[256] = {0};
  // TODO: Version check body alanlarini backend contract'inize gore duzenleyin.
  snprintf(body, sizeof(body),
           "{\"device_id\":\"%s\",\"mac\":\"%s\",\"fw\":\"2.2.0\"}",
           _runtime.device.device_id, _runtime.device.mac_hex);

  for (uint8_t attempt = 0; attempt <= _config.max_retries; ++attempt) {
    String response;
    int statusCode = -1;

    if (!postJson(url, body, response, statusCode)) {
      continue;
    }

    if (statusCode < 200 || statusCode >= 300) {
      LOG_W("WEB: version check http=%d body=%s", statusCode, response.c_str());
      continue;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err) {
      LOG_W("WEB: version check parse fail, response accepted raw");
      return true;
    }

    bool allowed = doc["allowed"] | true;
    if (!allowed) {
      LOG_E("WEB: version denied by backend");
      return false;
    }

    bool updateAvailable = doc["update_available"] | false;
    const char *updateUrl = doc["url"] | "";
    if (updateAvailable && updateUrl[0] != '\0') {
      outResult->update_available = true;
      strncpy(outResult->update_url, updateUrl, sizeof(outResult->update_url) - 1);
      LOG_W("WEB: update available url=%s", outResult->update_url);
    }

    LOG_I("WEB: version check ok");
    return true;
  }

  LOG_E("WEB: version check failed");
  return false;
}

bool WebService::fetchMqttCredentials(WebBootstrapResult *outResult) {
  if (!outResult) {
    return false;
  }

  memset(&outResult->mqtt, 0, sizeof(outResult->mqtt));

  char url[192] = {0};
  if (!buildUrl(_config.mqtt_bootstrap_path, url, sizeof(url))) {
    LOG_E("WEB: mqtt bootstrap URL build failed");
    return false;
  }

  char body[256] = {0};
  // TODO: MQTT bootstrap request body'sini backend contract'inize gore duzenleyin.
  snprintf(body, sizeof(body), "{\"device_id\":\"%s\"}", _runtime.device.device_id);

  for (uint8_t attempt = 0; attempt <= _config.max_retries; ++attempt) {
    String response;
    int statusCode = -1;

    if (!postJson(url, body, response, statusCode)) {
      continue;
    }

    if (statusCode < 200 || statusCode >= 300) {
      LOG_W("WEB: mqtt bootstrap http=%d body=%s", statusCode, response.c_str());
      continue;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err) {
      LOG_E("WEB: mqtt bootstrap parse failed");
      continue;
    }

    const char *host = doc["host"] | "";
    uint16_t port = doc["port"] | 1883;
    const char *clientId = doc["client_id"] | _runtime.device.device_id;
    const char *username = doc["username"] | "";
    const char *password = doc["password"] | "";
    bool useTls = doc["use_tls"] | false;

    // TODO: Response alan isimleri backend'inize gore degisebilir.
    if (host[0] == '\0') {
      LOG_E("WEB: mqtt bootstrap host missing");
      continue;
    }

    strncpy(outResult->mqtt.host, host, sizeof(outResult->mqtt.host) - 1);
    outResult->mqtt.port = port;
    strncpy(outResult->mqtt.client_id, clientId,
            sizeof(outResult->mqtt.client_id) - 1);
    strncpy(outResult->mqtt.username, username,
            sizeof(outResult->mqtt.username) - 1);
    strncpy(outResult->mqtt.password, password,
            sizeof(outResult->mqtt.password) - 1);
    outResult->mqtt.use_tls = useTls;
    outResult->mqtt.valid = true;

    LOG_I("WEB: mqtt credentials received host=%s port=%u", outResult->mqtt.host,
          static_cast<unsigned>(outResult->mqtt.port));
    return true;
  }

  LOG_E("WEB: mqtt credentials request failed");
  return false;
}

bool WebService::runBootstrap(WebBootstrapResult *outResult) {
  if (!_initialized || !outResult) {
    return false;
  }

  memset(outResult, 0, sizeof(*outResult));

  if (!_config.enabled) {
    LOG_W("WEB: disabled by config");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    LOG_W("WEB: bootstrap skipped, wifi disconnected");
    return false;
  }

  if (!performVersionCheck(outResult)) {
    return false;
  }

  if (outResult->update_available) {
    return true;
  }

  return fetchMqttCredentials(outResult);
}
