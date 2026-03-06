/**
 * @file OtaService.cpp
 * @brief HTTP/HTTPS OTA guncelleme servisi
 */

#include "OtaService.h"
#include "../config/Logger.h"

#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <string.h>

bool OtaService::runUpdateFromUrl(const char *url) {
  if (!url || url[0] == '\0') {
    LOG_E("OTA: empty URL");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    LOG_E("OTA: WiFi not connected");
    return false;
  }

  LOG_W("OTA: update started url=%s", url);
  httpUpdate.rebootOnUpdate(true);

  t_httpUpdate_return ret;
  if (strncmp(url, "https://", 8) == 0) {
    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    ret = httpUpdate.update(secureClient, url);
  } else {
    WiFiClient client;
    ret = httpUpdate.update(client, url);
  }

  switch (ret) {
  case HTTP_UPDATE_FAILED:
    LOG_E("OTA: failed code=%d err=%s", httpUpdate.getLastError(),
          httpUpdate.getLastErrorString().c_str());
    return false;
  case HTTP_UPDATE_NO_UPDATES:
    LOG_W("OTA: no updates");
    return false;
  case HTTP_UPDATE_OK:
    LOG_I("OTA: update ok, rebooting");
    ESP.restart();
    return true;
  default:
    return false;
  }
}
