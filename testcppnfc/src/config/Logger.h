/**
 * @file Logger.h
 * @brief Makro Tabanlı Loglama (C-Style)
 * @version 2.0.0 - Hibrit Mimari
 * 
 * Release modda tamamen devre dışı bırakılabilir.
 * Flash tasarrufu için makro tabanlı.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "Config.h"

// Log Level tanımları Config.h'da yapıldı

#if LOG_LEVEL >= LOG_LEVEL_ERROR
    #define LOG_E(fmt, ...) Serial.printf("[E] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_E(...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
    #define LOG_W(fmt, ...) Serial.printf("[W] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_W(...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
    #define LOG_I(fmt, ...) Serial.printf("[I] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_I(...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    #define LOG_D(fmt, ...) Serial.printf("[D] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_D(...)
#endif

// Hex dump helper (DEBUG modda)
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    static inline void LOG_HEX(const char* label, const uint8_t* data, uint8_t len) {
        Serial.printf("[D] %s: ", label);
        for (uint8_t i = 0; i < len; i++) {
            Serial.printf("%02X ", data[i]);
        }
        Serial.println();
    }
#else
    #define LOG_HEX(label, data, len)
#endif

// Function entry/exit tracing (DEBUG modda)
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    #define LOG_FUNC_ENTER() LOG_D(">> %s", __FUNCTION__)
    #define LOG_FUNC_EXIT()  LOG_D("<< %s", __FUNCTION__)
#else
    #define LOG_FUNC_ENTER()
    #define LOG_FUNC_EXIT()
#endif

#endif // LOGGER_H
