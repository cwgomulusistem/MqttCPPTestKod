/**
 * @file SpiffsConfig.h
 * @brief SPIFFS Konfigürasyon Yönetimi
 * @version 2.0.0 - Hibrit Mimari
 * 
 * NFC ayarlarını SPIFFS'te saklar ve yükler.
 * JSON formatında basit key-value depolama.
 */

#ifndef SPIFFS_CONFIG_H
#define SPIFFS_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "../drivers/NfcTypes.h"
#include "../config/Config.h"
#include "../logic/MifareCardHandler.h"

namespace SpiffsConfig {

/**
 * @brief SPIFFS dosya sistemini başlat
 * @param formatOnFail Başarısız olursa formatla
 * @return true başarılı
 */
bool init(bool formatOnFail = SPIFFS_FORMAT_ON_FAIL);

/**
 * @brief SPIFFS monte edildi mi?
 */
bool isMounted();

/**
 * @brief NFC debounce ayarlarını SPIFFS'ten yükle
 * @param state Hedef NfcDebounceState pointer
 * @return true başarılı, false dosya yok veya hata
 */
bool loadNfcSettings(NfcDebounceState* state);

/**
 * @brief NFC debounce ayarlarını SPIFFS'e kaydet
 * @param state Kaynak NfcDebounceState pointer
 * @return true başarılı
 */
bool saveNfcSettings(const NfcDebounceState* state);

/**
 * @brief Varsayılan NFC ayarlarını SPIFFS'e yaz
 * @return true başarılı
 */
bool createDefaultNfcSettings();

/**
 * @brief NFC ayar dosyası var mı?
 */
bool nfcSettingsExist();

/**
 * @brief Setup kartindan okunan konfig verisini SPIFFS'e kaydet
 * @param data Setup kart verisi
 * @return true basarili
 */
bool saveSetupCardData(const MifareHandler::SetupCardData* data);

/**
 * @brief Setup kart konfig verisini SPIFFS'ten yukle
 * @param outData Hedef setup verisi
 * @return true setup tamamlanmis ve veri basarili yuklendi
 */
bool loadSetupCardData(MifareHandler::SetupCardData* outData);

/**
 * @brief Setup kart konfig dosyasi var mi?
 */
bool setupCardDataExist();

/**
 * @brief SPIFFS bilgilerini al (debug için)
 * @param totalBytes Toplam alan
 * @param usedBytes Kullanılan alan
 */
void getInfo(size_t* totalBytes, size_t* usedBytes);

}  // namespace SpiffsConfig

#endif // SPIFFS_CONFIG_H
