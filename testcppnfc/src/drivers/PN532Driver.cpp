/**
 * @file PN532Driver.cpp
 * @brief PN532 NFC Modülü Driver Implementasyonu (Pure C-Style)
 * @version 2.0.0 - Hibrit Mimari
 */

#include "PN532Driver.h"
#include "../config/Logger.h"
#include <string.h>
#include <stdio.h>

// Adafruit_PN532 kutuphanesindeki global packet buffer'a erisim.
// readPassiveTargetID() sonrasi SAK degeri [11]'de kalir.
extern byte pn532_packetbuffer[];

namespace PN532Driver {

// SAK degerinden tag tipini belirle (Sadece Mifare 1K ve HCE)
static NfcTagType determineTagType(uint8_t sak, uint8_t uid_len) {
    (void)uid_len;
    if (sak == 0x08) {
        return TAG_MIFARE_CLASSIC;  // Mifare Classic 1K
    }
    if (sak == 0x20) {
        return TAG_ISO14443_4;      // HCE / Mobil
    }
    return TAG_NONE;                // Desteklenmiyor
}

NfcResult init(PN532DriverState* state, Adafruit_PN532* hw, ApduBuffer* buffer) {
    LOG_FUNC_ENTER();
    
    if (!state || !hw) {
        LOG_E("Invalid parameters");
        return NFC_ERR_INVALID_PARAM;
    }
    
    // Zero-init (Çöp veri önleme)
    memset(state, 0, sizeof(PN532DriverState));
    state->hw = hw;
    state->apdu_buffer = buffer;
    state->state = NFC_STATE_IDLE;
    
    // PN532 başlat
    state->hw->begin();
    
    // Firmware versiyon kontrolü
    state->firmware_version = state->hw->getFirmwareVersion();
    if (!state->firmware_version) {
        LOG_E("PN532 not found!");
        state->state = NFC_STATE_ERROR;
        return NFC_ERR_COMMUNICATION;
    }
    
    // SAMConfig - Secure Access Module
    state->hw->SAMConfig();
    
    state->initialized = true;
    
    char fw_str[32];
    getFirmwareString(state, fw_str, sizeof(fw_str));
    LOG_I("PN532 init OK, FW: %s", fw_str);
    
    LOG_FUNC_EXIT();
    return NFC_SUCCESS;
}

NfcResult waitForTag(PN532DriverState* state, uint16_t timeout_ms) {
    if (!state || !state->initialized) {
        return NFC_ERR_NOT_INITIALIZED;
    }
    
    state->state = NFC_STATE_DETECTING;
    
    uint8_t uid[7] = {0};
    uint8_t uid_len = 0;
    
    // readPassiveTargetID - blocking call with timeout
    bool success = state->hw->readPassiveTargetID(
        PN532_MIFARE_ISO14443A, 
        uid, 
        &uid_len, 
        timeout_ms
    );
    
    if (!success || uid_len == 0) {
        state->current_tag.type = TAG_NONE;
        state->current_tag.uid_length = 0;
        state->state = NFC_STATE_IDLE;
        return NFC_ERR_NO_TAG;
    }
    
    // Tag bulundu - bilgileri kaydet
    memcpy(state->current_tag.uid, uid, uid_len);
    state->current_tag.uid_length = uid_len;
    
    // SAK (SEL_RES) degerini Adafruit kutuphanesinin packet buffer'indan oku.
    // readPassiveTargetID -> readDetectedPassiveTargetID icinde
    // pn532_packetbuffer[11] = SEL_RES (SAK) byte'i.
    uint8_t sak = pn532_packetbuffer[11];
    state->current_tag.sak = sak;
    
    // SAK degerine gore tag tipini belirle
    state->current_tag.type = determineTagType(sak, uid_len);
    
    LOG_D("SAK=0x%02X -> type=%s", sak, 
          NfcTagType_toString(state->current_tag.type));
    
    state->state = NFC_STATE_TAG_PRESENT;
    
    LOG_D("Tag found: UID len=%d, type=%s", 
          uid_len, NfcTagType_toString(state->current_tag.type));
    
    return NFC_SUCCESS;
}

bool isTagPresent(PN532DriverState* state) {
    if (!state || !state->initialized) {
        return false;
    }
    
    // Hızlı kontrol için kısa timeout ile tekrar oku
    uint8_t uid[7] = {0};
    uint8_t uid_len = 0;
    
    bool present = state->hw->readPassiveTargetID(
        PN532_MIFARE_ISO14443A, 
        uid, 
        &uid_len, 
        50  // 50ms kısa timeout
    );
    
    return present && uid_len > 0;
}

NfcResult getUidString(PN532DriverState* state, char* buffer, uint8_t size) {
    if (!state || !buffer) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    if (!state->initialized) {
        return NFC_ERR_NOT_INITIALIZED;
    }
    
    uint8_t required = state->current_tag.uid_length * 2 + 1;
    if (size < required) {
        return NFC_ERR_BUFFER_OVERFLOW;
    }
    
    // Hex string oluştur
    for (uint8_t i = 0; i < state->current_tag.uid_length; i++) {
        sprintf(buffer + (i * 2), "%02X", state->current_tag.uid[i]);
    }
    buffer[state->current_tag.uid_length * 2] = '\0';
    
    return NFC_SUCCESS;
}

NfcResult authenticateMifare(PN532DriverState* state, 
                              uint8_t block, 
                              const uint8_t* key, 
                              bool use_key_a) {
    if (!state || !key) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    if (!state->initialized) {
        return NFC_ERR_NOT_INITIALIZED;
    }
    
    if (state->current_tag.uid_length == 0) {
        return NFC_ERR_NO_TAG;
    }
    
    state->state = NFC_STATE_AUTHENTICATING;
    
    uint8_t key_type = use_key_a ? 
                       MIFARE_CMD_AUTH_A : 
                       MIFARE_CMD_AUTH_B;
    
    bool success = state->hw->mifareclassic_AuthenticateBlock(
        state->current_tag.uid,
        state->current_tag.uid_length,
        block,
        key_type,
        const_cast<uint8_t*>(key)  // Adafruit API non-const alıyor
    );
    
    if (!success) {
        LOG_W("Auth failed for block %d", block);
        state->state = NFC_STATE_ERROR;
        return NFC_ERR_AUTH_FAILED;
    }
    
    LOG_D("Auth OK for block %d", block);
    return NFC_SUCCESS;
}

NfcResult readMifareBlock(PN532DriverState* state, 
                           uint8_t block, 
                           uint8_t* buffer) {
    if (!state || !buffer) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    if (!state->initialized) {
        return NFC_ERR_NOT_INITIALIZED;
    }
    
    state->state = NFC_STATE_READING;
    
    bool success = state->hw->mifareclassic_ReadDataBlock(block, buffer);
    
    if (!success) {
        LOG_W("Read failed for block %d", block);
        state->state = NFC_STATE_ERROR;
        return NFC_ERR_COMMUNICATION;
    }
    
    LOG_D("Read OK for block %d", block);
    LOG_HEX("Block data", buffer, MIFARE_BLOCK_SIZE);
    
    return NFC_SUCCESS;
}

NfcResult writeMifareBlock(PN532DriverState* state, 
                            uint8_t block, 
                            const uint8_t* data) {
    if (!state || !data) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    if (!state->initialized) {
        return NFC_ERR_NOT_INITIALIZED;
    }
    
    // Trailer blok kontrolü (0, 4, 8, ... numaralı blokların her 4'üncüsü)
    if ((block + 1) % 4 == 0) {
        LOG_W("Writing to trailer block %d is dangerous!", block);
    }
    
    state->state = NFC_STATE_WRITING;
    
    bool success = state->hw->mifareclassic_WriteDataBlock(
        block, 
        const_cast<uint8_t*>(data)
    );
    
    if (!success) {
        LOG_W("Write failed for block %d", block);
        state->state = NFC_STATE_ERROR;
        return NFC_ERR_COMMUNICATION;
    }
    
    LOG_D("Write OK for block %d", block);
    
    return NFC_SUCCESS;
}

NfcResult sendApdu(PN532DriverState* state, 
                    const uint8_t* apdu, 
                    uint16_t apdu_len,
                    uint16_t* response_len) {
    if (!state || !apdu || !response_len) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    if (!state->initialized || !state->apdu_buffer) {
        return NFC_ERR_NOT_INITIALIZED;
    }
    
    if (apdu_len > APDU_BUFFER_SIZE) {
        return NFC_ERR_BUFFER_OVERFLOW;
    }
    
    LOG_D("Sending APDU, len=%d", apdu_len);
    LOG_HEX("APDU", apdu, apdu_len);
    
    // HCE iletisiminde RF coupling her zaman stabil olmayabilir.
    // Retry ile guvenilirlik artar.
    const uint8_t MAX_RETRIES = 3;
    const uint16_t RETRY_DELAY_MS = 50;
    bool success = false;
    uint8_t resp_len = 0;
    
    for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
        resp_len = 255;
        success = state->hw->inDataExchange(
            const_cast<uint8_t*>(apdu),
            apdu_len,
            state->apdu_buffer->data,
            &resp_len
        );
        
        if (success) break;
        
        if (attempt < MAX_RETRIES - 1) {
            LOG_D("APDU retry %d/%d", attempt + 1, MAX_RETRIES - 1);
            delay(RETRY_DELAY_MS);
        }
    }
    
    if (!success) {
        LOG_W("APDU exchange failed");
        state->apdu_buffer->length = 0;
        *response_len = 0;
        return NFC_ERR_COMMUNICATION;
    }
    
    state->apdu_buffer->length = resp_len;
    *response_len = resp_len;
    
    LOG_D("APDU response, len=%d", resp_len);
    LOG_HEX("Response", state->apdu_buffer->data, resp_len);
    
    return NFC_SUCCESS;
}

const uint8_t* getApduResponse(PN532DriverState* state) {
    if (!state || !state->apdu_buffer) {
        return nullptr;
    }
    return state->apdu_buffer->data;
}

uint16_t getApduResponseLength(PN532DriverState* state) {
    if (!state || !state->apdu_buffer) {
        return 0;
    }
    return state->apdu_buffer->length;
}

void finishCommunication(PN532DriverState* state) {
    if (!state || !state->initialized) {
        return;
    }
    
    // PICC durdurulamıyor Adafruit kütüphanesinde
    // Sadece state temizle
    memset(&state->current_tag, 0, sizeof(NfcTagInfo));
    state->state = NFC_STATE_IDLE;
    
    LOG_D("Communication finished");
}

void reset(PN532DriverState* state) {
    if (!state || !state->hw) {
        return;
    }
    
    LOG_I("Resetting PN532...");
    
    // Soft reset
    state->hw->SAMConfig();
    
    memset(&state->current_tag, 0, sizeof(NfcTagInfo));
    if (state->apdu_buffer) {
        memset(state->apdu_buffer, 0, sizeof(ApduBuffer));
    }
    state->state = NFC_STATE_IDLE;
}

void getFirmwareString(PN532DriverState* state, char* buffer, uint8_t size) {
    if (!state || !buffer || size < 16) {
        if (buffer && size > 0) buffer[0] = '\0';
        return;
    }
    
    uint32_t fw = state->firmware_version;
    
    // Format: IC.FW.REV.SUP
    snprintf(buffer, size, "%d.%d.%d.%d",
             (int)((fw >> 24) & 0xFF),  // IC
             (int)((fw >> 16) & 0xFF),  // Firmware major
             (int)((fw >> 8) & 0xFF),   // Firmware minor
             (int)(fw & 0xFF));         // Support
}

// =============================================================================
// MOBİL ÖDEME (HCE) FONKSİYONLARI
// =============================================================================

// Son APDU SW1/SW2 değerlerini saklamak için static
static uint8_t s_lastSw1 = 0;
static uint8_t s_lastSw2 = 0;

NfcResult selectApplication(PN532DriverState* state, 
                             const uint8_t* aid, 
                             uint8_t aid_len) {
    if (!state || !aid || aid_len == 0) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    if (!state->initialized || !state->apdu_buffer) {
        return NFC_ERR_NOT_INITIALIZED;
    }
    
    LOG_I("Selecting application, AID len=%d", aid_len);
    LOG_HEX("AID", aid, aid_len);
    
    // SELECT APDU oluştur: CLA INS P1 P2 Lc [AID] Le
    uint8_t select_apdu[32];
    uint8_t idx = 0;
    
    // Header kopyala
    memcpy(select_apdu, APDU_SELECT_HEADER, 4);
    idx = 4;
    
    // Lc (AID uzunluğu)
    select_apdu[idx++] = aid_len;
    
    // AID kopyala
    memcpy(select_apdu + idx, aid, aid_len);
    idx += aid_len;
    
    // Le (beklenen yanıt uzunluğu)
    select_apdu[idx++] = 0x00;
    
    // APDU gönder
    uint16_t resp_len = 0;
    NfcResult res = sendApdu(state, select_apdu, idx, &resp_len);
    
    if (res != NFC_SUCCESS) {
        s_lastSw1 = 0;
        s_lastSw2 = 0;
        return res;
    }
    
    // Yanıt kontrolü (min 2 byte: SW1 SW2)
    if (resp_len < 2) {
        LOG_W("SELECT response too short: %d bytes", resp_len);
        s_lastSw1 = 0;
        s_lastSw2 = 0;
        return NFC_ERR_COMMUNICATION;
    }
    
    // SW1 SW2 al
    s_lastSw1 = state->apdu_buffer->data[resp_len - 2];
    s_lastSw2 = state->apdu_buffer->data[resp_len - 1];
    
    LOG_D("SELECT response: SW1=0x%02X, SW2=0x%02X", s_lastSw1, s_lastSw2);
    
    // Başarı kontrolü
    if (APDU_IS_SUCCESS(s_lastSw1, s_lastSw2)) {
        LOG_I("Application selected successfully!");
        return NFC_SUCCESS;
    }
    
    // Uygulama bulunamadı
    if (s_lastSw1 == SW_NOT_FOUND_1 && s_lastSw2 == SW_NOT_FOUND_2) {
        LOG_W("Application not found");
        return NFC_ERR_NOT_SUPPORTED;  // Uygulama yok
    }
    
    LOG_W("SELECT failed: SW1=0x%02X, SW2=0x%02X", s_lastSw1, s_lastSw2);
    return NFC_ERR_COMMUNICATION;
}

NfcResult selectFuntoriaApp(PN532DriverState* state) {
    return selectApplication(state, FUNTORIA_AID, FUNTORIA_AID_LENGTH);
}

NfcResult getMobileToken(PN532DriverState* state, MobilePaymentData* data) {
    if (!state || !data) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    if (!state->initialized || !state->apdu_buffer) {
        return NFC_ERR_NOT_INITIALIZED;
    }
    
    LOG_D("Getting mobile token...");
    
    // GET TOKEN APDU gönder
    uint16_t resp_len = 0;
    NfcResult res = sendApdu(state, APDU_GET_TOKEN, sizeof(APDU_GET_TOKEN), &resp_len);
    
    if (res != NFC_SUCCESS) {
        data->status = MOBILE_STATUS_ERROR;
        return res;
    }
    
    if (resp_len < 2) {
        data->status = MOBILE_STATUS_ERROR;
        return NFC_ERR_COMMUNICATION;
    }
    
    // SW kodlarını kaydet
    data->sw1 = state->apdu_buffer->data[resp_len - 2];
    data->sw2 = state->apdu_buffer->data[resp_len - 1];
    s_lastSw1 = data->sw1;
    s_lastSw2 = data->sw2;
    
    if (!APDU_IS_SUCCESS(data->sw1, data->sw2)) {
        LOG_W("GET TOKEN failed: SW=0x%02X%02X", data->sw1, data->sw2);
        data->status = MOBILE_STATUS_ERROR;
        return NFC_ERR_AUTH_FAILED;
    }
    
    // Token verisi (SW hariç)
    uint16_t token_len = resp_len - 2;
    if (token_len > 0 && token_len < MOBILE_TOKEN_MAX_LENGTH) {
        memcpy(data->token, state->apdu_buffer->data, token_len);
        data->token[token_len] = '\0';  // Null terminate
        data->token_length = token_len;
        data->status = MOBILE_STATUS_TOKEN_RECEIVED;
        
        LOG_I("Token received: %s", data->token);
    } else {
        data->status = MOBILE_STATUS_APP_FOUND;
    }
    
    return NFC_SUCCESS;
}

NfcResult getMobileUserId(PN532DriverState* state, MobilePaymentData* data) {
    if (!state || !data) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    if (!state->initialized || !state->apdu_buffer) {
        return NFC_ERR_NOT_INITIALIZED;
    }
    
    LOG_D("Getting mobile user ID...");
    
    // GET USER ID APDU gönder
    uint16_t resp_len = 0;
    NfcResult res = sendApdu(state, APDU_GET_USER_ID, sizeof(APDU_GET_USER_ID), &resp_len);
    
    if (res != NFC_SUCCESS) {
        return res;
    }
    
    if (resp_len < 2) {
        return NFC_ERR_COMMUNICATION;
    }
    
    uint8_t sw1 = state->apdu_buffer->data[resp_len - 2];
    uint8_t sw2 = state->apdu_buffer->data[resp_len - 1];
    s_lastSw1 = sw1;
    s_lastSw2 = sw2;
    
    if (!APDU_IS_SUCCESS(sw1, sw2)) {
        LOG_W("GET USER ID failed: SW=0x%02X%02X", sw1, sw2);
        return NFC_ERR_AUTH_FAILED;
    }
    
    // User ID verisi (SW hariç)
    uint16_t id_len = resp_len - 2;
    if (id_len > 0 && id_len < MOBILE_USER_ID_MAX_LENGTH) {
        memcpy(data->user_id, state->apdu_buffer->data, id_len);
        data->user_id[id_len] = '\0';
        data->user_id_length = id_len;
        
        LOG_I("User ID received: %s", data->user_id);
    }
    
    return NFC_SUCCESS;
}

NfcResult getMobileBalance(PN532DriverState* state, MobilePaymentData* data) {
    if (!state || !data) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    if (!state->initialized || !state->apdu_buffer) {
        return NFC_ERR_NOT_INITIALIZED;
    }
    
    LOG_D("Getting mobile balance...");
    
    // GET BALANCE APDU gönder
    uint16_t resp_len = 0;
    NfcResult res = sendApdu(state, APDU_GET_BALANCE, sizeof(APDU_GET_BALANCE), &resp_len);
    
    if (res != NFC_SUCCESS) {
        data->balance_available = false;
        return res;
    }
    
    if (resp_len < 2) {
        data->balance_available = false;
        return NFC_ERR_COMMUNICATION;
    }
    
    uint8_t sw1 = state->apdu_buffer->data[resp_len - 2];
    uint8_t sw2 = state->apdu_buffer->data[resp_len - 1];
    s_lastSw1 = sw1;
    s_lastSw2 = sw2;
    
    if (!APDU_IS_SUCCESS(sw1, sw2)) {
        LOG_W("GET BALANCE failed: SW=0x%02X%02X", sw1, sw2);
        data->balance_available = false;
        return NFC_ERR_NOT_SUPPORTED;  // Bakiye desteklenmiyorsa hata değil
    }
    
    // Bakiye verisi (4 byte big-endian, kuruş cinsinden)
    uint16_t bal_len = resp_len - 2;
    if (bal_len >= 4) {
        data->balance = ((uint32_t)state->apdu_buffer->data[0] << 24) |
                        ((uint32_t)state->apdu_buffer->data[1] << 16) |
                        ((uint32_t)state->apdu_buffer->data[2] << 8) |
                        ((uint32_t)state->apdu_buffer->data[3]);
        data->balance_available = true;
        
        LOG_I("Balance: %lu kurus (%.2f TL)", data->balance, data->balance / 100.0f);
    } else {
        data->balance_available = false;
    }
    
    return NFC_SUCCESS;
}

NfcResult processMobilePayment(PN532DriverState* state, MobilePaymentData* data) {
    if (!state || !data) {
        return NFC_ERR_INVALID_PARAM;
    }
    
    // Data yapısını sıfırla
    MobilePaymentData_init(data);
    
    // UID bilgisini kopyala
    memcpy(data->uid, state->current_tag.uid, state->current_tag.uid_length);
    data->uid_length = state->current_tag.uid_length;
    getUidString(state, data->uid_string, sizeof(data->uid_string));
    
    LOG_I("=== MOBİL ÖDEME AKIŞI BAŞLIYOR ===");
    LOG_I("UID: %s", data->uid_string);
    
    data->platform = MOBILE_PLATFORM_ANDROID;
    
    // 1. SELECT Funtoria Application (retry ile)
    NfcResult res = NFC_ERR_COMMUNICATION;
    for (uint8_t sel_try = 0; sel_try < 3; sel_try++) {
        res = selectFuntoriaApp(state);
        if (res == NFC_SUCCESS) break;
        if (sel_try < 2) {
            LOG_D("SELECT retry %d/2", sel_try + 1);
            delay(100);
        }
    }
    
    if (res != NFC_SUCCESS) {
        data->status = MOBILE_STATUS_APP_NOT_FOUND;
        data->sw1 = s_lastSw1;
        data->sw2 = s_lastSw2;
        LOG_W("Funtoria application not found");
        return NFC_ERR_NOT_SUPPORTED;
    }
    
    data->status = MOBILE_STATUS_APP_FOUND;
    LOG_I("Funtoria application found!");
    
    // 2. GET TOKEN
    res = getMobileToken(state, data);
    if (res != NFC_SUCCESS) {
        LOG_W("Token retrieval failed");
        // Devam et, belki sadece user ID var
    }
    
    // 3. GET USER ID
    res = getMobileUserId(state, data);
    if (res != NFC_SUCCESS) {
        LOG_W("User ID retrieval failed");
    }
    
    // 4. GET BALANCE (opsiyonel)
    getMobileBalance(state, data);  // Hata olsa da sorun değil
    
    // Son durumu belirle
    if (data->token_length > 0 || data->user_id_length > 0) {
        data->status = MOBILE_STATUS_TOKEN_RECEIVED;
        LOG_I("=== MOBİL ÖDEME BAŞARILI ===");
        LOG_I("User ID: %s", data->user_id_length > 0 ? data->user_id : "N/A");
        LOG_I("Token: %s", data->token_length > 0 ? data->token : "N/A");
        if (data->balance_available) {
            LOG_I("Balance: %.2f TL", data->balance / 100.0f);
        }
        return NFC_SUCCESS;
    }
    
    data->status = MOBILE_STATUS_ERROR;
    LOG_W("=== MOBİL ÖDEME BAŞARISIZ ===");
    return NFC_ERR_AUTH_FAILED;
}

void getLastApduStatus(PN532DriverState* state, uint8_t* sw1, uint8_t* sw2) {
    (void)state;  // Kullanılmıyor, static değerler
    if (sw1) *sw1 = s_lastSw1;
    if (sw2) *sw2 = s_lastSw2;
}

}  // namespace PN532Driver
