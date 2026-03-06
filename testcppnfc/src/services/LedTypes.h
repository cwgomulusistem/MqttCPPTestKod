/**
 * @file LedTypes.h
 * @brief LED Animasyon Servisi Tipleri
 * @version 1.0.0
 *
 * FreeRTOS kuyruğu üzerinden LED servisine gönderilen komutlar.
 */

#ifndef LED_TYPES_H
#define LED_TYPES_H

#include <stdint.h>

// =============================================================================
// LED ANIMASYON MODLARI
// =============================================================================

enum LedAnimMode : uint8_t {
  LED_ANIM_OFF = 0,        // Tum LED'ler kapali
  LED_ANIM_IDLE,           // Bekleme animasyonu (kosan nokta)
  LED_ANIM_WAIT_NETWORK,   // Ag bekleme (tek-cift alternans)
  LED_ANIM_WAIT_PAYMENT,   // Odeme bekleme (dolup-bosalma)
  LED_ANIM_SOLID_ON,       // Tum LED'ler acik
  LED_ANIM_SOLID_MASK,     // Ozel bitmask (32 bite kadar)
  LED_ANIM_FLASH_SUCCESS,  // Basari flash
  LED_ANIM_FLASH_ERROR,    // Hata flash
  LED_ANIM_ERROR           // Surekli hata blink
};

// =============================================================================
// SERVIS KOMUTLARI
// =============================================================================

enum LedCommand : uint8_t {
  LED_CMD_SET_MODE = 0, // Kalici mod degistir
  LED_CMD_FLASH_SUCCESS,
  LED_CMD_FLASH_ERROR,
  LED_CMD_SET_MASK // Bitmask (kalici veya sureli)
};

// =============================================================================
// KUYRUK MESAJI
// =============================================================================

struct LedMessage {
  LedCommand command;   // Komut tipi
  LedAnimMode mode;     // LED_CMD_SET_MODE icin hedef mod
  uint32_t mask;        // LED_CMD_SET_MASK icin bitmask
  uint16_t duration_ms; // Sureli komutlarda zaman (0 = varsayilan/kalici)
};

// =============================================================================
// YARDIMCI FONKSIYONLAR
// =============================================================================

static inline const char *LedAnimMode_toString(LedAnimMode mode) {
  switch (mode) {
  case LED_ANIM_OFF:
    return "OFF";
  case LED_ANIM_IDLE:
    return "IDLE";
  case LED_ANIM_WAIT_NETWORK:
    return "WAIT_NETWORK";
  case LED_ANIM_WAIT_PAYMENT:
    return "WAIT_PAYMENT";
  case LED_ANIM_SOLID_ON:
    return "SOLID_ON";
  case LED_ANIM_SOLID_MASK:
    return "SOLID_MASK";
  case LED_ANIM_FLASH_SUCCESS:
    return "FLASH_SUCCESS";
  case LED_ANIM_FLASH_ERROR:
    return "FLASH_ERROR";
  case LED_ANIM_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

static inline const char *LedCommand_toString(LedCommand command) {
  switch (command) {
  case LED_CMD_SET_MODE:
    return "SET_MODE";
  case LED_CMD_FLASH_SUCCESS:
    return "FLASH_SUCCESS";
  case LED_CMD_FLASH_ERROR:
    return "FLASH_ERROR";
  case LED_CMD_SET_MASK:
    return "SET_MASK";
  default:
    return "UNKNOWN";
  }
}

#endif // LED_TYPES_H
