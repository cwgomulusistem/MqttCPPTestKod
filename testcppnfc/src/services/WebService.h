/**
 * @file WebService.h
 * @brief Cihaz acilis bootstrap API servisi
 */

#ifndef WEB_SERVICE_H
#define WEB_SERVICE_H

#include <Arduino.h>

#include "../config/Config.h"
#include "MqttTypes.h"

typedef struct {
  bool update_available;
  char update_url[192];
  MqttConnectionInfo mqtt;
} WebBootstrapResult;

class WebService {
private:
  WebApiConfig _config;
  RuntimeValues _runtime;
  bool _initialized;

  bool buildUrl(const char *path, char *out, size_t outLen) const;
  bool postJson(const char *url, const char *jsonBody, String &response,
                int &statusCode) const;
  bool performVersionCheck(WebBootstrapResult *outResult);
  bool fetchMqttCredentials(WebBootstrapResult *outResult);

public:
  WebService();

  bool init(const WebApiConfig *config, const RuntimeValues *runtime);
  bool isInitialized() const;

  bool runBootstrap(WebBootstrapResult *outResult);
};

#endif // WEB_SERVICE_H
