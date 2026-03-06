/**
 * @file main.cpp
 * @brief Funtoria NFC Sistemi - Ana Giriş Noktası
 * @version 2.2.0 - Hibrit Mimari + Mobil Ödeme + LCD Ekran
 *
 * Thin Abstraction Layer:
 * - Driver Layer: Pure C (PN532Driver, ILI9341Driver)
 * - System Layer: C++ RAII (OsWrappers)
 * - Service Layer: Hibrit C++ (NfcService, LcdService)
 *
 * Desteklenen Özellikler:
 * - Fiziksel NFC Kartlar (Mifare Classic, Ultralight)
 * - Mobil Ödeme (HCE - Host Card Emulation)
 *   - Android: Tam HCE desteği (arka planda da çalışır)
 *   - iOS: CoreNFC (uygulama açıkken)
 * - Debounce/Cooldown (kart spam önleme)
 * - SPIFFS ayar depolama
 * - ILI9341 LCD Ekran (LVGL + FreeRTOS Queue)
 *
 * Bellek Verimliliği:
 * - Global objeler BSS section'da (zero-initialized)
 * - Heap fragmentation YOK
 * - Vtable overhead YOK
 */

#include <Adafruit_PN532.h>
#include <Arduino.h>
#include <new>
#include <string.h>

#include "config/Config.h"
#include "config/Logger.h"
#include "drivers/NfcTypes.h"
#include "drivers/PN532Driver.h"
#include "logic/MifareCardHandler.h"
#include "services/LcdService.h"
#include "services/LedAnimationService.h"
#include "services/MqttService.h"
#include "services/NfcService.h"
#include "services/WebService.h"
#include "utils/SpiffsConfig.h"

// =============================================================================
// GLOBAL OBJELER
// - Cekirdek servisler BSS'te
// - Web/MQTT servisleri RAM baskisi nedeniyle setup'ta heap'te olusturulur
// =============================================================================

// Hardware instance (HSU - UART2, sadece NFC aktifken olusturulur)
Adafruit_PN532 *g_pn532Hw = nullptr;

// APDU buffer (stack overflow önleme için global)
ApduBuffer g_apduBuffer;

// NFC Service (Hibrit C++ class)
NfcService g_nfcService;

// LCD Service (Kuyruk tabanlı ekran yönetimi)
LcdService g_lcdService;

// LED Animasyon Servisi (22 LED - WS2812)
LedAnimationService g_ledService;

// Web Bootstrap Servisi (Version check + MQTT credentials)
WebService *g_webService = nullptr;

// MQTT Servisi (WiFi + Queue + Topic Handler)
MqttService *g_mqttService = nullptr;

// Uygulama konfigürasyonu
AppConfig g_appConfig;

// Mifare işlem konfigürasyonu
MifareOpConfig g_mifareConfig;

// Setup kart provisioning durumu
bool g_isSetupProvisioned = false;
bool g_setupProvisioningMode = false;

namespace {
constexpr uint8_t kSetupLengthFieldCount = 6;

size_t boundedFieldLen(const char *text) {
  if (!text) {
    return 0;
  }
  return strnlen(text, MifareHandler::SETUP_FIELD_DATA_BYTES);
}

bool validateSetupCardData(const MifareHandler::SetupCardData *data, char *reason,
                           size_t reasonLen) {
  if (!data) {
    if (reason && reasonLen > 0) {
      snprintf(reason, reasonLen, "setup_data_null");
    }
    return false;
  }

  if (data->card_type != CARD_TYPE_SETUP) {
    if (reason && reasonLen > 0) {
      snprintf(reason, reasonLen, "card_type_invalid");
    }
    return false;
  }

  const char *fields[kSetupLengthFieldCount] = {
      data->wifi_ssid,   data->wifi_password, data->ip_address,
      data->mfirm_id,    data->guid_part_1,   data->guid_part_2};
  const char *fieldNames[kSetupLengthFieldCount] = {
      "wifi_ssid", "wifi_password", "ip_address",
      "mfirm_id", "guid_part_1", "guid_part_2"};

  for (uint8_t i = 0; i < kSetupLengthFieldCount; i++) {
    uint8_t encodedLen = data->lengths_raw[i];
    size_t actualLen = boundedFieldLen(fields[i]);

    if (encodedLen == 0 || encodedLen > MifareHandler::SETUP_FIELD_DATA_BYTES) {
      if (reason && reasonLen > 0) {
        snprintf(reason, reasonLen, "%s_length_invalid", fieldNames[i]);
      }
      return false;
    }

    if (actualLen != encodedLen) {
      if (reason && reasonLen > 0) {
        snprintf(reason, reasonLen, "%s_length_mismatch", fieldNames[i]);
      }
      return false;
    }
  }

  if (data->wifi_ssid[0] == '\0' || data->wifi_password[0] == '\0' ||
      data->ip_address[0] == '\0') {
    if (reason && reasonLen > 0) {
      snprintf(reason, reasonLen, "mandatory_fields_missing");
    }
    return false;
  }

  return true;
}

void applySetupCardToConfig(const MifareHandler::SetupCardData *setupData) {
  if (!setupData) {
    return;
  }

  strncpy(g_appConfig.wifi.ssid, setupData->wifi_ssid,
          sizeof(g_appConfig.wifi.ssid) - 1);
  strncpy(g_appConfig.wifi.password, setupData->wifi_password,
          sizeof(g_appConfig.wifi.password) - 1);

  if (setupData->ip_address[0] != '\0') {
    if (strncmp(setupData->ip_address, "http://", 7) == 0 ||
        strncmp(setupData->ip_address, "https://", 8) == 0) {
      strncpy(g_appConfig.web_api.base_url, setupData->ip_address,
              sizeof(g_appConfig.web_api.base_url) - 1);
    } else {
      snprintf(g_appConfig.web_api.base_url, sizeof(g_appConfig.web_api.base_url),
               "http://%s", setupData->ip_address);
    }
  }
}
} // namespace

// =============================================================================
// CALLBACK FONKSİYONLARI
// =============================================================================

/**
 * @brief Fiziksel kart okunduğunda çağrılır
 *
 * NOT: Bu callback Mutex AÇIKKEN çağrılır (Unlock Before Callback pattern)
 * Bu sayede callback içinde uzun işlemler yapılabilir.
 */
void onCardRead(const NfcCardData *data) {
  if (!data) {
    return;
  }

  if (g_setupProvisioningMode) {
    if (data->tag_type != TAG_MIFARE_CLASSIC || data->card_type != CARD_TYPE_SETUP) {
      LOG_W("SETUP MODE: setup kart bekleniyor uid=%s",
            data->uid_string[0] ? data->uid_string : "-");
      if (g_appConfig.lcd.enabled) {
        g_lcdService.showSetupRequired(g_appConfig.runtime.device.device_id,
                                       g_appConfig.runtime.device.mac_hex);
      }
      return;
    }

    MifareHandler::SetupCardData setupData = {};
    if (!g_nfcService.getLastSetupCardData(&setupData)) {
      LOG_E("SETUP MODE: setup kart verisi alinamadi");
      if (g_appConfig.lcd.enabled) {
        g_lcdService.showSetupRequired(g_appConfig.runtime.device.device_id,
                                       g_appConfig.runtime.device.mac_hex);
      }
      if (g_appConfig.led.enabled) {
        g_ledService.flashError(1000);
      }
      return;
    }

    char reason[64] = {0};
    if (!validateSetupCardData(&setupData, reason, sizeof(reason))) {
      LOG_E("SETUP MODE: setup kart gecersiz (%s)", reason);
      if (g_appConfig.lcd.enabled) {
        g_lcdService.showSetupRequired(g_appConfig.runtime.device.device_id,
                                       g_appConfig.runtime.device.mac_hex);
      }
      if (g_appConfig.led.enabled) {
        g_ledService.flashError(1400);
      }
      return;
    }

    if (!SpiffsConfig::saveSetupCardData(&setupData)) {
      LOG_E("SETUP MODE: setup SPIFFS yazma hatasi");
      if (g_appConfig.lcd.enabled) {
        g_lcdService.showSetupRequired(g_appConfig.runtime.device.device_id,
                                       g_appConfig.runtime.device.mac_hex);
      }
      if (g_appConfig.led.enabled) {
        g_ledService.flashError(1400);
      }
      return;
    }

    applySetupCardToConfig(&setupData);
    LOG_I("SETUP MODE: setup kart kaydedildi, cihaz yeniden baslatiliyor");

    if (g_appConfig.led.enabled) {
      g_ledService.flashSuccess(1200);
    }

    delay(900);
    ESP.restart();
    return;
  }

  // === TELEFON ALGILANDI AMA HCE AKTİF DEĞİL ===
  if (data->mobile_app_not_ready) {
    LOG_W("========================================");
    LOG_W(">>> TELEFON ALGILANDI - UYGULAMA ACIN!");
    LOG_W("========================================");
    LOG_W("UID: %s", data->uid_string);
    LOG_W("Telefon NFC algilandi ancak HCE aktif degil.");
    LOG_W("Lutfen telefonda Funtoria uygulamasini acin");
    LOG_W("ve tekrar yaklastirin.");
    LOG_W("========================================");

    // LCD: Telefon algılandı ekranı göster
    if (g_appConfig.lcd.enabled) {
      g_lcdService.showPhoneDetected();
    }

    if (g_appConfig.led.enabled) {
      g_ledService.flashError(1200);
    }
    return;
  }

  // === NORMAL FİZİKSEL KART ===
  LOG_I("========================================");
  LOG_I(">>> FIZIKSEL KART ALGILANDI");
  LOG_I("========================================");
  LOG_I("UID: %s", data->uid_string);
  LOG_I("Tip: %s", NfcTagType_toString(data->tag_type));
  LOG_I("CardType: %s", MifareHandler::cardTypeToString(data->card_type));

  // LCD: Kart okundu ekranı göster
  if (g_appConfig.lcd.enabled) {
    g_lcdService.showNfcRead(data->uid_string,
                             NfcTagType_toString(data->tag_type));
  }

  if (g_appConfig.led.enabled) {
    g_ledService.flashSuccess(800);
  }

  // Mifare Classic ise ek işlem yap
  if (data->tag_type == TAG_MIFARE_CLASSIC) {
    LOG_I("Mifare Classic kart");

    if (data->card_type == CARD_TYPE_SETUP) {
      LOG_I("Setup card algilandi");
    } else if (data->card_type == CARD_TYPE_CUSTOMER) {
      LOG_I("Customer card algilandi");
    } else if (data->card_type == CARD_TYPE_GIFT) {
      LOG_I("Gift card algilandi");
    } else if (data->card_type == CARD_TYPE_SERVICE) {
      LOG_I("Service card algilandi");
    }

    // İleride: bakiye okuma, kart tipi kontrolü vb.
    // NfcService üzerinden thread-safe erişim:
    // uint8_t buffer[16];
    // g_nfcService.readMifareBlockSafe(block, key, buffer);
  }

  LOG_I("========================================");
}

/**
 * @brief Mobil ödeme algılandığında çağrılır (HCE)
 *
 * Bu callback, telefon tespit edilip Funtoria uygulaması
 * başarıyla seçildiğinde çağrılır.
 *
 * Gelen veri:
 * - user_id: Kullanıcı ID'si (mobil uygulamadan)
 * - token: Tek kullanımlık ödeme token'ı
 * - balance: Kullanıcı bakiyesi (varsa)
 */
void onMobilePayment(const MobilePaymentData *data) {
  if (!data) {
    return;
  }

  if (g_setupProvisioningMode) {
    LOG_W("SETUP MODE: mobil odeme akisi pasif");
    return;
  }

  LOG_I("========================================");
  LOG_I(">>> MOBIL ODEME ALGILANDI (HCE)");
  LOG_I("========================================");

  LOG_I("Durum: %s", MobilePaymentStatus_toString(data->status));
  LOG_I("UID: %s", data->uid_string);

  LOG_I("Platform: Android (HCE)");

  // Kullanıcı bilgileri
  if (data->user_id_length > 0) {
    LOG_I("Kullanici ID: %s", data->user_id);
  }

  // Token
  if (data->token_length > 0) {
    LOG_I("Token: %s", data->token);

    // ============================================
    // BACKEND'E TOKEN GONDER
    // ============================================
    // Burada WiFi/MQTT ile backend'e token gönderilir
    // Backend token'ı doğrular ve ödemeyi gerçekleştirir
    //
    // Örnek akış:
    // 1. HTTP POST /api/payment/verify
    //    Body: { "token": "...", "device_id": "..." }
    // 2. Backend token'ı kontrol eder
    // 3. Kullanıcının kartından tahsilat yapar
    // 4. Sonucu ESP32'ye döner
    // 5. ESP32 LED/Buzzer ile kullanıcıya bildirir
    // ============================================

    LOG_I(">>> BACKEND'E GONDERILECEK: Token=%s, DeviceID=%s", data->token,
          g_appConfig.runtime.device.device_id);
  }

  // Bakiye bilgisi (varsa)
  if (data->balance_available) {
    float balance_tl = data->balance / 100.0f;
    LOG_I("Bakiye: %.2f TL", balance_tl);
  }

  // LCD: Ödeme başarılı ekranı göster
  if (g_appConfig.lcd.enabled) {
    g_lcdService.showPaymentOk(data->user_id, data->balance);
  }

  if (g_appConfig.led.enabled) {
    g_ledService.flashSuccess(1400);
  }

  // APDU durum kodu
  LOG_D("APDU Status: SW1=0x%02X, SW2=0x%02X", data->sw1, data->sw2);

  LOG_I("========================================");
  LOG_I(">>> ODEME ISLEMI TAMAMLANDI");
  LOG_I("========================================");
}

void onMqttMessage(const char *topic, const uint8_t *payload, size_t len,
                   void *userData) {
  (void)userData;

  char payloadText[MQTT_MAX_PAYLOAD_LENGTH + 1] = {0};
  size_t copyLen = len;
  if (copyLen > MQTT_MAX_PAYLOAD_LENGTH) {
    copyLen = MQTT_MAX_PAYLOAD_LENGTH;
  }

  if (payload && copyLen > 0) {
    memcpy(payloadText, payload, copyLen);
  }
  payloadText[copyLen] = '\0';

  LOG_I("MQTT RX topic=%s payload=%s", topic ? topic : "", payloadText);

  // TODO: Burada topic'e gore komut islemlerini ekleyin.
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  // Serial başlat
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    delay(10); // Serial bağlantısı için bekle (maks 3 saniye)
  }

  LOG_I("========================================");
  LOG_I("Funtoria NFC System v2.2.0");
  LOG_I("Hibrit Mimari + Mobil Odeme + LCD Ekran");
  LOG_I("========================================");

  // Konfigürasyonu yükle
  Config_setDefaults(&g_appConfig);
  LOG_D("Config loaded");

  // Tasklarin ortak kullanacagi runtime kimlik alanlarini doldur
  RuntimeValues_setFromMac(&g_appConfig.runtime, ESP.getEfuseMac());
  LOG_I("MAC: %s", g_appConfig.runtime.device.mac_hex);
  LOG_I("Device ID: %s", g_appConfig.runtime.device.device_id);

  // SPIFFS başlat
  bool spiffsReady = false;
  if (!g_appConfig.system.spiffs_enabled) {
    LOG_W("SPIFFS disabled from service switches");
  } else {
    spiffsReady = SpiffsConfig::init();
    if (!spiffsReady) {
      LOG_W("SPIFFS init failed - setup persistence disabled");
    }
  }

  if (spiffsReady) {
    MifareHandler::SetupCardData setupFromDisk = {};
    if (SpiffsConfig::loadSetupCardData(&setupFromDisk)) {
      char validationReason[64] = {0};
      if (validateSetupCardData(&setupFromDisk, validationReason,
                                sizeof(validationReason))) {
        applySetupCardToConfig(&setupFromDisk);
        g_isSetupProvisioned = true;
        LOG_I("Setup config loaded from SPIFFS");
      } else {
        LOG_W("Setup config found but invalid (%s)", validationReason);
      }
    } else {
      LOG_W("Setup config not found in SPIFFS");
    }
  }

  // Gecici saha testleri icin setup kart gerekliligini devre disi birak.
#if SERVICE_BYPASS_SETUP_PROVISIONING
  g_isSetupProvisioned = true;
  g_setupProvisioningMode = false;
  LOG_W("Setup provisioning bypass AKTIF (setup kart zorunlulugu kapali)");
#else
  g_setupProvisioningMode = !g_isSetupProvisioned;
#endif
  LOG_I("Provisioning mode: %s", g_setupProvisioningMode ? "AKTIF" : "KAPALI");

  // === WEB + MQTT Servisleri Başlat ===
  if (g_setupProvisioningMode) {
    LOG_W("WEB/MQTT setup mode'da pasif (setup kart bekleniyor)");
  } else {
    bool webInitialized = false;

    if (g_appConfig.web_api.enabled || g_appConfig.mqtt.enabled) {
      g_webService = new (std::nothrow) WebService();
      if (!g_webService) {
        LOG_E("WEB service allocation FAILED!");
      }
    }

    if (g_appConfig.mqtt.enabled) {
      g_mqttService = new (std::nothrow) MqttService();
      if (!g_mqttService) {
        LOG_E("MQTT service allocation FAILED!");
      }
    }

    if (!g_appConfig.web_api.enabled) {
      LOG_W("WEB service disabled from service switches");
    } else if (!g_webService) {
      LOG_E("WEB service instance not available");
    } else if (!g_webService->init(&g_appConfig.web_api, &g_appConfig.runtime)) {
      LOG_E("WEB service init FAILED!");
    } else {
      webInitialized = true;
    }

    if (!g_appConfig.mqtt.enabled) {
      LOG_W("MQTT service disabled from service switches");
    } else if (!g_mqttService) {
      LOG_E("MQTT service instance not available");
    } else {
      WebService *webPtr = webInitialized ? g_webService : nullptr;
      if (!g_mqttService->init(&g_appConfig.wifi, &g_appConfig.mqtt, webPtr,
                               &g_appConfig.runtime)) {
        LOG_E("MQTT Service init FAILED!");
      } else {
        // TODO: Buraya istenen topic kayitlarini ekleyin.
        // Ornek:
        // g_mqttService->addTopicSubscription("funtoria/device/+/command", 1,
        //                                     onMqttMessage, nullptr);

        if (!g_mqttService->start()) {
          LOG_E("MQTT Task start FAILED!");
        } else {
          LOG_I("MQTT Service started (stack=%u, priority=%u, queue=%u, "
                "reconnect=%lu ms)",
                static_cast<unsigned>(g_appConfig.mqtt.task_stack),
                static_cast<unsigned>(g_appConfig.mqtt.task_priority),
                static_cast<unsigned>(g_appConfig.mqtt.queue_size),
                static_cast<unsigned long>(g_appConfig.mqtt.reconnect_delay_ms));
        }
      }
    }
  }

  // === LED Animasyon Servisi Başlat ===
  if (!g_appConfig.led.enabled) {
    LOG_W("LED service disabled from config");
  } else {
    if (!g_ledService.init(&g_appConfig.led, &g_appConfig.led_task)) {
      LOG_E("LED Service init FAILED!");
      // LED hatası kritik değil, devam et
    } else {
      if (!g_ledService.start()) {
        LOG_E("LED Task start FAILED!");
      } else {
        g_ledService.setMode(LED_ANIM_IDLE);
        LOG_I("LED Service started (count=%d, stack=%d, priority=%d, queue=%d)",
              g_appConfig.led.led_count, g_appConfig.led_task.task_stack,
              g_appConfig.led_task.task_priority, g_appConfig.led_task.queue_size);
      }
    }
  }

  // === LCD Service Başlat ===
  if (!g_appConfig.lcd.enabled) {
    LOG_W("LCD disabled from config");
  } else {
    if (!g_lcdService.init(&g_appConfig.lcd, &g_appConfig.lcd_task)) {
      LOG_E("LCD Service init FAILED!");
      // LCD hatası kritik değil, devam et
    } else {
      if (!g_lcdService.start()) {
        LOG_E("LCD Task start FAILED!");
      } else {
        LOG_I("LCD Service started (stack=%d, priority=%d, queue=%d)",
              g_appConfig.lcd_task.task_stack,
              g_appConfig.lcd_task.task_priority,
              g_appConfig.lcd_task.queue_size);
        if (SERVICE_ENABLE_LCD_SCREEN_DEMO) {
          LOG_W("LCD demo mode AKTIF: ekranlar otomatik siralanacak");
        }
        if (g_setupProvisioningMode) {
          g_lcdService.showSetupRequired(g_appConfig.runtime.device.device_id,
                                         g_appConfig.runtime.device.mac_hex);
        }
      }
    }
  }

  if (!g_appConfig.system.nfc_enabled) {
    LOG_W("NFC service disabled from service switches");
  } else {
    if (!g_pn532Hw) {
      g_pn532Hw =
          new (std::nothrow) Adafruit_PN532(g_appConfig.nfc.reset_pin, &Serial2);
      if (!g_pn532Hw) {
        LOG_E("PN532 object allocation FAILED!");
        while (true) {
          delay(1000);
        }
      }
    }

    // Mifare config başlat (Funtoria anahtarı ile)
    MifareHandler::initConfig(&g_mifareConfig, g_appConfig.mifare.key_funtoria);
    LOG_D("Mifare config initialized");

    // UART (HSU) başlat - Serial2 pin konfigürasyonu
    Serial2.begin(g_appConfig.uart.baud_rate, SERIAL_8N1, g_appConfig.uart.rx_pin,
                  g_appConfig.uart.tx_pin);
    LOG_D("UART (HSU) initialized: RX=%d, TX=%d, Baud=%lu",
          g_appConfig.uart.rx_pin, g_appConfig.uart.tx_pin,
          g_appConfig.uart.baud_rate);

    // PN532 HSU bağlantı kontrolü
    g_pn532Hw->begin();
    uint32_t versiondata = g_pn532Hw->getFirmwareVersion();
    if (!versiondata) {
      if (g_appConfig.led.enabled) {
        g_ledService.setMode(LED_ANIM_ERROR);
      }
      LOG_E("========================================");
      LOG_E("PN532 BULUNAMADI! HSU baglanti hatasi!");
      LOG_E("========================================");
      LOG_E("Kontrol edin:");
      LOG_E("  1. PN532 DIP Switch: SEL0=OFF, SEL1=ON (HSU modu)");
      LOG_E("  2. TX/RX kablolama: ESP32 TX(GPIO%d)->PN532 RX, ESP32 "
            "RX(GPIO%d)<-PN532 TX",
            g_appConfig.uart.tx_pin, g_appConfig.uart.rx_pin);
      LOG_E("  3. VCC ve GND baglantilari");
      LOG_E("========================================");
      while (true) {
        delay(1000);
      }
    }
    LOG_I("PN532 HSU OK! FW: %lu.%lu.%lu.%lu", (versiondata >> 24) & 0xFF,
          (versiondata >> 16) & 0xFF, (versiondata >> 8) & 0xFF,
          versiondata & 0xFF);

    // NFC Service başlat
    NfcResult res = g_nfcService.init(g_pn532Hw, &g_apduBuffer);
    if (res != NFC_SUCCESS) {
      if (g_appConfig.led.enabled) {
        g_ledService.setMode(LED_ANIM_ERROR);
      }
      LOG_E("NFC Service init FAILED: %s", NfcResult_toString(res));
      LOG_E("Sistem baslatılamadi!");

      // Hata durumunda LED yak veya buzzer çal (ileride)
      while (true) {
        delay(1000); // Sonsuz döngü - manual reset gerekli
      }
    }

    // Callbacks ayarla
    g_nfcService.setCallback(onCardRead);                   // Fiziksel kart
    g_nfcService.setMobilePaymentCallback(onMobilePayment); // Mobil ödeme (HCE)

    // Setup mode'da mobil ödeme kapalı
    g_nfcService.setMobilePaymentEnabled(!g_setupProvisioningMode);
    LOG_I("Mobile payment (HCE): %s",
          g_setupProvisioningMode ? "DISABLED (setup mode)" : "ENABLED");

    // Timeout ve gecikme ayarla
    g_nfcService.setTagDetectTimeout(g_appConfig.nfc.tag_detect_timeout_ms);
    g_nfcService.setLoopDelay(g_appConfig.system.nfc_loop_delay_ms);

    // NFC debounce ayarlarını SPIFFS'ten yükle
    NfcDebounceState debounceSettings;
    NfcDebounceState_init(&debounceSettings);

    if (spiffsReady) {
      if (SpiffsConfig::loadNfcSettings(&debounceSettings)) {
        g_nfcService.setDebounceState(&debounceSettings);
        LOG_I("NFC debounce settings loaded from SPIFFS");
      } else {
        // Dosya yoksa varsayılanları kaydet
        if (!SpiffsConfig::nfcSettingsExist()) {
          SpiffsConfig::createDefaultNfcSettings();
          LOG_I("NFC default settings created in SPIFFS");
        }
      }
    } else {
      LOG_W("NFC SPIFFS settings skipped (SPIFFS not ready)");
    }

    LOG_I("Debounce: %s, Cooldown: %lu ms",
          g_nfcService.isDebounceEnabled() ? "ON" : "OFF",
          g_nfcService.getCooldownMs());

    // NFC Task başlat
    res = g_nfcService.start(g_appConfig.system.nfc_task_stack,
                             g_appConfig.system.nfc_task_priority);

    if (res != NFC_SUCCESS) {
      if (g_appConfig.led.enabled) {
        g_ledService.setMode(LED_ANIM_ERROR);
      }
      LOG_E("NFC Task start FAILED: %s", NfcResult_toString(res));
      while (true) {
        delay(1000);
      }
    }
  }

  LOG_I("========================================");
  LOG_I("Setup tamamlandi!");
  LOG_I("Provisioning mode: %s", g_setupProvisioningMode ? "AKTIF" : "KAPALI");
  LOG_I("NFC servis: %s", g_appConfig.system.nfc_enabled ? "AKTIF" : "KAPALI");
  LOG_I("LCD servis: %s", g_appConfig.lcd.enabled ? "AKTIF" : "KAPALI");
  LOG_I("LED servis: %s", g_appConfig.led.enabled ? "AKTIF" : "KAPALI");
  LOG_I("WEB servis: %s",
        (!g_setupProvisioningMode && g_appConfig.web_api.enabled) ? "AKTIF"
                                                                   : "KAPALI");
  LOG_I("MQTT servis: %s",
        (!g_setupProvisioningMode && g_appConfig.mqtt.enabled) ? "AKTIF"
                                                                : "KAPALI");
  LOG_I("========================================");
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
  // FreeRTOS task'ları çalışıyor
  // Ana loop kullanılmıyor - CPU tasarrufu için task sil

  // NOT: loop() tamamen silinirse Arduino framework hata verir
  // Bu yüzden vTaskDelete ile kendini silen task yapıyoruz

  LOG_D("Main loop deleting itself (FreeRTOS tasks active)");
  vTaskDelete(nullptr);
}

// =============================================================================
// BELLEK KULLANIMI (Tahmini)
// =============================================================================
/*
 * Global Objeler:
 * - g_pn532Hw pointer:              ~4 bytes (NFC aciksa heap'te olusur)
 * - g_apduBuffer:                   ~262 bytes
 * - g_nfcService:                   ~100 bytes (+ Mutex ~40, Task ~8)
 * - g_appConfig:                    ~50 bytes
 * - g_mifareConfig:                 ~20 bytes
 *
 * Task Stack:
 * - NfcTask:                        4096 bytes
 *
 * TOPLAM: ~5 KB (heap fragmentation YOK)
 *
 * Karşılaştırma (Eski mimari): ~10 KB + heap fragmentation
 */
