/**
 * @file RuntimeValues.h
 * @brief Tasklar arasi paylasilan runtime degerleri (MAC, Device ID vb.)
 */

#ifndef RUNTIME_VALUES_H
#define RUNTIME_VALUES_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DEVICE_HEX_LENGTH 12
#define DEVICE_HALF_LENGTH (DEVICE_HEX_LENGTH / 2)

// Cihaz kimlik bilgileri (runtime'da hesaplanir)
typedef struct {
  char mac_hex[DEVICE_HEX_LENGTH + 1];
  char device_id[DEVICE_HEX_LENGTH + 1];
} DeviceIdentity;

// Tum task'larin okuyabilecegi runtime alanlari
typedef struct {
  DeviceIdentity device;
} RuntimeValues;

static inline void RuntimeValues_clear(RuntimeValues *runtime) {
  if (!runtime) {
    return;
  }
  memset(runtime, 0, sizeof(RuntimeValues));
}

static inline void RuntimeValues_reverseText(char *text, size_t len) {
  if (!text || len < 2) {
    return;
  }

  for (size_t i = 0; i < (len / 2); ++i) {
    char tmp = text[i];
    text[i] = text[len - 1 - i];
    text[len - 1 - i] = tmp;
  }
}

// Device ID kurali:
// 1) MAC (12 hex) ikiye bolunur
// 2) Sadece ikinci yari ters cevrilir
// 3) Device ID = ters ikinci yari + birinci yari
static inline void RuntimeValues_setFromMac(RuntimeValues *runtime,
                                            uint64_t mac_raw) {
  if (!runtime) {
    return;
  }

  snprintf(runtime->device.mac_hex, sizeof(runtime->device.mac_hex),
           "%02X%02X%02X%02X%02X%02X",
           (unsigned int)((mac_raw >> 40) & 0xFF),
           (unsigned int)((mac_raw >> 32) & 0xFF),
           (unsigned int)((mac_raw >> 24) & 0xFF),
           (unsigned int)((mac_raw >> 16) & 0xFF),
           (unsigned int)((mac_raw >> 8) & 0xFF),
           (unsigned int)(mac_raw & 0xFF));

  char first_half[DEVICE_HALF_LENGTH + 1] = {0};
  char second_half[DEVICE_HALF_LENGTH + 1] = {0};
  memcpy(first_half, runtime->device.mac_hex, DEVICE_HALF_LENGTH);
  memcpy(second_half, runtime->device.mac_hex + DEVICE_HALF_LENGTH,
         DEVICE_HALF_LENGTH);

  RuntimeValues_reverseText(second_half, DEVICE_HALF_LENGTH);

  snprintf(runtime->device.device_id, sizeof(runtime->device.device_id), "%s%s",
           second_half, first_half);
}

#endif // RUNTIME_VALUES_H
