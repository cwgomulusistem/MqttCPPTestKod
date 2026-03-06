/**
 * @file SpiffsConfig.cpp
 * @brief SPIFFS Konfigürasyon Yönetimi Implementasyonu
 * @version 2.0.0 - Hibrit Mimari
 */

#include "SpiffsConfig.h"
#include "../config/Logger.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <string.h>

namespace SpiffsConfig {

static bool s_mounted = false;

bool init(bool formatOnFail) {
    LOG_D("SPIFFS initializing...");
    
    if (s_mounted) {
        LOG_D("SPIFFS already mounted");
        return true;
    }
    
    if (!SPIFFS.begin(formatOnFail)) {
        LOG_E("SPIFFS mount failed!");
        return false;
    }
    
    s_mounted = true;
    
    size_t total, used;
    getInfo(&total, &used);
    LOG_I("SPIFFS mounted: %u/%u bytes used", used, total);
    
    return true;
}

bool isMounted() {
    return s_mounted;
}

bool loadNfcSettings(NfcDebounceState* state) {
    if (!state) {
        LOG_E("Invalid state pointer");
        return false;
    }
    
    if (!s_mounted) {
        LOG_E("SPIFFS not mounted");
        return false;
    }
    
    if (!SPIFFS.exists(SPIFFS_NFC_SETTINGS_PATH)) {
        LOG_W("NFC settings file not found: %s", SPIFFS_NFC_SETTINGS_PATH);
        return false;
    }
    
    File file = SPIFFS.open(SPIFFS_NFC_SETTINGS_PATH, "r");
    if (!file) {
        LOG_E("Failed to open NFC settings file");
        return false;
    }
    
    // JSON parse (ArduinoJson 7.x)
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        LOG_E("JSON parse error: %s", error.c_str());
        return false;
    }
    
    // Değerleri yükle
    state->cooldown_ms = doc["cooldown_ms"] | NFC_COOLDOWN_MS_DEFAULT;
    state->debounce_enabled = doc["debounce_enabled"] | NFC_DEBOUNCE_ENABLED;
    state->card_removed_threshold_ms = doc["card_removed_threshold_ms"] | NFC_CARD_REMOVED_THRESHOLD;
    
    // Sınır kontrolleri
    if (state->cooldown_ms < NFC_COOLDOWN_MS_MIN) {
        state->cooldown_ms = NFC_COOLDOWN_MS_MIN;
    }
    if (state->cooldown_ms > NFC_COOLDOWN_MS_MAX) {
        state->cooldown_ms = NFC_COOLDOWN_MS_MAX;
    }
    
    LOG_I("NFC settings loaded: cooldown=%lums, debounce=%s", 
          state->cooldown_ms, 
          state->debounce_enabled ? "ON" : "OFF");
    
    return true;
}

bool saveNfcSettings(const NfcDebounceState* state) {
    if (!state) {
        LOG_E("Invalid state pointer");
        return false;
    }
    
    if (!s_mounted) {
        LOG_E("SPIFFS not mounted");
        return false;
    }
    
    // /config klasörünü oluştur (yoksa)
    if (!SPIFFS.exists("/config")) {
        LOG_D("Creating /config directory");
    }
    
    File file = SPIFFS.open(SPIFFS_NFC_SETTINGS_PATH, "w");
    if (!file) {
        LOG_E("Failed to create NFC settings file");
        return false;
    }
    
    // JSON oluştur (ArduinoJson 7.x)
    JsonDocument doc;
    doc["cooldown_ms"] = state->cooldown_ms;
    doc["debounce_enabled"] = state->debounce_enabled;
    doc["card_removed_threshold_ms"] = state->card_removed_threshold_ms;
    
    // Yaz
    size_t written = serializeJson(doc, file);
    file.close();
    
    if (written == 0) {
        LOG_E("Failed to write NFC settings");
        return false;
    }
    
    LOG_I("NFC settings saved: %u bytes", written);
    return true;
}

bool createDefaultNfcSettings() {
    NfcDebounceState defaults;
    NfcDebounceState_init(&defaults);
    return saveNfcSettings(&defaults);
}

bool nfcSettingsExist() {
    if (!s_mounted) return false;
    return SPIFFS.exists(SPIFFS_NFC_SETTINGS_PATH);
}

bool saveSetupCardData(const MifareHandler::SetupCardData *data) {
    if (!data) {
        LOG_E("Invalid setup data pointer");
        return false;
    }

    if (!s_mounted) {
        LOG_E("SPIFFS not mounted");
        return false;
    }

    File file = SPIFFS.open(SPIFFS_SETUP_CARD_PATH, "w");
    if (!file) {
        LOG_E("Failed to create setup file");
        return false;
    }

    JsonDocument doc;
    doc["setup_completed"] = true;
    doc["card_type"] = static_cast<uint8_t>(data->card_type);
    doc["wifi_ssid"] = data->wifi_ssid;
    doc["wifi_password"] = data->wifi_password;
    doc["ip_address"] = data->ip_address;
    doc["mfirm_id"] = data->mfirm_id;
    doc["guid_part_1"] = data->guid_part_1;
    doc["guid_part_2"] = data->guid_part_2;
    doc["guid_full"] = data->guid_full;

    JsonArray lengths = doc["lengths_raw"].to<JsonArray>();
    for (uint8_t i = 0; i < MifareHandler::SETUP_FIELD_DATA_BYTES; i++) {
        lengths.add(data->lengths_raw[i]);
    }

    size_t written = serializeJson(doc, file);
    file.close();

    if (written == 0) {
        LOG_E("Failed to write setup file");
        return false;
    }

    LOG_I("Setup data saved: %u bytes", static_cast<unsigned>(written));
    return true;
}

bool loadSetupCardData(MifareHandler::SetupCardData *outData) {
    if (!outData) {
        LOG_E("Invalid setup out pointer");
        return false;
    }

    if (!s_mounted) {
        LOG_E("SPIFFS not mounted");
        return false;
    }

    if (!SPIFFS.exists(SPIFFS_SETUP_CARD_PATH)) {
        return false;
    }

    File file = SPIFFS.open(SPIFFS_SETUP_CARD_PATH, "r");
    if (!file) {
        LOG_E("Failed to open setup file");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        LOG_E("Setup JSON parse error: %s", error.c_str());
        return false;
    }

    bool setupCompleted = doc["setup_completed"] | false;
    if (!setupCompleted) {
        return false;
    }

    memset(outData, 0, sizeof(*outData));
    outData->card_type =
        MifareHandler::parseCardType(doc["card_type"] | static_cast<uint8_t>(CARD_TYPE_SETUP));
    if (outData->card_type == CARD_TYPE_UNKNOWN) {
        outData->card_type = CARD_TYPE_SETUP;
    }

    const char *ssid = doc["wifi_ssid"] | "";
    const char *pass = doc["wifi_password"] | "";
    const char *ip = doc["ip_address"] | "";
    const char *mfirm = doc["mfirm_id"] | "";
    const char *guid1 = doc["guid_part_1"] | "";
    const char *guid2 = doc["guid_part_2"] | "";
    const char *guidFull = doc["guid_full"] | "";

    strncpy(outData->wifi_ssid, ssid, sizeof(outData->wifi_ssid) - 1);
    strncpy(outData->wifi_password, pass, sizeof(outData->wifi_password) - 1);
    strncpy(outData->ip_address, ip, sizeof(outData->ip_address) - 1);
    strncpy(outData->mfirm_id, mfirm, sizeof(outData->mfirm_id) - 1);
    strncpy(outData->guid_part_1, guid1, sizeof(outData->guid_part_1) - 1);
    strncpy(outData->guid_part_2, guid2, sizeof(outData->guid_part_2) - 1);
    strncpy(outData->guid_full, guidFull, sizeof(outData->guid_full) - 1);

    JsonArray lengths = doc["lengths_raw"].as<JsonArray>();
    uint8_t idx = 0;
    for (JsonVariant v : lengths) {
        if (idx >= MifareHandler::SETUP_FIELD_DATA_BYTES) {
            break;
        }
        outData->lengths_raw[idx++] = v.as<uint8_t>();
    }

    if (outData->guid_full[0] == '\0') {
        snprintf(outData->guid_full, sizeof(outData->guid_full), "%s%s",
                 outData->guid_part_1, outData->guid_part_2);
    }

    LOG_I("Setup data loaded from SPIFFS");
    return true;
}

bool setupCardDataExist() {
    if (!s_mounted) {
        return false;
    }
    return SPIFFS.exists(SPIFFS_SETUP_CARD_PATH);
}

void getInfo(size_t* totalBytes, size_t* usedBytes) {
    if (!s_mounted) {
        if (totalBytes) *totalBytes = 0;
        if (usedBytes) *usedBytes = 0;
        return;
    }
    
    if (totalBytes) *totalBytes = SPIFFS.totalBytes();
    if (usedBytes) *usedBytes = SPIFFS.usedBytes();
}

}  // namespace SpiffsConfig
