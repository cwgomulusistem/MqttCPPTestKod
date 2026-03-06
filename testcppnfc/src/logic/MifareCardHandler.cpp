/**
 * @file MifareCardHandler.cpp
 * @brief Mifare Kart Isleri Implementasyonu (C-Style)
 * @version 3.0.0 - Setup Card Layout
 */

#include "MifareCardHandler.h"
#include "../config/Logger.h"

#include <stdio.h>
#include <string.h>

namespace {
constexpr uint8_t kCardTypeBlock = 4;
constexpr uint8_t kTenantVerifyBlock = 36;

constexpr uint8_t kWifiPasswordStartBlock = 8;
constexpr uint8_t kWifiSsidStartBlock = 12;
constexpr uint8_t kIpAddressStartBlock = 16;
constexpr uint8_t kMFirmIdStartBlock = 20;
constexpr uint8_t kGuidPart1StartBlock = 24;
constexpr uint8_t kGuidPart2StartBlock = 28;
constexpr uint8_t kDataLengthsStartBlock = 32;

constexpr uint8_t kFieldBlockCount = MifareHandler::SETUP_FIELD_BLOCK_COUNT;
constexpr uint8_t kFieldDataBytes = MifareHandler::SETUP_FIELD_DATA_BYTES;

const uint8_t kCardTypeKeyA[MIFARE_KEY_SIZE] = {'A', 'F', 'B', 'I', 'B', 'A'};
const uint8_t kCardTypeKeyB[MIFARE_KEY_SIZE] = {'A', 'F', 'B', 'I', 'B', 'A'};
const uint8_t kSetupKeyA[MIFARE_KEY_SIZE] = {'f', 'n', 't', 'r', 'i', 'a'};
const uint8_t kSetupKeyB[MIFARE_KEY_SIZE] = {'a', 'i', 'r', 't', 'n', 'f'};
MifareHandler::TenantVerifyConfig g_tenantVerifyConfig = {};

bool isValidWritableCardType(MifareCardType type) {
  return type == CARD_TYPE_SETUP || type == CARD_TYPE_CUSTOMER ||
         type == CARD_TYPE_GIFT || type == CARD_TYPE_SERVICE;
}

NfcResult authenticateAndRead(PN532DriverState *driver, uint8_t block,
                              const uint8_t *key, bool useKeyA,
                              uint8_t *outBlock) {
  NfcResult res = PN532Driver::authenticateMifare(driver, block, key, useKeyA);
  if (res != NFC_SUCCESS) {
    return res;
  }
  return PN532Driver::readMifareBlock(driver, block, outBlock);
}

NfcResult authenticateAndWrite(PN532DriverState *driver, uint8_t block,
                               const uint8_t *key, bool useKeyA,
                               const uint8_t *data) {
  NfcResult res = PN532Driver::authenticateMifare(driver, block, key, useKeyA);
  if (res != NFC_SUCCESS) {
    return res;
  }
  return PN532Driver::writeMifareBlock(driver, block, data);
}

NfcResult readBlockWithFallback(PN532DriverState *driver, uint8_t block,
                                const uint8_t *keyA, const uint8_t *keyB,
                                uint8_t *outBlock) {
  NfcResult res = authenticateAndRead(driver, block, keyA, true, outBlock);
  if (res == NFC_SUCCESS) {
    return NFC_SUCCESS;
  }
  return authenticateAndRead(driver, block, keyB, false, outBlock);
}

NfcResult writeBlockWithFallback(PN532DriverState *driver, uint8_t block,
                                 const uint8_t *keyA, const uint8_t *keyB,
                                 const uint8_t *data) {
  NfcResult res = authenticateAndWrite(driver, block, keyA, true, data);
  if (res == NFC_SUCCESS) {
    return NFC_SUCCESS;
  }
  return authenticateAndWrite(driver, block, keyB, false, data);
}

NfcResult readFieldBlocks(PN532DriverState *driver, uint8_t startBlock,
                          const uint8_t *keyA, const uint8_t *keyB,
                          uint8_t *outData) {
  for (uint8_t i = 0; i < kFieldBlockCount; i++) {
    uint8_t blockData[MIFARE_BLOCK_SIZE] = {0};
    const uint8_t blockNo = static_cast<uint8_t>(startBlock + i);

    NfcResult res = readBlockWithFallback(driver, blockNo, keyA, keyB, blockData);
    if (res != NFC_SUCCESS) {
      LOG_W("MIFARE: read failed block=%u", static_cast<unsigned>(blockNo));
      return res;
    }

    memcpy(outData + (i * MIFARE_BLOCK_SIZE), blockData, MIFARE_BLOCK_SIZE);
  }
  return NFC_SUCCESS;
}

NfcResult writeFieldBlocks(PN532DriverState *driver, uint8_t startBlock,
                           const uint8_t *keyA, const uint8_t *keyB,
                           const uint8_t *data) {
  for (uint8_t i = 0; i < kFieldBlockCount; i++) {
    const uint8_t blockNo = static_cast<uint8_t>(startBlock + i);
    const uint8_t *blockData = data + (i * MIFARE_BLOCK_SIZE);

    NfcResult res = writeBlockWithFallback(driver, blockNo, keyA, keyB, blockData);
    if (res != NFC_SUCCESS) {
      LOG_W("MIFARE: write failed block=%u", static_cast<unsigned>(blockNo));
      return res;
    }
  }
  return NFC_SUCCESS;
}

uint8_t sanitizeLen(uint8_t len) {
  return (len > kFieldDataBytes) ? kFieldDataBytes : len;
}

void rawToText(char *out, size_t outLen, const uint8_t *raw, uint8_t explicitLen) {
  if (!out || outLen == 0 || !raw) {
    return;
  }

  uint8_t len = sanitizeLen(explicitLen);
  if (len == 0) {
    while (len < kFieldDataBytes && raw[len] != '\0') {
      len++;
    }
  }

  if (len >= outLen) {
    len = static_cast<uint8_t>(outLen - 1);
  }

  memcpy(out, raw, len);
  out[len] = '\0';
}

void textToRaw(const char *text, uint8_t *outRaw, uint8_t *outLen) {
  if (!outRaw || !outLen) {
    return;
  }

  memset(outRaw, 0, kFieldDataBytes);
  *outLen = 0;

  if (!text) {
    return;
  }

  size_t textLen = strlen(text);
  if (textLen > kFieldDataBytes) {
    textLen = kFieldDataBytes;
  }

  memcpy(outRaw, text, textLen);
  *outLen = static_cast<uint8_t>(textLen);
}
} // namespace

namespace MifareHandler {

void setTenantVerifyConfig(const TenantVerifyConfig *config) {
  if (!config) {
    return;
  }

  g_tenantVerifyConfig = *config;
  if (g_tenantVerifyConfig.verify_block == 0) {
    g_tenantVerifyConfig.verify_block = kTenantVerifyBlock;
  }
  g_tenantVerifyConfig.configured = true;
  LOG_I("MIFARE: tenant verify config updated (block=%u)",
        static_cast<unsigned>(g_tenantVerifyConfig.verify_block));
}

void clearTenantVerifyConfig() {
  memset(&g_tenantVerifyConfig, 0, sizeof(g_tenantVerifyConfig));
  g_tenantVerifyConfig.verify_block = kTenantVerifyBlock;
  g_tenantVerifyConfig.configured = false;
}

void initConfig(MifareOpConfig *config, const uint8_t *key) {
  if (!config) {
    return;
  }

  memset(config, 0, sizeof(MifareOpConfig));

  if (key) {
    memcpy(config->key_a, key, MIFARE_KEY_SIZE);
    memcpy(config->key_b, key, MIFARE_KEY_SIZE);
  } else {
    memcpy(config->key_a, kCardTypeKeyA, MIFARE_KEY_SIZE);
    memcpy(config->key_b, kCardTypeKeyB, MIFARE_KEY_SIZE);
  }

  config->auth_block = kCardTypeBlock;
  config->data_block = kCardTypeBlock;
  config->use_key_a = true;
}

NfcResult process(PN532DriverState *driver, const NfcTagInfo *tag,
                  MifareOpConfig *config, MifareCardType *out_card_type) {
  LOG_FUNC_ENTER();

  if (!driver || !tag) {
    return NFC_ERR_INVALID_PARAM;
  }

  if (!driver->initialized) {
    return NFC_ERR_NOT_INITIALIZED;
  }

  if (tag->type != TAG_MIFARE_CLASSIC) {
    LOG_W("Not a Mifare Classic card");
    if (out_card_type) {
      *out_card_type = CARD_TYPE_UNKNOWN;
    }
    return NFC_ERR_NOT_SUPPORTED;
  }

  MifareOpConfig localConfig = {};
  if (!config) {
    initConfig(&localConfig, nullptr);
    config = &localConfig;
  }

  const uint8_t *key = config->use_key_a ? config->key_a : config->key_b;
  uint8_t block_data[MIFARE_BLOCK_SIZE] = {0};

  NfcResult res = authenticateAndRead(driver, config->auth_block, key,
                                      config->use_key_a, block_data);
  if (res != NFC_SUCCESS) {
    LOG_W("Auth/read failed for card-type block %u",
          static_cast<unsigned>(config->auth_block));
    if (out_card_type) {
      *out_card_type = CARD_TYPE_UNKNOWN;
    }
    return res;
  }

  MifareCardType parsedType = parseCardType(block_data[0]);
  if (out_card_type) {
    *out_card_type = parsedType;
  }

  LOG_I("MIFARE: card_type=%s raw=%u", cardTypeToString(parsedType),
        static_cast<unsigned>(block_data[0]));

  if (parsedType == CARD_TYPE_SETUP) {
    SetupCardData setup = {};
    res = readSetupCardData(driver, &setup);
    if (res == NFC_SUCCESS) {
      LOG_I("SETUP CARD: ssid='%s' ip='%s' mfirm='%s' guid='%s'", setup.wifi_ssid,
            setup.ip_address, setup.mfirm_id, setup.guid_full);
    } else {
      LOG_W("SETUP CARD: decode failed: %s", NfcResult_toString(res));
    }
  }

  LOG_FUNC_EXIT();
  return NFC_SUCCESS;
}

NfcResult readSetupCardData(PN532DriverState *driver, SetupCardData *out_data) {
  if (!driver || !out_data) {
    return NFC_ERR_INVALID_PARAM;
  }

  if (!driver->initialized) {
    return NFC_ERR_NOT_INITIALIZED;
  }

  memset(out_data, 0, sizeof(*out_data));
  out_data->card_type = CARD_TYPE_UNKNOWN;

  uint8_t cardTypeBlock[MIFARE_BLOCK_SIZE] = {0};
  NfcResult res =
      readBlockWithFallback(driver, kCardTypeBlock, kCardTypeKeyA, kCardTypeKeyB,
                            cardTypeBlock);
  if (res != NFC_SUCCESS) {
    return res;
  }

  out_data->card_type = parseCardType(cardTypeBlock[0]);

  if (out_data->card_type != CARD_TYPE_SETUP) {
    if (!g_tenantVerifyConfig.configured) {
      LOG_W("TODO: tenant verify endpoint bilgisi yok, block 36 dogrulama atlandi");
      out_data->tenant_verify_valid = false;
      return NFC_SUCCESS;
    }

    uint8_t tenantBlock[MIFARE_BLOCK_SIZE] = {0};
    const uint8_t verifyBlock = g_tenantVerifyConfig.verify_block;
    res = readBlockWithFallback(driver, verifyBlock, g_tenantVerifyConfig.key_a,
                                g_tenantVerifyConfig.key_b, tenantBlock);
    if (res == NFC_SUCCESS) {
      memcpy(out_data->tenant_verify_block, tenantBlock, MIFARE_BLOCK_SIZE);
      out_data->tenant_verify_valid = true;
    } else {
      out_data->tenant_verify_valid = false;
    }
    return NFC_SUCCESS;
  }

  uint8_t rawLengths[kFieldDataBytes] = {0};
  uint8_t rawWifiPass[kFieldDataBytes] = {0};
  uint8_t rawWifiSsid[kFieldDataBytes] = {0};
  uint8_t rawIp[kFieldDataBytes] = {0};
  uint8_t rawMFirm[kFieldDataBytes] = {0};
  uint8_t rawGuid1[kFieldDataBytes] = {0};
  uint8_t rawGuid2[kFieldDataBytes] = {0};

  res = readFieldBlocks(driver, kDataLengthsStartBlock, kSetupKeyA, kSetupKeyB,
                        rawLengths);
  if (res != NFC_SUCCESS) {
    return res;
  }
  memcpy(out_data->lengths_raw, rawLengths, kFieldDataBytes);

  res = readFieldBlocks(driver, kWifiPasswordStartBlock, kSetupKeyA, kSetupKeyB,
                        rawWifiPass);
  if (res != NFC_SUCCESS) {
    return res;
  }

  res = readFieldBlocks(driver, kWifiSsidStartBlock, kSetupKeyA, kSetupKeyB,
                        rawWifiSsid);
  if (res != NFC_SUCCESS) {
    return res;
  }

  res = readFieldBlocks(driver, kIpAddressStartBlock, kSetupKeyA, kSetupKeyB,
                        rawIp);
  if (res != NFC_SUCCESS) {
    return res;
  }

  res = readFieldBlocks(driver, kMFirmIdStartBlock, kSetupKeyA, kSetupKeyB,
                        rawMFirm);
  if (res != NFC_SUCCESS) {
    return res;
  }

  res = readFieldBlocks(driver, kGuidPart1StartBlock, kSetupKeyA, kSetupKeyB,
                        rawGuid1);
  if (res != NFC_SUCCESS) {
    return res;
  }

  res = readFieldBlocks(driver, kGuidPart2StartBlock, kSetupKeyA, kSetupKeyB,
                        rawGuid2);
  if (res != NFC_SUCCESS) {
    return res;
  }

  const uint8_t ssidLen = sanitizeLen(rawLengths[0]);
  const uint8_t passLen = sanitizeLen(rawLengths[1]);
  const uint8_t ipLen = sanitizeLen(rawLengths[2]);
  const uint8_t mfirmLen = sanitizeLen(rawLengths[3]);
  const uint8_t guid1Len = sanitizeLen(rawLengths[4]);
  const uint8_t guid2Len = sanitizeLen(rawLengths[5]);

  rawToText(out_data->wifi_ssid, sizeof(out_data->wifi_ssid), rawWifiSsid,
            ssidLen);
  rawToText(out_data->wifi_password, sizeof(out_data->wifi_password), rawWifiPass,
            passLen);
  rawToText(out_data->ip_address, sizeof(out_data->ip_address), rawIp, ipLen);
  rawToText(out_data->mfirm_id, sizeof(out_data->mfirm_id), rawMFirm, mfirmLen);
  rawToText(out_data->guid_part_1, sizeof(out_data->guid_part_1), rawGuid1,
            guid1Len);
  rawToText(out_data->guid_part_2, sizeof(out_data->guid_part_2), rawGuid2,
            guid2Len);

  snprintf(out_data->guid_full, sizeof(out_data->guid_full), "%s%s",
           out_data->guid_part_1, out_data->guid_part_2);

  return NFC_SUCCESS;
}

NfcResult writeCardType(PN532DriverState *driver, MifareCardType card_type) {
  if (!driver) {
    return NFC_ERR_INVALID_PARAM;
  }

  if (!driver->initialized) {
    return NFC_ERR_NOT_INITIALIZED;
  }

  if (!isValidWritableCardType(card_type)) {
    return NFC_ERR_INVALID_PARAM;
  }

  uint8_t blockData[MIFARE_BLOCK_SIZE] = {0};
  NfcResult res =
      readBlockWithFallback(driver, kCardTypeBlock, kCardTypeKeyA, kCardTypeKeyB,
                            blockData);
  if (res != NFC_SUCCESS) {
    memset(blockData, 0, sizeof(blockData));
  }

  blockData[0] = static_cast<uint8_t>(card_type);

  return writeBlockWithFallback(driver, kCardTypeBlock, kCardTypeKeyA,
                                kCardTypeKeyB, blockData);
}

NfcResult writeSetupCardData(PN532DriverState *driver, const SetupCardData *data) {
  if (!driver || !data) {
    return NFC_ERR_INVALID_PARAM;
  }

  if (!driver->initialized) {
    return NFC_ERR_NOT_INITIALIZED;
  }

  NfcResult res = writeCardType(driver, CARD_TYPE_SETUP);
  if (res != NFC_SUCCESS) {
    return res;
  }

  uint8_t rawLengths[kFieldDataBytes] = {0};
  uint8_t rawWifiPass[kFieldDataBytes] = {0};
  uint8_t rawWifiSsid[kFieldDataBytes] = {0};
  uint8_t rawIp[kFieldDataBytes] = {0};
  uint8_t rawMFirm[kFieldDataBytes] = {0};
  uint8_t rawGuid1[kFieldDataBytes] = {0};
  uint8_t rawGuid2[kFieldDataBytes] = {0};

  uint8_t ssidLen = 0;
  uint8_t passLen = 0;
  uint8_t ipLen = 0;
  uint8_t mfirmLen = 0;
  uint8_t guid1Len = 0;
  uint8_t guid2Len = 0;

  textToRaw(data->wifi_ssid, rawWifiSsid, &ssidLen);
  textToRaw(data->wifi_password, rawWifiPass, &passLen);
  textToRaw(data->ip_address, rawIp, &ipLen);
  textToRaw(data->mfirm_id, rawMFirm, &mfirmLen);
  textToRaw(data->guid_part_1, rawGuid1, &guid1Len);
  textToRaw(data->guid_part_2, rawGuid2, &guid2Len);

  rawLengths[0] = ssidLen;
  rawLengths[1] = passLen;
  rawLengths[2] = ipLen;
  rawLengths[3] = mfirmLen;
  rawLengths[4] = guid1Len;
  rawLengths[5] = guid2Len;

  res = writeFieldBlocks(driver, kDataLengthsStartBlock, kSetupKeyA, kSetupKeyB,
                         rawLengths);
  if (res != NFC_SUCCESS) {
    return res;
  }

  res = writeFieldBlocks(driver, kWifiPasswordStartBlock, kSetupKeyA, kSetupKeyB,
                         rawWifiPass);
  if (res != NFC_SUCCESS) {
    return res;
  }

  res = writeFieldBlocks(driver, kWifiSsidStartBlock, kSetupKeyA, kSetupKeyB,
                         rawWifiSsid);
  if (res != NFC_SUCCESS) {
    return res;
  }

  res = writeFieldBlocks(driver, kIpAddressStartBlock, kSetupKeyA, kSetupKeyB,
                         rawIp);
  if (res != NFC_SUCCESS) {
    return res;
  }

  res = writeFieldBlocks(driver, kMFirmIdStartBlock, kSetupKeyA, kSetupKeyB,
                         rawMFirm);
  if (res != NFC_SUCCESS) {
    return res;
  }

  res = writeFieldBlocks(driver, kGuidPart1StartBlock, kSetupKeyA, kSetupKeyB,
                         rawGuid1);
  if (res != NFC_SUCCESS) {
    return res;
  }

  return writeFieldBlocks(driver, kGuidPart2StartBlock, kSetupKeyA, kSetupKeyB,
                          rawGuid2);
}

NfcResult readBalance(PN532DriverState *driver, uint8_t block, const uint8_t *key,
                      int32_t *out_balance) {
  if (!driver || !key || !out_balance) {
    return NFC_ERR_INVALID_PARAM;
  }

  if (!driver->initialized) {
    return NFC_ERR_NOT_INITIALIZED;
  }

  uint8_t block_data[MIFARE_BLOCK_SIZE] = {0};
  NfcResult res = authenticateAndRead(driver, block, key, true, block_data);
  if (res != NFC_SUCCESS) {
    return res;
  }

  *out_balance = (int32_t)(((uint32_t)block_data[0]) |
                           ((uint32_t)block_data[1] << 8) |
                           ((uint32_t)block_data[2] << 16) |
                           ((uint32_t)block_data[3] << 24));

  LOG_D("Balance read: %ld", (long)*out_balance);
  return NFC_SUCCESS;
}

NfcResult writeBalance(PN532DriverState *driver, uint8_t block, const uint8_t *key,
                       int32_t balance) {
  if (!driver || !key) {
    return NFC_ERR_INVALID_PARAM;
  }

  if (!driver->initialized) {
    return NFC_ERR_NOT_INITIALIZED;
  }

  uint8_t block_data[MIFARE_BLOCK_SIZE] = {0};
  block_data[0] = (uint8_t)(balance & 0xFF);
  block_data[1] = (uint8_t)((balance >> 8) & 0xFF);
  block_data[2] = (uint8_t)((balance >> 16) & 0xFF);
  block_data[3] = (uint8_t)((balance >> 24) & 0xFF);

  NfcResult res = authenticateAndWrite(driver, block, key, true, block_data);
  if (res != NFC_SUCCESS) {
    return res;
  }

  LOG_D("Balance written: %ld", (long)balance);
  return NFC_SUCCESS;
}

MifareCardType parseCardType(uint8_t type_byte) {
  switch (type_byte) {
  case 0x01:
    return CARD_TYPE_SETUP;
  case 0x02:
    return CARD_TYPE_CUSTOMER;
  case 0x03:
    return CARD_TYPE_GIFT;
  case 0x04:
    return CARD_TYPE_SERVICE;
  case 0x05:
    return CARD_TYPE_GIFT; // Legacy gift mapping
  default:
    return CARD_TYPE_UNKNOWN;
  }
}

const char *cardTypeToString(MifareCardType type) {
  switch (type) {
  case CARD_TYPE_SETUP:
    return "SETUP";
  case CARD_TYPE_CUSTOMER:
    return "CUSTOMER";
  case CARD_TYPE_GIFT:
    return "GIFT";
  case CARD_TYPE_SERVICE:
    return "SERVICE";
  case CARD_TYPE_MOBILE:
    return "MOBILE";
  default:
    return "UNKNOWN";
  }
}

} // namespace MifareHandler
