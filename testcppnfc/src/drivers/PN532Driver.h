/**
 * @file PN532Driver.h
 * @brief PN532 NFC Modülü Driver (Pure C-Style)
 * @version 2.0.0 - Hibrit Mimari
 * 
 * typedef struct + namespace fonksiyonları.
 * Mutex YOK - Service katmanı yönetir.
 * Vtable overhead yok, her byte'ın yeri belli.
 */

#ifndef PN532_DRIVER_H
#define PN532_DRIVER_H

#include <Adafruit_PN532.h>
#include "NfcTypes.h"
#include "../config/Config.h"

// Driver State (Saf C struct - Mutex YOK)
typedef struct {
    Adafruit_PN532* hw;             // Hardware pointer
    NfcTagInfo current_tag;         // Mevcut tag bilgisi
    ApduBuffer* apdu_buffer;        // Global APDU buffer pointer
    NfcState state;                 // Durum makinesi
    uint32_t firmware_version;      // Firmware cache
    bool initialized;               // Init durumu
} PN532DriverState;

// Driver Namespace Fonksiyonları
namespace PN532Driver {

    /**
     * @brief Driver'ı başlat
     * @param state Driver state pointer
     * @param hw Adafruit_PN532 hardware pointer
     * @param buffer Global APDU buffer pointer
     * @return NFC_SUCCESS veya hata kodu
     */
    NfcResult init(PN532DriverState* state, Adafruit_PN532* hw, ApduBuffer* buffer);
    
    /**
     * @brief Tag bekle ve algıla
     * @param state Driver state pointer
     * @param timeout_ms Timeout (milisaniye)
     * @return NFC_SUCCESS tag bulunursa, NFC_ERR_NO_TAG veya NFC_ERR_TIMEOUT
     */
    NfcResult waitForTag(PN532DriverState* state, uint16_t timeout_ms);
    
    /**
     * @brief Tag hala mevcut mu kontrol et
     * @param state Driver state pointer
     * @return true tag mevcutsa
     */
    bool isTagPresent(PN532DriverState* state);
    
    /**
     * @brief UID'yi hex string'e çevir
     * @param state Driver state pointer
     * @param buffer Hedef buffer
     * @param size Buffer boyutu
     * @return NFC_SUCCESS veya hata kodu
     */
    NfcResult getUidString(PN532DriverState* state, char* buffer, uint8_t size);
    
    /**
     * @brief Mifare bloğunu doğrula
     * @param state Driver state pointer
     * @param block Blok numarası
     * @param key 6-byte anahtar
     * @param use_key_a true=KeyA, false=KeyB
     * @return NFC_SUCCESS veya NFC_ERR_AUTH_FAILED
     */
    NfcResult authenticateMifare(PN532DriverState* state, 
                                  uint8_t block, 
                                  const uint8_t* key, 
                                  bool use_key_a);
    
    /**
     * @brief Mifare bloğu oku
     * @param state Driver state pointer
     * @param block Blok numarası
     * @param buffer 16-byte hedef buffer
     * @return NFC_SUCCESS veya hata kodu
     */
    NfcResult readMifareBlock(PN532DriverState* state, 
                               uint8_t block, 
                               uint8_t* buffer);
    
    /**
     * @brief Mifare bloğu yaz
     * @param state Driver state pointer
     * @param block Blok numarası
     * @param data 16-byte kaynak veri
     * @return NFC_SUCCESS veya hata kodu
     */
    NfcResult writeMifareBlock(PN532DriverState* state, 
                                uint8_t block, 
                                const uint8_t* data);
    
    /**
     * @brief APDU komutu gönder (ISO14443-4)
     * @param state Driver state pointer
     * @param apdu APDU komut byte'ları
     * @param apdu_len APDU uzunluğu
     * @param response_len Yanıt uzunluğu (out)
     * @return NFC_SUCCESS veya hata kodu
     * @note Yanıt state->apdu_buffer'a yazılır
     */
    NfcResult sendApdu(PN532DriverState* state, 
                        const uint8_t* apdu, 
                        uint16_t apdu_len,
                        uint16_t* response_len);
    
    /**
     * @brief Son APDU yanıtını al
     * @param state Driver state pointer
     * @return Yanıt buffer pointer
     */
    const uint8_t* getApduResponse(PN532DriverState* state);
    
    /**
     * @brief Son APDU yanıt uzunluğunu al
     * @param state Driver state pointer
     * @return Yanıt uzunluğu
     */
    uint16_t getApduResponseLength(PN532DriverState* state);
    
    /**
     * @brief İletişimi sonlandır
     * @param state Driver state pointer
     */
    void finishCommunication(PN532DriverState* state);
    
    /**
     * @brief Driver'ı sıfırla
     * @param state Driver state pointer
     */
    void reset(PN532DriverState* state);
    
    /**
     * @brief Firmware versiyonunu string olarak al
     * @param state Driver state pointer
     * @param buffer Hedef buffer
     * @param size Buffer boyutu
     */
    void getFirmwareString(PN532DriverState* state, char* buffer, uint8_t size);
    
    // =========================================================================
    // MOBİL ÖDEME (HCE) FONKSİYONLARI
    // =========================================================================
    
    /**
     * @brief Mobil uygulama seç (SELECT AID)
     * @param state Driver state pointer
     * @param aid Application ID byte array
     * @param aid_len AID uzunluğu
     * @return NFC_SUCCESS uygulama bulunursa
     */
    NfcResult selectApplication(PN532DriverState* state, 
                                 const uint8_t* aid, 
                                 uint8_t aid_len);
    
    /**
     * @brief Funtoria uygulamasını seç
     * @param state Driver state pointer
     * @return NFC_SUCCESS uygulama bulunursa
     */
    NfcResult selectFuntoriaApp(PN532DriverState* state);
    
    /**
     * @brief Mobil ödeme token'ı al
     * @param state Driver state pointer
     * @param data Hedef MobilePaymentData pointer
     * @return NFC_SUCCESS token alınırsa
     */
    NfcResult getMobileToken(PN532DriverState* state, MobilePaymentData* data);
    
    /**
     * @brief Mobil kullanıcı ID'si al
     * @param state Driver state pointer
     * @param data Hedef MobilePaymentData pointer
     * @return NFC_SUCCESS ID alınırsa
     */
    NfcResult getMobileUserId(PN532DriverState* state, MobilePaymentData* data);
    
    /**
     * @brief Mobil cihaz bakiyesi al (varsa)
     * @param state Driver state pointer
     * @param data Hedef MobilePaymentData pointer
     * @return NFC_SUCCESS bakiye alınırsa
     */
    NfcResult getMobileBalance(PN532DriverState* state, MobilePaymentData* data);
    
    /**
     * @brief Tam mobil ödeme akışı (SELECT + GET_TOKEN + GET_USER)
     * @param state Driver state pointer
     * @param data Hedef MobilePaymentData pointer
     * @return NFC_SUCCESS tam akış başarılıysa
     */
    NfcResult processMobilePayment(PN532DriverState* state, MobilePaymentData* data);
    
    /**
     * @brief Son APDU durum kodlarını al
     * @param state Driver state pointer
     * @param sw1 SW1 pointer (out)
     * @param sw2 SW2 pointer (out)
     */
    void getLastApduStatus(PN532DriverState* state, uint8_t* sw1, uint8_t* sw2);

}  // namespace PN532Driver

#endif // PN532_DRIVER_H
