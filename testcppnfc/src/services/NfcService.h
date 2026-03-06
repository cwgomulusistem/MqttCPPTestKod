/**
 * @file NfcService.h
 * @brief NFC Servis Katmanı (Hibrit C++)
 * @version 2.0.0 - Hibrit Mimari
 * 
 * C-Style PN532DriverState + C++ Mutex RAII birleşimi.
 * ScopedLock ile deadlock riski SIFIR.
 * Unlock Before Callback pattern ile uzun callback'ler güvenli.
 */

#ifndef NFC_SERVICE_H
#define NFC_SERVICE_H

#include "../drivers/PN532Driver.h"
#include "../drivers/NfcTypes.h"
#include "../logic/MifareCardHandler.h"
#include "../system/OsWrappers.h"
#include "../config/Config.h"

// Callback tipleri (C-style function pointer - std::function yerine)
typedef void (*OnCardReadCallback)(const NfcCardData* data);
typedef void (*OnMobilePaymentCallback)(const MobilePaymentData* data);

/**
 * @class NfcService
 * @brief NFC Servis Sınıfı (Hibrit Mimari)
 * 
 * İçinde barındırır:
 * - PN532DriverState (C struct) - veri
 * - NfcDebounceState (C struct) - debounce durumu
 * - Mutex (C++ RAII) - thread safety
 * - Task (C++ RAII) - FreeRTOS task
 * 
 * Mutex::ScopedLock ile:
 * - Deadlock İMKANSIZ
 * - Exception-safe
 * - Otomatik unlock
 * 
 * Debounce/Cooldown:
 * - Kart tutuluyorken tekrar okumaz
 * - Kart çekildikten sonra cooldown süresi bekler
 */
class NfcService {
private:
    // C-Style state (Composition)
    PN532DriverState _driverState;
    
    // Debounce durumu
    NfcDebounceState _debounce;
    
    // C++ RAII objeler - otomatik cleanup
    Mutex _driverLock;
    Task _task;
    
    // Callbacks (C-style function pointer)
    OnCardReadCallback _callback;
    OnMobilePaymentCallback _mobileCallback;
    
    // Yapılandırma
    uint16_t _tagDetectTimeout;
    uint16_t _loopDelayMs;
    bool _running;
    bool _mobilePaymentEnabled;
    MifareHandler::SetupCardData* _lastSetupCardData;
    bool _hasLastSetupCardData;
    
    // Task loop (static - FreeRTOS uyumluluğu için)
    static void taskLoop(void* param);
    
    // İç işlem döngüsü
    void processLoop();

public:
    /**
     * @brief Constructor
     */
    NfcService();
    
    /**
     * @brief Destructor - Otomatik cleanup
     */
    ~NfcService();
    
    // Copy/Move engelle
    NfcService(const NfcService&) = delete;
    NfcService& operator=(const NfcService&) = delete;
    
    /**
     * @brief Servisi başlat
     * @param hw Adafruit_PN532 hardware pointer
     * @param buffer Global APDU buffer pointer
     * @return NFC_SUCCESS veya hata kodu
     */
    NfcResult init(Adafruit_PN532* hw, ApduBuffer* buffer);
    
    /**
     * @brief Callback ayarla
     * @param cb Kart okunduğunda çağrılacak fonksiyon
     */
    void setCallback(OnCardReadCallback cb);
    
    /**
     * @brief Tag algılama timeout ayarla
     * @param timeout_ms Timeout (milisaniye)
     */
    void setTagDetectTimeout(uint16_t timeout_ms);
    
    /**
     * @brief Task döngü gecikmesi ayarla
     * @param delay_ms Gecikme (milisaniye)
     */
    void setLoopDelay(uint16_t delay_ms);
    
    /**
     * @brief NFC task'ı başlat
     * @param stackSize Stack boyutu (byte)
     * @param priority Task önceliği
     * @return NFC_SUCCESS veya hata kodu
     */
    NfcResult start(uint16_t stackSize = 4096, uint8_t priority = 5);
    
    /**
     * @brief NFC task'ı durdur
     */
    void stop();
    
    /**
     * @brief Servis çalışıyor mu?
     */
    bool isRunning() const;
    
    /**
     * @brief Thread-safe tag okuma (manuel kullanım için)
     * @param timeout_ms Timeout (milisaniye)
     * @return NFC_SUCCESS tag bulunursa
     */
    NfcResult readTagSafe(uint16_t timeout_ms = 150);
    
    /**
     * @brief Thread-safe mevcut tag bilgisini al
     * @param info Hedef NfcTagInfo pointer
     * @return NFC_SUCCESS veya hata kodu
     */
    NfcResult getCurrentTagSafe(NfcTagInfo* info);
    
    /**
     * @brief Thread-safe Mifare bloğu oku
     * @param block Blok numarası
     * @param key 6-byte anahtar
     * @param buffer 16-byte hedef buffer
     * @return NFC_SUCCESS veya hata kodu
     */
    NfcResult readMifareBlockSafe(uint8_t block, 
                                   const uint8_t* key, 
                                   uint8_t* buffer);
    
    /**
     * @brief Thread-safe Mifare bloğu yaz
     * @param block Blok numarası
     * @param key 6-byte anahtar
     * @param data 16-byte kaynak veri
     * @return NFC_SUCCESS veya hata kodu
     */
    NfcResult writeMifareBlockSafe(uint8_t block, 
                                    const uint8_t* key, 
                                    const uint8_t* data);
    
    /**
     * @brief Task stack durumunu al (debug için)
     * @return Kalan stack (words)
     */
    UBaseType_t getStackHighWaterMark() const;
    
    // === DEBOUNCE/COOLDOWN API ===
    
    /**
     * @brief Cooldown süresini ayarla (runtime)
     * @param ms Cooldown süresi (milisaniye)
     */
    void setCooldownMs(uint32_t ms);
    
    /**
     * @brief Mevcut cooldown süresini al
     * @return Cooldown süresi (milisaniye)
     */
    uint32_t getCooldownMs() const;
    
    /**
     * @brief Debounce'u etkinleştir/devre dışı bırak
     * @param enabled true: aktif, false: devre dışı
     */
    void setDebounceEnabled(bool enabled);
    
    /**
     * @brief Debounce aktif mi?
     */
    bool isDebounceEnabled() const;
    
    /**
     * @brief Debounce state'ine erişim (SPIFFS load/save için)
     * @return NfcDebounceState pointer
     */
    NfcDebounceState* getDebounceState();
    
    /**
     * @brief Debounce state'ini dışarıdan yükle
     * @param state Kaynak NfcDebounceState
     */
    void setDebounceState(const NfcDebounceState* state);
    
    /**
     * @brief Son okunan kartı temizle (debounce reset)
     */
    void clearLastCard();

    /**
     * @brief Son okunan setup kart verisini al
     * @param outData Hedef setup veri pointer
     * @return true setup verisi mevcut ve kopyalandiysa
     */
    bool getLastSetupCardData(MifareHandler::SetupCardData* outData);
    
    // === MOBİL ÖDEME API ===
    
    /**
     * @brief Mobil ödeme callback ayarla
     * @param cb Mobil ödeme algılandığında çağrılacak fonksiyon
     */
    void setMobilePaymentCallback(OnMobilePaymentCallback cb);
    
    /**
     * @brief Mobil ödeme işlemeyi etkinleştir/devre dışı bırak
     * @param enabled true: ISO14443-4 algılandığında HCE akışı dene
     */
    void setMobilePaymentEnabled(bool enabled);
    
    /**
     * @brief Mobil ödeme aktif mi?
     */
    bool isMobilePaymentEnabled() const;
    
    /**
     * @brief Thread-safe mobil ödeme işle
     * @param data Hedef MobilePaymentData pointer
     * @return NFC_SUCCESS başarılıysa
     */
    NfcResult processMobilePaymentSafe(MobilePaymentData* data);
};

#endif // NFC_SERVICE_H
