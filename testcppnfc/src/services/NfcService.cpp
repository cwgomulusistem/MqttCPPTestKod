/**
 * @file NfcService.cpp
 * @brief NFC Servis Katmanı Implementasyonu (Hibrit C++)
 * @version 2.0.0 - Hibrit Mimari
 * 
 * UNLOCK BEFORE CALLBACK pattern uygulanmıştır:
 * - Veri yerel değişkene kopyalanır
 * - Mutex serbest bırakılır
 * - Callback çağrılır
 * - Bu sayede callback ne kadar uzun sürerse sürsün driver bloklanmaz
 */

#include "NfcService.h"
#include "../config/Logger.h"
#include "../logic/MifareCardHandler.h"
#include <new>
#include <string.h>

NfcService::NfcService()
    : _callback(nullptr)
    , _mobileCallback(nullptr)
    , _tagDetectTimeout(150)
    , _loopDelayMs(100)
    , _running(false)
    , _mobilePaymentEnabled(true)  // Varsayılan açık
    , _lastSetupCardData(nullptr)
    , _hasLastSetupCardData(false)
{
    memset(&_driverState, 0, sizeof(PN532DriverState));
    NfcDebounceState_init(&_debounce);
}

NfcService::~NfcService() {
    stop();  // Task RAII destructor'da da duracak ama explicit çağıralım
    if (_lastSetupCardData) {
        delete _lastSetupCardData;
        _lastSetupCardData = nullptr;
    }
}

NfcResult NfcService::init(Adafruit_PN532* hw, ApduBuffer* buffer) {
    LOG_FUNC_ENTER();
    
    if (!hw) {
        LOG_E("Invalid hardware pointer");
        return NFC_ERR_INVALID_PARAM;
    }
    
    // Driver'ı başlat (mutex gerekmez, henüz task yok)
    NfcResult res = PN532Driver::init(&_driverState, hw, buffer);
    
    if (res != NFC_SUCCESS) {
        LOG_E("Driver init failed: %s", NfcResult_toString(res));
    }
    
    LOG_FUNC_EXIT();
    return res;
}

void NfcService::setCallback(OnCardReadCallback cb) {
    _callback = cb;
}

void NfcService::setTagDetectTimeout(uint16_t timeout_ms) {
    _tagDetectTimeout = timeout_ms;
}

void NfcService::setLoopDelay(uint16_t delay_ms) {
    _loopDelayMs = delay_ms;
}

NfcResult NfcService::start(uint16_t stackSize, uint8_t priority) {
    LOG_FUNC_ENTER();
    
    if (!_driverState.initialized) {
        LOG_E("Driver not initialized");
        return NFC_ERR_NOT_INITIALIZED;
    }
    
    if (_running) {
        LOG_W("Service already running");
        return NFC_SUCCESS;
    }
    
    // Task başlat
    bool success = _task.start(
        "NfcTask", 
        taskLoop, 
        this, 
        stackSize, 
        priority,
        0  // Core 0 (protocol core)
    );
    
    if (!success) {
        LOG_E("Task start failed");
        return NFC_ERR_COMMUNICATION;
    }
    
    _running = true;
    LOG_I("NFC Service started");
    
    LOG_FUNC_EXIT();
    return NFC_SUCCESS;
}

void NfcService::stop() {
    if (!_running) return;
    
    _running = false;
    _task.stop();  // RAII - destructor da çağırır ama explicit yapalım
    
    LOG_I("NFC Service stopped");
}

bool NfcService::isRunning() const {
    return _running;
}

void NfcService::taskLoop(void* param) {
    NfcService* self = static_cast<NfcService*>(param);
    
    LOG_D("NFC Task started");
    
    while (self->_running) {
        self->processLoop();
        vTaskDelay(pdMS_TO_TICKS(self->_loopDelayMs));
    }
    
    LOG_D("NFC Task exiting");
    vTaskDelete(nullptr);  // Self-delete
}

/**
 * @brief Ana işlem döngüsü
 * 
 * UNLOCK BEFORE CALLBACK PATTERN:
 * 1. Mutex kilitle (ScopedLock)
 * 2. Tag oku, veriyi YEREL değişkene kopyala
 * 3. Scope bitir -> Mutex OTOMATİK açılır
 * 4. Callback'i kilit AÇIKKEN çağır
 * 
 * DEBOUNCE/COOLDOWN PATTERN:
 * - Kart tutuluyorken: Tekrar okuma (card_present = true)
 * - Kart çekilince: cooldown_ms kadar bekle
 * - Aynı kart cooldown içinde: Atla
 * - Farklı kart: Hemen oku
 * 
 * MOBİL ÖDEME AKIŞI:
 * - ISO14443-4 algılanırsa -> HCE akışı başlat
 * - SELECT AID -> GET TOKEN -> Callback
 */
void NfcService::processLoop() {
    NfcCardData cardData;
    MobilePaymentData mobileData;
    bool shouldCardCallback = false;
    bool shouldMobileCallback = false;
    bool isMobileDevice = false;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // === MUTEX KİLİTLİ BÖLGE BAŞLANGIÇ ===
    {
        Mutex::ScopedLock lock(_driverLock, pdMS_TO_TICKS(100));
        
        if (!lock.isLocked()) {
            LOG_W("Mutex lock timeout in processLoop");
            return;
        }
        
        // Tag algıla
        NfcResult res = PN532Driver::waitForTag(&_driverState, _tagDetectTimeout);
        
        if (res == NFC_SUCCESS) {
            // Kart algılandı
            _debounce.last_seen_ms = now;
            
            // Desteklenmeyen tag tipini reddet (sadece Mifare 1K ve HCE)
            if (_driverState.current_tag.type == TAG_NONE) {
                LOG_W("Desteklenmeyen tag tipi, SAK=0x%02X", _driverState.current_tag.sak);
                PN532Driver::finishCommunication(&_driverState);
                return;
            }
            
            // Tag tipini belirle (debounce'dan once lazim)
            bool canSendApdu = (_driverState.current_tag.type == TAG_ISO14443_4);
            bool isGenericMobileUid = (_driverState.current_tag.uid_length == 4 &&
                _driverState.current_tag.uid[0] == 0x01 && 
                _driverState.current_tag.uid[1] == 0x02 &&
                _driverState.current_tag.uid[2] == 0x03 && 
                _driverState.current_tag.uid[3] == 0x04);
            bool currentIsMobile = canSendApdu || isGenericMobileUid;
            
            // Debounce aktif mi kontrol et
            if (_debounce.debounce_enabled) {
                if (currentIsMobile) {
                    // MOBIL DEBOUNCE: UID her seferinde degisir,
                    // "mobil cihaz hala burada mi" bazli kontrol yap
                    if (_debounce.last_was_mobile && _debounce.card_present) {
                        return;
                    }
                    if (_debounce.last_was_mobile && !_debounce.card_present) {
                        uint32_t elapsed = now - _debounce.last_read_ms;
                        if (elapsed < _debounce.cooldown_ms) {
                            LOG_D("Mobile cooldown: %lu/%lu ms", elapsed, _debounce.cooldown_ms);
                            _debounce.card_present = true;
                            return;
                        }
                    }
                } else {
                    // FIZIKSEL KART DEBOUNCE: UID bazli
                    bool sameCard = NfcDebounce_isSameUid(
                        &_debounce,
                        _driverState.current_tag.uid,
                        _driverState.current_tag.uid_length
                    );
                    
                    if (sameCard && _debounce.card_present) {
                        return;
                    }
                    
                    if (sameCard && !_debounce.card_present) {
                        uint32_t elapsed = now - _debounce.last_read_ms;
                        if (elapsed < _debounce.cooldown_ms) {
                            LOG_D("Cooldown active: %lu/%lu ms", elapsed, _debounce.cooldown_ms);
                            _debounce.card_present = true;
                            return;
                        }
                    }
                }
            }
            
            // Kart verilerini kaydet
            _debounce.card_present = true;
            _debounce.last_was_mobile = currentIsMobile;
            memcpy(_debounce.last_uid, 
                   _driverState.current_tag.uid, 
                   _driverState.current_tag.uid_length);
            _debounce.last_uid_len = _driverState.current_tag.uid_length;
            _debounce.last_read_ms = now;
            
            // Mobil cihaz tespiti (debounce blogundan once belirlendi)
            isMobileDevice = currentIsMobile;
            
            LOG_I("Tag detected: type=%s, mobile=%s, canAPDU=%s", 
                  NfcTagType_toString(_driverState.current_tag.type),
                  isMobileDevice ? "Yes" : "No",
                  canSendApdu ? "Yes" : "No");
            
            // === MOBİL ÖDEME AKIŞI ===
            // NOT: APDU komutları SADECE ISO14443-4 tipinde çalışır!
            // Mifare Classic'e APDU göndermek hata verir.
            if (isMobileDevice && _mobilePaymentEnabled && _mobileCallback) {
                
                // SENARYO A: Gerçek HCE (ISO14443-4) - APDU gönderilebilir
                if (canSendApdu) {
                    LOG_I("=== MOBİL ÖDEME AKIŞI (ISO14443-4) ===");
                    
                    // HCE akışını işle
                    res = PN532Driver::processMobilePayment(&_driverState, &mobileData);
                    
                    // APDU islemi bittikten sonra zamani guncelle
                    // (HCE ~500-1000ms suruyor, bu surede last_seen_ms eskimis olabilir)
                    now = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    _debounce.last_seen_ms = now;
                    _debounce.last_read_ms = now;
                    
                    if (res == NFC_SUCCESS && 
                        (mobileData.token_length > 0 || mobileData.user_id_length > 0)) {
                        shouldMobileCallback = true;
                        LOG_I("Mobile payment processed successfully");
                    } else {
                        // APDU basarisiz -- ama cihaz hala mobil.
                        // "Fiziksel kart"a dusurme, mobil hata olarak bildir.
                        LOG_W("APDU exchange failed - mobil iletisim hatasi");
                        
                        memset(&cardData, 0, sizeof(NfcCardData));
                        memcpy(cardData.uid, 
                               _driverState.current_tag.uid, 
                               _driverState.current_tag.uid_length);
                        cardData.uid_length = _driverState.current_tag.uid_length;
                        cardData.tag_type = _driverState.current_tag.type;
                        cardData.is_mobile_payment = true;
                        cardData.mobile_app_not_ready = true;
                        cardData.card_type = CARD_TYPE_MOBILE;
                        
                        PN532Driver::getUidString(&_driverState, 
                                                   cardData.uid_string, 
                                                   sizeof(cardData.uid_string));
                        
                        shouldCardCallback = true;
                    }
                }
                // SENARYO B: Telefon HCE kapalıyken (Mifare görünümlü)
                else if (isGenericMobileUid) {
                    LOG_W("========================================");
                    LOG_W("TELEFON ALGILANDI - HCE AKTIF DEGIL!");
                    LOG_W("Lutfen telefonda Funtoria uygulamasini acin.");
                    LOG_W("========================================");
                    
                    // APDU gönderme! Sadece kart verisi hazırla
                    memset(&cardData, 0, sizeof(NfcCardData));
                    memcpy(cardData.uid, 
                           _driverState.current_tag.uid, 
                           _driverState.current_tag.uid_length);
                    cardData.uid_length = _driverState.current_tag.uid_length;
                    cardData.tag_type = _driverState.current_tag.type;
                    cardData.is_mobile_payment = true;
                    cardData.mobile_app_not_ready = true;  // Yeni flag!
                    cardData.card_type = CARD_TYPE_MOBILE;
                    
                    PN532Driver::getUidString(&_driverState, 
                                               cardData.uid_string, 
                                               sizeof(cardData.uid_string));
                    
                    shouldCardCallback = true;
                }
            }
            
            // === NORMAL KART VERİSİ HAZIRLA ===
            if (!shouldMobileCallback && !shouldCardCallback) {
                memset(&cardData, 0, sizeof(NfcCardData));
                
                memcpy(cardData.uid, 
                       _driverState.current_tag.uid, 
                       _driverState.current_tag.uid_length);
                
                cardData.uid_length = _driverState.current_tag.uid_length;
                cardData.tag_type = _driverState.current_tag.type;
                cardData.is_mobile_payment = false;
                cardData.mobile_app_not_ready = false;
                cardData.card_type = CARD_TYPE_UNKNOWN;

                if (cardData.tag_type == TAG_MIFARE_CLASSIC) {
                    MifareOpConfig mifareConfig = {};
                    MifareCardType parsedType = CARD_TYPE_UNKNOWN;
                    MifareHandler::initConfig(&mifareConfig, nullptr);

                    NfcResult mifareRes = MifareHandler::process(
                        &_driverState,
                        &_driverState.current_tag,
                        &mifareConfig,
                        &parsedType
                    );

                    if (mifareRes == NFC_SUCCESS) {
                        cardData.card_type = parsedType;

                        if (parsedType == CARD_TYPE_SETUP) {
                            MifareHandler::SetupCardData setupData = {};
                            NfcResult setupRes =
                                MifareHandler::readSetupCardData(&_driverState, &setupData);
                            if (setupRes == NFC_SUCCESS) {
                                if (!_lastSetupCardData) {
                                    _lastSetupCardData =
                                        new (std::nothrow) MifareHandler::SetupCardData();
                                }
                                if (_lastSetupCardData) {
                                    *_lastSetupCardData = setupData;
                                    _hasLastSetupCardData = true;
                                } else {
                                    _hasLastSetupCardData = false;
                                    LOG_W("Setup card cache alloc failed");
                                }
                            } else {
                                _hasLastSetupCardData = false;
                                LOG_W("Setup card data read failed: %s",
                                      NfcResult_toString(setupRes));
                            }
                        } else {
                            _hasLastSetupCardData = false;
                        }
                    } else {
                        _hasLastSetupCardData = false;
                        LOG_W("MIFARE type parse failed: %s",
                              NfcResult_toString(mifareRes));
                    }
                } else {
                    _hasLastSetupCardData = false;
                }
                
                PN532Driver::getUidString(&_driverState, 
                                           cardData.uid_string, 
                                           sizeof(cardData.uid_string));
                
                shouldCardCallback = true;
            }
            
            // İletişimi sonlandır
            PN532Driver::finishCommunication(&_driverState);
            
        } else {
            // Tag yok - kart çekildi mi kontrol et
            if (_debounce.card_present) {
                uint32_t elapsed = now - _debounce.last_seen_ms;
                if (elapsed > _debounce.card_removed_threshold_ms) {
                    _debounce.card_present = false;
                    LOG_D("Card removed");
                }
            }
        }
        
    }  // === MUTEX OTOMATİK AÇILIR ===
    
    // === CALLBACKS KİLİT AÇIKKEN ÇAĞRILIR ===
    if (shouldMobileCallback && _mobileCallback) {
        LOG_I("Calling mobile payment callback");
        _mobileCallback(&mobileData);
    } else if (shouldCardCallback && _callback) {
        _callback(&cardData);
    }
}

NfcResult NfcService::readTagSafe(uint16_t timeout_ms) {
    Mutex::ScopedLock lock(_driverLock, pdMS_TO_TICKS(timeout_ms + 100));
    
    if (!lock.isLocked()) {
        return NFC_ERR_TIMEOUT;
    }
    
    return PN532Driver::waitForTag(&_driverState, timeout_ms);
}

NfcResult NfcService::getCurrentTagSafe(NfcTagInfo* info) {
    if (!info) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    Mutex::ScopedLock lock(_driverLock, pdMS_TO_TICKS(100));
    
    if (!lock.isLocked()) {
        return NFC_ERR_TIMEOUT;
    }
    
    memcpy(info, &_driverState.current_tag, sizeof(NfcTagInfo));
    return NFC_SUCCESS;
}

NfcResult NfcService::readMifareBlockSafe(uint8_t block, 
                                           const uint8_t* key, 
                                           uint8_t* buffer) {
    if (!key || !buffer) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    Mutex::ScopedLock lock(_driverLock, pdMS_TO_TICKS(500));
    
    if (!lock.isLocked()) {
        return NFC_ERR_TIMEOUT;
    }
    
    // Önce doğrula
    NfcResult res = PN532Driver::authenticateMifare(&_driverState, block, key, true);
    if (res != NFC_SUCCESS) {
        return res;
    }
    
    // Sonra oku
    return PN532Driver::readMifareBlock(&_driverState, block, buffer);
}

NfcResult NfcService::writeMifareBlockSafe(uint8_t block, 
                                            const uint8_t* key, 
                                            const uint8_t* data) {
    if (!key || !data) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    Mutex::ScopedLock lock(_driverLock, pdMS_TO_TICKS(500));
    
    if (!lock.isLocked()) {
        return NFC_ERR_TIMEOUT;
    }
    
    // Önce doğrula
    NfcResult res = PN532Driver::authenticateMifare(&_driverState, block, key, true);
    if (res != NFC_SUCCESS) {
        return res;
    }
    
    // Sonra yaz
    return PN532Driver::writeMifareBlock(&_driverState, block, data);
}

UBaseType_t NfcService::getStackHighWaterMark() const {
    return _task.getStackHighWaterMark();
}

// === DEBOUNCE/COOLDOWN API ===

void NfcService::setCooldownMs(uint32_t ms) {
    // Sınır kontrolleri
    if (ms < NFC_COOLDOWN_MS_MIN) ms = NFC_COOLDOWN_MS_MIN;
    if (ms > NFC_COOLDOWN_MS_MAX) ms = NFC_COOLDOWN_MS_MAX;
    
    _debounce.cooldown_ms = ms;
    LOG_I("Cooldown set to %lu ms", ms);
}

uint32_t NfcService::getCooldownMs() const {
    return _debounce.cooldown_ms;
}

void NfcService::setDebounceEnabled(bool enabled) {
    _debounce.debounce_enabled = enabled;
    LOG_I("Debounce %s", enabled ? "enabled" : "disabled");
}

bool NfcService::isDebounceEnabled() const {
    return _debounce.debounce_enabled;
}

NfcDebounceState* NfcService::getDebounceState() {
    return &_debounce;
}

void NfcService::setDebounceState(const NfcDebounceState* state) {
    if (!state) return;
    
    // Sadece ayarları kopyala, runtime state'i koruma
    _debounce.cooldown_ms = state->cooldown_ms;
    _debounce.debounce_enabled = state->debounce_enabled;
    _debounce.card_removed_threshold_ms = state->card_removed_threshold_ms;
    
    // Sınır kontrolleri
    if (_debounce.cooldown_ms < NFC_COOLDOWN_MS_MIN) {
        _debounce.cooldown_ms = NFC_COOLDOWN_MS_MIN;
    }
    if (_debounce.cooldown_ms > NFC_COOLDOWN_MS_MAX) {
        _debounce.cooldown_ms = NFC_COOLDOWN_MS_MAX;
    }
}

void NfcService::clearLastCard() {
    memset(_debounce.last_uid, 0, sizeof(_debounce.last_uid));
    _debounce.last_uid_len = 0;
    _debounce.last_read_ms = 0;
    _debounce.card_present = false;
    _hasLastSetupCardData = false;
    LOG_D("Last card cleared");
}

bool NfcService::getLastSetupCardData(MifareHandler::SetupCardData* outData) {
    if (!outData) {
        return false;
    }

    Mutex::ScopedLock lock(_driverLock, pdMS_TO_TICKS(150));
    if (!lock.isLocked()) {
        return false;
    }

    if (!_hasLastSetupCardData) {
        return false;
    }

    if (!_lastSetupCardData) {
        return false;
    }

    *outData = *_lastSetupCardData;
    return true;
}

// === MOBİL ÖDEME API ===

void NfcService::setMobilePaymentCallback(OnMobilePaymentCallback cb) {
    _mobileCallback = cb;
    LOG_I("Mobile payment callback %s", cb ? "set" : "cleared");
}

void NfcService::setMobilePaymentEnabled(bool enabled) {
    _mobilePaymentEnabled = enabled;
    LOG_I("Mobile payment %s", enabled ? "enabled" : "disabled");
}

bool NfcService::isMobilePaymentEnabled() const {
    return _mobilePaymentEnabled;
}

NfcResult NfcService::processMobilePaymentSafe(MobilePaymentData* data) {
    if (!data) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    Mutex::ScopedLock lock(_driverLock, pdMS_TO_TICKS(MOBILE_APDU_TIMEOUT_MS + 500));
    
    if (!lock.isLocked()) {
        return NFC_ERR_TIMEOUT;
    }
    
    return PN532Driver::processMobilePayment(&_driverState, data);
}
