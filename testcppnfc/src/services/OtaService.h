/**
 * @file OtaService.h
 * @brief HTTP/HTTPS OTA guncelleme servisi
 */

#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

class OtaService {
public:
  bool runUpdateFromUrl(const char *url);
};

#endif // OTA_SERVICE_H
