/**
 * @file NfcTypes.h
 * @brief NFC Ortak Veri Tipleri (Pure C)
 * @version 2.0.0 - Hibrit Mimari
 * 
 * typedef struct ve enum ile bellek verimli veri yapıları.
 * Heap allocation yok, fixed-size buffers.
 */

#ifndef NFC_TYPES_H
#define NFC_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "../config/Config.h"

// Detaylı Hata Kodları (bool yerine)
typedef enum {
    NFC_SUCCESS = 0,
    NFC_ERR_NOT_INITIALIZED,
    NFC_ERR_TIMEOUT,
    NFC_ERR_COMMUNICATION,
    NFC_ERR_CRC,
    NFC_ERR_AUTH_FAILED,
    NFC_ERR_NO_TAG,
    NFC_ERR_BUFFER_OVERFLOW,
    NFC_ERR_INVALID_PARAM,
    NFC_ERR_NOT_SUPPORTED
} NfcResult;

// Tag Tipleri (Sadece Mifare 1K ve HCE destekleniyor)
typedef enum {
    TAG_NONE = 0,
    TAG_MIFARE_CLASSIC,
    TAG_ISO14443_4
} NfcTagType;

// NFC Okuyucu Durum Makinesi
typedef enum {
    NFC_STATE_IDLE = 0,
    NFC_STATE_DETECTING,
    NFC_STATE_TAG_PRESENT,
    NFC_STATE_AUTHENTICATING,
    NFC_STATE_READING,
    NFC_STATE_WRITING,
    NFC_STATE_ERROR
} NfcState;

// Kart Tipleri
typedef enum {
    CARD_TYPE_UNKNOWN = 0x00,
    CARD_TYPE_SETUP = 0x01,    // Setup Card
    CARD_TYPE_CUSTOMER = 0x02,   // Customer Card
    CARD_TYPE_GIFT = 0x03,       // Gift Card
    CARD_TYPE_SERVICE = 0x04,    // Service Card
    CARD_TYPE_MOBILE = 0xF0      // HCE/Mobile virtual card
} MifareCardType;

// Mobil Ödeme Durumu
typedef enum {
    MOBILE_STATUS_UNKNOWN = 0,          // Bilinmiyor
    MOBILE_STATUS_NOT_MOBILE,           // Mobil cihaz değil
    MOBILE_STATUS_APP_NOT_FOUND,        // Uygulama bulunamadı
    MOBILE_STATUS_APP_FOUND,            // Uygulama bulundu
    MOBILE_STATUS_TOKEN_RECEIVED,       // Token alındı
    MOBILE_STATUS_AUTH_REQUIRED,        // Kimlik doğrulama gerekli
    MOBILE_STATUS_INSUFFICIENT_BALANCE, // Yetersiz bakiye
    MOBILE_STATUS_ERROR                 // Genel hata
} MobilePaymentStatus;

// Mobil Platform Tipi
typedef enum {
    MOBILE_PLATFORM_UNKNOWN = 0,
    MOBILE_PLATFORM_ANDROID,
    MOBILE_PLATFORM_IOS
} MobilePlatformType;

// Etiket Bilgisi (Fixed-size - Heap yok)
typedef struct {
    uint8_t uid[MAX_UID_LENGTH];
    uint8_t uid_length;
    NfcTagType type;
    uint8_t sak;  // Select Acknowledge byte
} NfcTagInfo;

// APDU Buffer (Stack overflow önleme için global'de tutulacak)
typedef struct {
    uint8_t data[APDU_BUFFER_SIZE];
    uint16_t length;
} ApduBuffer;

// Mifare Blok Verisi
typedef struct {
    uint8_t data[MIFARE_BLOCK_SIZE];
    uint8_t block_number;
    bool valid;
} MifareBlockData;

// Kart Verisi (Callback'e gönderilir)
typedef struct {
    uint8_t uid[MAX_UID_LENGTH];
    uint8_t uid_length;
    char uid_string[MAX_UID_LENGTH * 2 + 1];  // Hex string + null
    NfcTagType tag_type;
    MifareCardType card_type;
    bool is_mobile_payment;
    bool mobile_app_not_ready;  // Telefon algılandı ama HCE aktif değil
} NfcCardData;

// Mobil Ödeme Verisi (HCE Response)
typedef struct {
    // Temel bilgiler
    MobilePaymentStatus status;
    MobilePlatformType platform;
    
    // Kullanıcı bilgileri (Mobil uygulamadan gelir)
    char user_id[MOBILE_USER_ID_MAX_LENGTH];
    uint8_t user_id_length;
    
    // Ödeme token'ı (Tek kullanımlık)
    char token[MOBILE_TOKEN_MAX_LENGTH];
    uint8_t token_length;
    
    // Bakiye bilgisi (opsiyonel)
    uint32_t balance;           // Kuruş cinsinden
    bool balance_available;
    
    // APDU durum kodu
    uint8_t sw1;
    uint8_t sw2;
    
    // UID bilgisi
    uint8_t uid[MAX_UID_LENGTH];
    uint8_t uid_length;
    char uid_string[MAX_UID_LENGTH * 2 + 1];
} MobilePaymentData;

// Helper: MobilePaymentData sıfırla
static inline void MobilePaymentData_init(MobilePaymentData* data) {
    memset(data, 0, sizeof(MobilePaymentData));
    data->status = MOBILE_STATUS_UNKNOWN;
    data->platform = MOBILE_PLATFORM_UNKNOWN;
}

// Helper: MobilePaymentStatus'u string'e çevir
static inline const char* MobilePaymentStatus_toString(MobilePaymentStatus status) {
    switch (status) {
        case MOBILE_STATUS_UNKNOWN:             return "UNKNOWN";
        case MOBILE_STATUS_NOT_MOBILE:          return "NOT_MOBILE";
        case MOBILE_STATUS_APP_NOT_FOUND:       return "APP_NOT_FOUND";
        case MOBILE_STATUS_APP_FOUND:           return "APP_FOUND";
        case MOBILE_STATUS_TOKEN_RECEIVED:      return "TOKEN_RECEIVED";
        case MOBILE_STATUS_AUTH_REQUIRED:       return "AUTH_REQUIRED";
        case MOBILE_STATUS_INSUFFICIENT_BALANCE: return "INSUFFICIENT_BALANCE";
        case MOBILE_STATUS_ERROR:               return "ERROR";
        default:                                return "INVALID";
    }
}

// Mifare İşlem Yapılandırması
typedef struct {
    uint8_t key_a[MIFARE_KEY_SIZE];
    uint8_t key_b[MIFARE_KEY_SIZE];
    uint8_t auth_block;
    uint8_t data_block;
    bool use_key_a;
} MifareOpConfig;

// NFC Debounce/Cooldown Durumu
typedef struct {
    uint8_t last_uid[MAX_UID_LENGTH];   // Son okunan UID
    uint8_t last_uid_len;                // Son UID uzunluğu
    uint32_t last_read_ms;               // Son okuma zamanı (ms)
    uint32_t cooldown_ms;                // Cooldown süresi (runtime değiştirilebilir)
    uint32_t card_removed_threshold_ms;  // Kart çekildi eşiği
    uint32_t last_seen_ms;               // Kartın en son görüldüğü zaman
    bool card_present;                   // Kart şu an yakın mı?
    bool debounce_enabled;               // Debounce aktif mi?
    bool last_was_mobile;                // Son okunan mobil cihaz miydi?
} NfcDebounceState;

// Helper: NfcDebounceState varsayılan değerleri yükle
static inline void NfcDebounceState_init(NfcDebounceState* state) {
    memset(state, 0, sizeof(NfcDebounceState));
    state->cooldown_ms = NFC_COOLDOWN_MS_DEFAULT;
    state->card_removed_threshold_ms = NFC_CARD_REMOVED_THRESHOLD;
    state->debounce_enabled = NFC_DEBOUNCE_ENABLED;
    state->card_present = false;
    state->last_was_mobile = false;
}

// Helper: İki UID aynı mı kontrol et
static inline bool NfcDebounce_isSameUid(const NfcDebounceState* state, 
                                          const uint8_t* uid, 
                                          uint8_t uid_len) {
    if (state->last_uid_len != uid_len) return false;
    if (uid_len == 0) return false;
    return (memcmp(state->last_uid, uid, uid_len) == 0);
}

// Helper: NfcResult'u string'e çevir
static inline const char* NfcResult_toString(NfcResult result) {
    switch (result) {
        case NFC_SUCCESS:           return "SUCCESS";
        case NFC_ERR_NOT_INITIALIZED: return "NOT_INITIALIZED";
        case NFC_ERR_TIMEOUT:       return "TIMEOUT";
        case NFC_ERR_COMMUNICATION: return "COMMUNICATION";
        case NFC_ERR_CRC:           return "CRC";
        case NFC_ERR_AUTH_FAILED:   return "AUTH_FAILED";
        case NFC_ERR_NO_TAG:        return "NO_TAG";
        case NFC_ERR_BUFFER_OVERFLOW: return "BUFFER_OVERFLOW";
        case NFC_ERR_INVALID_PARAM: return "INVALID_PARAM";
        case NFC_ERR_NOT_SUPPORTED: return "NOT_SUPPORTED";
        default:                    return "UNKNOWN";
    }
}

// Helper: NfcTagType'i string'e cevir
static inline const char* NfcTagType_toString(NfcTagType type) {
    switch (type) {
        case TAG_NONE:              return "NONE";
        case TAG_MIFARE_CLASSIC:    return "MIFARE_CLASSIC";
        case TAG_ISO14443_4:        return "ISO14443_4";
        default:                    return "UNSUPPORTED";
    }
}

#endif // NFC_TYPES_H
