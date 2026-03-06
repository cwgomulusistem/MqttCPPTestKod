/**
 * @file MifareCardHandler.h
 * @brief Mifare Kart İşlemleri (C-Style)
 * @version 2.0.0 - Hibrit Mimari
 * 
 * Namespace fonksiyonları ile Mifare Classic işlemleri.
 * Interface/virtual function YOK.
 */

#ifndef MIFARE_CARD_HANDLER_H
#define MIFARE_CARD_HANDLER_H

#include "../drivers/PN532Driver.h"
#include "../drivers/NfcTypes.h"
#include "../config/Config.h"

namespace MifareHandler {

    // Setup card alanlari 3 blok = 48 byte saklanir.
    static constexpr uint8_t SETUP_FIELD_BLOCK_COUNT = 3;
    static constexpr uint8_t SETUP_FIELD_DATA_BYTES =
        SETUP_FIELD_BLOCK_COUNT * MIFARE_BLOCK_SIZE;

    typedef struct {
        MifareCardType card_type;
        char wifi_ssid[SETUP_FIELD_DATA_BYTES + 1];
        char wifi_password[SETUP_FIELD_DATA_BYTES + 1];
        char ip_address[SETUP_FIELD_DATA_BYTES + 1];
        char mfirm_id[SETUP_FIELD_DATA_BYTES + 1];
        char guid_part_1[SETUP_FIELD_DATA_BYTES + 1];
        char guid_part_2[SETUP_FIELD_DATA_BYTES + 1];
        char guid_full[(SETUP_FIELD_DATA_BYTES * 2) + 1];
        uint8_t lengths_raw[SETUP_FIELD_DATA_BYTES];
        uint8_t tenant_verify_block[MIFARE_BLOCK_SIZE];
        bool tenant_verify_valid;
    } SetupCardData;

    typedef struct {
        bool configured;
        uint8_t verify_block; // Varsayilan: 36
        uint8_t key_a[MIFARE_KEY_SIZE];
        uint8_t key_b[MIFARE_KEY_SIZE];
    } TenantVerifyConfig;

    /**
     * @brief MifareOpConfig'i varsayılan değerlerle başlat
     * @param config Hedef config pointer
     * @param key Varsayılan anahtar (NULL ise KEY_DEFAULT kullanılır)
     */
    void initConfig(MifareOpConfig* config, const uint8_t* key);
    
    /**
     * @brief Mifare kartı işle (basit doğrulama + tip okuma)
     * @param driver PN532 driver state
     * @param tag Tag bilgisi
     * @param config Mifare yapılandırması
     * @param out_card_type Okunan kart tipi (out)
     * @return NFC_SUCCESS veya hata kodu
     */
    NfcResult process(PN532DriverState* driver, 
                      const NfcTagInfo* tag,
                      MifareOpConfig* config,
                      MifareCardType* out_card_type);

    /**
     * @brief Setup karti blok haritasina gore coz
     * @param driver PN532 driver state
     * @param out_data Ciktilar
     * @return NFC_SUCCESS veya hata
     */
    NfcResult readSetupCardData(PN532DriverState* driver, SetupCardData* out_data);

    /**
     * @brief Kart tipi alanini (blok 4 byte0) yazar
     * @param driver PN532 driver state
     * @param card_type 1..4 kart tipi
     * @return NFC_SUCCESS veya hata
     */
    NfcResult writeCardType(PN532DriverState* driver, MifareCardType card_type);

    /**
     * @brief Setup kart data bloklarini yazar (wifi/ip/mfirm/guid/lengths)
     * @param driver PN532 driver state
     * @param data Yazilacak setup kart icerigi
     * @return NFC_SUCCESS veya hata
     */
    NfcResult writeSetupCardData(PN532DriverState* driver, const SetupCardData* data);

    /**
     * @brief Tenant verify (blok 36) anahtarlarini runtime set et
     *        TODO: Web endpoint hazir oldugunda bu API web servis tarafindan cagrilacak.
     */
    void setTenantVerifyConfig(const TenantVerifyConfig* config);

    /**
     * @brief Tenant verify konfigunu temizle (endpoint bilgisi yoksa)
     */
    void clearTenantVerifyConfig();
    
    /**
     * @brief Bloktan bakiye oku
     * @param driver PN532 driver state
     * @param block Blok numarası
     * @param key 6-byte anahtar
     * @param out_balance Okunan bakiye (out)
     * @return NFC_SUCCESS veya hata kodu
     */
    NfcResult readBalance(PN532DriverState* driver,
                          uint8_t block,
                          const uint8_t* key,
                          int32_t* out_balance);
    
    /**
     * @brief Bloğa bakiye yaz
     * @param driver PN532 driver state
     * @param block Blok numarası
     * @param key 6-byte anahtar
     * @param balance Yazılacak bakiye
     * @return NFC_SUCCESS veya hata kodu
     */
    NfcResult writeBalance(PN532DriverState* driver,
                           uint8_t block,
                           const uint8_t* key,
                           int32_t balance);
    
    /**
     * @brief Kart tipi byte'ını MifareCardType enum'a çevir
     * @param type_byte Okunan byte
     * @return MifareCardType enum değeri
     */
    MifareCardType parseCardType(uint8_t type_byte);
    
    /**
     * @brief MifareCardType'ı string'e çevir
     * @param type Kart tipi
     * @return String açıklama
     */
    const char* cardTypeToString(MifareCardType type);

}  // namespace MifareHandler

#endif // MIFARE_CARD_HANDLER_H
