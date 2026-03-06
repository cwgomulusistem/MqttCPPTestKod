/**
 * @file LcdTypes.h
 * @brief LCD Servis Tipleri - Ekran Komutları ve Kuyruk Mesajları
 * @version 1.0.0
 *
 * Diğer servislerden LCD servisine gönderilen komut tipleri.
 * FreeRTOS kuyruğu üzerinden iletişim için mesaj yapısı.
 */

#ifndef LCD_TYPES_H
#define LCD_TYPES_H

#include <stdint.h>

// =============================================================================
// EKRAN KOMUT TİPLERİ
// =============================================================================

/**
 * @brief Ekran komutu enum'u
 *
 * Diğer servisler bu komutları kuyruğa göndererek
 * LCD ekranındaki görüntüyü değiştirir.
 */
enum ScreenCommand : uint8_t {
  SCREEN_CMD_SHOW_IDLE = 0,       // Bekleme ekranı (logo + "Hazır")
  SCREEN_CMD_SHOW_NFC_READ,       // Kart okundu bildirimi
  SCREEN_CMD_SHOW_PAYMENT_OK,     // Ödeme başarılı
  SCREEN_CMD_SHOW_PAYMENT_FAIL,   // Ödeme başarısız
  SCREEN_CMD_SHOW_ERROR,          // Hata mesajı
  SCREEN_CMD_SHOW_PHONE_DETECTED, // Telefon algılandı, uygulama aç
  SCREEN_CMD_SHOW_SETUP_REQUIRED, // Ilk kurulum: setup kart bekleniyor
  SCREEN_CMD_SET_BACKLIGHT,       // Backlight seviyesi değiştir
  SCREEN_CMD_CUSTOM_TEXT,         // Özel metin göster
};

// =============================================================================
// KUYRUK MESAJ YAPISI
// =============================================================================

/**
 * @brief Ekran mesajı (kuyruk elemanı)
 *
 * Bu struct FreeRTOS kuyruğuna kopyalanır (value semantics).
 * text1, text2 char array olduğu için pointer hatası riski YOK.
 * lv_label_set_text() metni LVGL içinde kopyalar, güvenlidir.
 *
 * Boyut: ~72 byte
 */
struct ScreenMessage {
  ScreenCommand command; // Komut tipi
  char text1[32];        // Ana metin (UID, kullanıcı ID, hata vb.)
  char text2[32];        // Alt metin (kart tipi, bakiye, detay vb.)
  uint32_t value;        // Sayısal değer (bakiye, backlight seviyesi vb.)
  uint32_t duration_ms; // Gösterim süresi (0 = süresiz, otomatik idle'a dönmez)
};

// =============================================================================
// YARDIMCI FONKSİYONLAR
// =============================================================================

/**
 * @brief ScreenCommand enum'unu string'e çevir (debug için)
 */
static inline const char *ScreenCommand_toString(ScreenCommand cmd) {
  switch (cmd) {
  case SCREEN_CMD_SHOW_IDLE:
    return "WAITING";
  case SCREEN_CMD_SHOW_NFC_READ:
    return "NFC_READ";
  case SCREEN_CMD_SHOW_PAYMENT_OK:
    return "PAYMENT_OK";
  case SCREEN_CMD_SHOW_PAYMENT_FAIL:
    return "PAYMENT_FAIL";
  case SCREEN_CMD_SHOW_ERROR:
    return "ERROR";
  case SCREEN_CMD_SHOW_PHONE_DETECTED:
    return "PHONE_DETECTED";
  case SCREEN_CMD_SHOW_SETUP_REQUIRED:
    return "SETUP_REQUIRED";
  case SCREEN_CMD_SET_BACKLIGHT:
    return "SET_BACKLIGHT";
  case SCREEN_CMD_CUSTOM_TEXT:
    return "CUSTOM_TEXT";
  default:
    return "UNKNOWN";
  }
}

#endif // LCD_TYPES_H
