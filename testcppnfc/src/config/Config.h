/**
 * @file Config.h
 * @brief Funtoria NFC Sistemi - Merkezi Konfigürasyon (C-Style)
 * @version 2.0.0 - Hibrit Mimari
 *
 * typedef struct ile bellek verimli konfigürasyon.
 * Vtable overhead yok, her byte'ın yeri belli.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <string.h>

#include "RuntimeValues.h"
#include "ServiceSwitches.h"

// Pin Tanımları (Compile-time constant)
#define PN532_UART_RX 16 // ESP32 UART2 RX (PN532 TX'e bağlanacak)
#define PN532_UART_TX 17 // ESP32 UART2 TX (PN532 RX'e bağlanacak)

// LCD SPI Pin Tanımları
#define LCD_SPI_MOSI 23
#define LCD_SPI_MISO 19
#define LCD_SPI_SCLK 18
#define LCD_CS 5
#define LCD_DC 25
#define LCD_RST 33
#define LCD_BL 21 // Backlight PWM

// Sistem Sabitleri
#define MAX_UID_LENGTH 10
#define APDU_BUFFER_SIZE 260
#define MIFARE_BLOCK_SIZE 16
#define MIFARE_KEY_SIZE 6

// LCD Sabitleri
#define LCD_WIDTH 240
#define LCD_HEIGHT 320
#define LCD_BUF_LINES 10 // LVGL partial buffer satır sayısı
#define LCD_ENABLED SERVICE_ENABLE_LCD

// LED Sabitleri (Funtoria: 22 adreslenebilir LED - WS2812)
#define LED_ENABLED SERVICE_ENABLE_LED
#define LED_COUNT 22
#define LED_DATA_PIN 3
#define LED_RMT_CHANNEL 0

// Servis Switch Sabitleri
#define NFC_ENABLED SERVICE_ENABLE_NFC
#define SPIFFS_ENABLED SERVICE_ENABLE_SPIFFS
#define WIFI_ENABLED SERVICE_ENABLE_WIFI
#define WEB_ENABLED SERVICE_ENABLE_WEB
#define MQTT_ENABLED SERVICE_ENABLE_MQTT

// Log Seviyeleri
#define LOG_LEVEL_OFF 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_DEBUG 4

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

// NFC Debounce/Cooldown Ayarları
#define NFC_DEBOUNCE_ENABLED true
#define NFC_COOLDOWN_MS_DEFAULT 1000 // 3 saniye (varsayılan)
#define NFC_COOLDOWN_MS_MIN 500      // Minimum 0.5 saniye
#define NFC_COOLDOWN_MS_MAX 30000    // Maximum 30 saniye
#define NFC_CARD_REMOVED_THRESHOLD                                             \
  700 // Kart cekildi sayilmasi icin ms (HCE ~500-1000ms suruyor)

// SPIFFS Ayarları
#define SPIFFS_NFC_SETTINGS_PATH "/config/nfc_settings.json"
#define SPIFFS_SETUP_CARD_PATH "/config/setup_card.json"
#define SPIFFS_FORMAT_ON_FAIL true

// UART (HSU) Konfigürasyonu
typedef struct {
  uint8_t rx_pin;
  uint8_t tx_pin;
  uint32_t baud_rate;
} UartConfig;

// NFC Modül Konfigürasyonu
typedef struct {
  uint8_t reset_pin;
  uint8_t max_retries;
  uint16_t tag_detect_timeout_ms;
  uint16_t card_debounce_ms;
  uint16_t apdu_timeout_ms;
} NFCConfig;

// Mifare Konfigürasyonu
typedef struct {
  uint8_t auth_block;
  uint8_t data_block;
  uint8_t key_default[MIFARE_KEY_SIZE];
  uint8_t key_funtoria[MIFARE_KEY_SIZE];
} MifareConfig;

// LCD Konfigürasyonu
typedef struct {
  bool enabled; // LCD aktif/pasif
  uint8_t mosi_pin;
  uint8_t miso_pin;
  uint8_t sclk_pin;
  uint8_t cs_pin;
  uint8_t dc_pin;
  uint8_t rst_pin;
  uint8_t bl_pin;
  uint16_t width;
  uint16_t height;
  uint8_t rotation; // 0-3 (0=portrait, 1=landscape, 2=portrait inv, 3=landscape
                    // inv)
  uint8_t bl_brightness; // 0-255 PWM
} LcdConfig;

// LCD Task Konfigürasyonu
typedef struct {
  uint16_t task_stack;   // Task stack boyutu (bytes)
  uint8_t task_priority; // Task önceliği
  uint8_t queue_size;    // Kuyruk kapasitesi
} LcdTaskConfig;

// LED Konfigürasyonu
typedef struct {
  bool enabled;      // LED servisi aktif/pasif
  uint8_t led_count; // Toplam LED sayısı
  uint8_t data_pin;  // WS2812 data pini
  uint8_t rmt_channel; // RMT kanal numarasi
} LedConfig;

// LED Task Konfigürasyonu
typedef struct {
  uint16_t task_stack;      // Task stack boyutu (bytes)
  uint8_t task_priority;    // Task önceliği
  uint8_t queue_size;       // Kuyruk kapasitesi
  uint16_t animation_step_ms; // Animasyon adım süresi
} LedTaskConfig;

// WiFi Konfigürasyonu
typedef struct {
  bool enabled;
  char ssid[33];
  char password[65];
  uint16_t connect_timeout_ms;
  uint16_t reconnect_interval_ms;
} WifiConfig;

// Web API Konfigürasyonu
typedef struct {
  bool enabled;
  char base_url[96];
  char version_check_path[64];
  char mqtt_bootstrap_path[64];
  uint16_t request_timeout_ms;
  uint8_t max_retries;
} WebApiConfig;

// MQTT Servis Konfigürasyonu
typedef struct {
  bool enabled;
  uint16_t task_stack;
  uint8_t task_priority;
  uint8_t queue_size;
  uint16_t loop_delay_ms;
  uint32_t reconnect_delay_ms;
  uint16_t keep_alive_sec;
  bool clean_session;
  char fallback_host[64];
  uint16_t fallback_port;
  char fallback_username[64];
  char fallback_password[64];
  char fallback_client_id[48];
  char lwt_topic[96];
  char lwt_payload[32];
  uint8_t lwt_qos;
  bool lwt_retain;
} MqttServiceConfig;

// Sistem/FreeRTOS Konfigürasyonu
typedef struct {
  uint32_t serial_baud;
  uint16_t nfc_task_stack;
  uint8_t nfc_task_priority;
  uint16_t nfc_loop_delay_ms;
  uint16_t wdt_timeout_sec;
  bool nfc_enabled;
  bool spiffs_enabled;
} SystemConfig;

// Ana Uygulama Konfigürasyonu
typedef struct {
  UartConfig uart;
  NFCConfig nfc;
  MifareConfig mifare;
  LcdConfig lcd;
  LcdTaskConfig lcd_task;
  LedConfig led;
  LedTaskConfig led_task;
  WifiConfig wifi;
  WebApiConfig web_api;
  MqttServiceConfig mqtt;
  SystemConfig system;
  RuntimeValues runtime; // MAC, Device ID gibi runtime paylasim alani
  uint8_t log_level;
} AppConfig;

// Varsayılan Değerleri Yükle (Constructor yerine)
static inline void Config_setDefaults(AppConfig *config) {
  memset(config, 0, sizeof(AppConfig));

  // UART (HSU)
  config->uart.rx_pin = PN532_UART_RX;
  config->uart.tx_pin = PN532_UART_TX;
  config->uart.baud_rate = 115200; // PN532 HSU default baud rate

  // NFC
  config->nfc.reset_pin = 0xFF; // Reset pini kullanilmiyor (NFC kapali)
  config->nfc.max_retries = 5;
  config->nfc.tag_detect_timeout_ms = 150;
  config->nfc.card_debounce_ms = 1000;
  config->nfc.apdu_timeout_ms = 500;

  // Mifare
  config->mifare.auth_block = 4;
  config->mifare.data_block = 5;

  // Default Key: FF FF FF FF FF FF
  memset(config->mifare.key_default, 0xFF, MIFARE_KEY_SIZE);

  // Funtoria Key: "ozveri" hex
  config->mifare.key_funtoria[0] = 0x6F;
  config->mifare.key_funtoria[1] = 0x7A;
  config->mifare.key_funtoria[2] = 0x76;
  config->mifare.key_funtoria[3] = 0x65;
  config->mifare.key_funtoria[4] = 0x72;
  config->mifare.key_funtoria[5] = 0x69;

  // LCD
  config->lcd.enabled = LCD_ENABLED;
  config->lcd.mosi_pin = LCD_SPI_MOSI;
  config->lcd.miso_pin = LCD_SPI_MISO;
  config->lcd.sclk_pin = LCD_SPI_SCLK;
  config->lcd.cs_pin = LCD_CS;
  config->lcd.dc_pin = LCD_DC;
  config->lcd.rst_pin = LCD_RST;
  config->lcd.bl_pin = LCD_BL;
  config->lcd.width = LCD_WIDTH;
  config->lcd.height = LCD_HEIGHT;
  config->lcd.rotation = 0;        // Portrait
  config->lcd.bl_brightness = 255; // Tam parlaklık

  // LCD Task
  config->lcd_task.task_stack = 16384; // 16K - LVGL grafik işlemleri için
  config->lcd_task.task_priority = 3;
  config->lcd_task.queue_size = 10;

  // LED
  config->led.enabled = LED_ENABLED;
  config->led.led_count = LED_COUNT;
  config->led.data_pin = LED_DATA_PIN;
  config->led.rmt_channel = LED_RMT_CHANNEL;

  // LED Task
  config->led_task.task_stack = 4096;
  config->led_task.task_priority = 2;
  config->led_task.queue_size = 8;
  // LED gorev dongusu icin temel adim suresi
  config->led_task.animation_step_ms = 20;

  // WiFi
  config->wifi.enabled = WIFI_ENABLED;
  config->wifi.connect_timeout_ms = 15000;
  config->wifi.reconnect_interval_ms = 5000;

  // Web API
  config->web_api.enabled = WEB_ENABLED;
  strncpy(config->web_api.base_url, "http://api.funtoria.local",
          sizeof(config->web_api.base_url) - 1);
  strncpy(config->web_api.version_check_path, "/v1/device/version/check",
          sizeof(config->web_api.version_check_path) - 1);
  strncpy(config->web_api.mqtt_bootstrap_path, "/v1/device/mqtt/bootstrap",
          sizeof(config->web_api.mqtt_bootstrap_path) - 1);
  config->web_api.request_timeout_ms = 8000;
  config->web_api.max_retries = 2;

  // MQTT
  config->mqtt.enabled = MQTT_ENABLED;
  config->mqtt.task_stack = 8192;
  config->mqtt.task_priority = 4;
  config->mqtt.queue_size = 16;
  config->mqtt.loop_delay_ms = 30;
  config->mqtt.reconnect_delay_ms = 30000; // 30 saniye
  config->mqtt.keep_alive_sec = 30;
  config->mqtt.clean_session = true;
  config->mqtt.fallback_port = 1883;
  strncpy(config->mqtt.lwt_payload, "offline",
          sizeof(config->mqtt.lwt_payload) - 1);
  config->mqtt.lwt_qos = 1;
  config->mqtt.lwt_retain = true;

  // System
  config->system.serial_baud = 115200;
  config->system.nfc_task_stack = 4096;
  config->system.nfc_task_priority = 5;
  config->system.nfc_loop_delay_ms = 100;
  config->system.wdt_timeout_sec = 20;
  config->system.nfc_enabled = NFC_ENABLED;
  config->system.spiffs_enabled = SPIFFS_ENABLED;

  // Log
  RuntimeValues_clear(&config->runtime);
  config->log_level = LOG_LEVEL_INFO;
}

// =============================================================================
// MOBİL ÖDEME (HCE) KONFİGÜRASYONU
// =============================================================================

// Funtoria AID (Application Identifier) - 7 byte
// A0 00 00 02 47 10 01 (Standart RID+PIX formati)
// NOT: Mobil uygulamanizda (apduservice.xml + HCEService) ayni AID olmali!
#define FUNTORIA_AID_LENGTH 7
static const uint8_t FUNTORIA_AID[FUNTORIA_AID_LENGTH] = {
    0xA0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01};

// Mobil Odeme Sabitleri
#define MOBILE_TOKEN_MAX_LENGTH 64   // Maksimum token uzunluğu
#define MOBILE_USER_ID_MAX_LENGTH 32 // Maksimum kullanıcı ID uzunluğu
#define MOBILE_APDU_TIMEOUT_MS 1000  // APDU timeout (ms)

// =============================================================================
// APDU KOMUTLARI (ISO 7816-4)
// =============================================================================

// SELECT AID Komutu Header (CLA INS P1 P2)
static const uint8_t APDU_SELECT_HEADER[4] = {
    0x00, // CLA: Standard
    0xA4, // INS: SELECT
    0x04, // P1:  Select by DF name (AID)
    0x00  // P2:  First or only occurrence
};

// GET DATA Komutları (Özel - Mobil uygulamanızla anlaşmalı)
static const uint8_t APDU_GET_USER_ID[5] = {
    0x80, // CLA: Proprietary
    0xCA, // INS: GET DATA
    0x00, // P1:  User ID tag (high byte)
    0x01, // P2:  User ID tag (low byte)
    0x00  // Le:  Expected max response length
};

static const uint8_t APDU_GET_TOKEN[5] = {
    0x80, // CLA: Proprietary
    0xCA, // INS: GET DATA
    0x00, // P1:  Token tag (high byte)
    0x02, // P2:  Token tag (low byte)
    0x00  // Le:  Expected max response length
};

static const uint8_t APDU_GET_BALANCE[5] = {
    0x80, // CLA: Proprietary
    0xCA, // INS: GET DATA
    0x00, // P1:  Balance tag (high byte)
    0x03, // P2:  Balance tag (low byte)
    0x00  // Le:  Expected max response length
};

// =============================================================================
// APDU DURUM KODLARI (SW1 SW2)
// =============================================================================

#define SW_SUCCESS_1 0x90
#define SW_SUCCESS_2 0x00

#define SW_NOT_FOUND_1 0x6A
#define SW_NOT_FOUND_2 0x82

#define APDU_IS_SUCCESS(sw1, sw2)                                              \
  ((sw1) == SW_SUCCESS_1 && (sw2) == SW_SUCCESS_2)

#endif // CONFIG_H
