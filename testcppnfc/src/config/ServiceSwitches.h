/**
 * @file ServiceSwitches.h
 * @brief Tum servislerin merkezi ac/kapat ayarlari
 *
 * Kullanim:
 * - true  => servis aktif
 * - false => servis devre disi
 *
 * Not: Bu dosya compile-time ana kontrol noktasi olarak tasarlandi.
 */

#ifndef SERVICE_SWITCHES_H
#define SERVICE_SWITCHES_H

// SPIFFS config dosya sistemi
#define SERVICE_ENABLE_SPIFFS true

// LED animasyon servisi (WS2812)
#define SERVICE_ENABLE_LED false

// LCD servis (ILI9341 + LVGL)
#define SERVICE_ENABLE_LCD true

// NFC servis (PN532 HSU)
// PN532 baglantisi hazir degilse false yap.
#define SERVICE_ENABLE_NFC true

// WiFi baglantisi
#define SERVICE_ENABLE_WIFI false

// Web API bootstrap servisi
#define SERVICE_ENABLE_WEB false

// MQTT servis
#define SERVICE_ENABLE_MQTT false

// Gecici: setup kart zorunlulugunu bypass et
// true  -> cihaz dogrudan normal moda gecer
// false -> setup kart gerekli olur
#define SERVICE_BYPASS_SETUP_PROVISIONING true

#endif // SERVICE_SWITCHES_H
