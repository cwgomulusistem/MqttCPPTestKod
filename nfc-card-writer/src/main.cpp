#include <Adafruit_PN532.h>
#include <Arduino.h>
#include <ctype.h>
#include <string.h>

namespace {
constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kPn532Baud = 115200;
constexpr uint8_t kPn532ResetPin = 0xFF; // Not used in HSU
constexpr uint8_t kPn532RxPin = 16;
constexpr uint8_t kPn532TxPin = 17;
constexpr uint16_t kTagDetectTimeoutMs = 120;
constexpr uint8_t kCardTypeBlock = 4;
constexpr uint8_t kCardTypeTrailerBlock = 7;
constexpr uint8_t kTenantVerifyBlock = 36;
constexpr size_t kInputBufferSize = 64;
constexpr uint8_t kKeyTypeA = 0;
constexpr uint8_t kKeyTypeB = 1;

// Card type keys: AFBIBA (hex: 41 46 42 49 42 41).
const uint8_t kCardTypeKeyA[6] = {0x41, 0x46, 0x42, 0x49, 0x42, 0x41};
const uint8_t kCardTypeKeyB[6] = {0x41, 0x46, 0x42, 0x49, 0x42, 0x41};
const uint8_t kDefaultTestKeyA[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const uint8_t kTenantVerifyKeyA[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
constexpr char kTenantName[] = "ismet";

enum class PendingAction {
  NONE = 0,
  READ_TYPE,
  WRITE_TYPE,
};

enum class CardTypeAuthProfile {
  UNKNOWN = 0,
  AFBIBA,
  DEFAULT_FF,
};

Adafruit_PN532 g_nfc(kPn532ResetPin, &Serial2);
bool g_nfcReady = false;
PendingAction g_pendingAction = PendingAction::NONE;
uint8_t g_pendingType = 0;

char g_inputBuffer[kInputBufferSize] = {0};
size_t g_inputLen = 0;

void printHelp() {
  Serial.println();
  Serial.println("===   Card Writer ===");
  Serial.println("Commands:");
  Serial.println("  help                  -> show this menu");
  Serial.println("  read                  -> read card type from next card");
  Serial.println("  setup | kurulum | 1   -> write SETUP card type (0x01)");
  Serial.println("  customer | musteri | 2-> write CUSTOMER card type (0x02)");
  Serial.println("  gift | hediye | 3     -> write GIFT card type (0x03)");
  Serial.println("  service | servis | 4  -> write SERVICE card type (0x04)");
  Serial.println("  cancel                -> cancel pending action");
  Serial.println("Note: non-SETUP cards also write tenant verify block 36 = 'ismet'");
  Serial.println("============================");
  Serial.println();
}

const char *cardTypeToString(uint8_t type) {
  switch (type) {
  case 0x01:
    return "SETUP";
  case 0x02:
    return "CUSTOMER";
  case 0x03:
    return "GIFT";
  case 0x04:
    return "SERVICE";
  case 0x05:
    return "GIFT (legacy)";
  case 0xF0:
    return "MOBILE";
  default:
    return "UNKNOWN";
  }
}

void uidToString(const uint8_t *uid, uint8_t uidLen, char *out, size_t outSize) {
  if (!uid || !out || outSize == 0) {
    return;
  }

  size_t writePos = 0;
  for (uint8_t i = 0; i < uidLen; ++i) {
    if (writePos + 3 >= outSize) {
      break;
    }
    int written = snprintf(out + writePos, outSize - writePos, "%02X", uid[i]);
    if (written <= 0) {
      break;
    }
    writePos += static_cast<size_t>(written);
  }
}

bool authenticateBlock(const uint8_t *uid, uint8_t uidLen, uint8_t blockNumber,
                       uint8_t keyType, const uint8_t *key) {
  return g_nfc.mifareclassic_AuthenticateBlock(
      const_cast<uint8_t *>(uid), uidLen, blockNumber, keyType,
      const_cast<uint8_t *>(key));
}

bool authenticateCardTypeBlock(const uint8_t *uid, uint8_t uidLen,
                               CardTypeAuthProfile *outProfile) {
  if (outProfile) {
    *outProfile = CardTypeAuthProfile::UNKNOWN;
  }

  struct AuthAttempt {
    uint8_t keyType;
    const uint8_t *key;
    CardTypeAuthProfile profile;
    const char *label;
  };

  const AuthAttempt attempts[] = {
      {kKeyTypeA, kCardTypeKeyA, CardTypeAuthProfile::AFBIBA, "AFBIBA Key A"},
      {kKeyTypeB, kCardTypeKeyB, CardTypeAuthProfile::AFBIBA, "AFBIBA Key B"},
      {kKeyTypeA, kDefaultTestKeyA, CardTypeAuthProfile::DEFAULT_FF,
       "FF FF FF FF FF FF Key A"},
      {kKeyTypeB, kDefaultTestKeyA, CardTypeAuthProfile::DEFAULT_FF,
       "FF FF FF FF FF FF Key B"},
  };

  for (size_t i = 0; i < (sizeof(attempts) / sizeof(attempts[0])); ++i) {
    if (authenticateBlock(uid, uidLen, kCardTypeBlock, attempts[i].keyType,
                          attempts[i].key)) {
      Serial.printf("Auth success on block %u with %s.\n", kCardTypeBlock,
                    attempts[i].label);
      if (outProfile) {
        *outProfile = attempts[i].profile;
      }
      return true;
    }

    Serial.printf("Auth failed on block %u with %s.\n", kCardTypeBlock,
                  attempts[i].label);

    // If auth fails, card can enter HALT. Re-select before next key attempt.
    if ((i + 1) < (sizeof(attempts) / sizeof(attempts[0]))) {
      uint8_t dummyUid[7] = {0};
      uint8_t dummyUidLen = 0;
      g_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, dummyUid, &dummyUidLen,
                                50);
    }
  }

  return false;
}

bool migrateCardTypeKeysIfNeeded(const uint8_t *uid, uint8_t uidLen,
                                 CardTypeAuthProfile profile,
                                 bool *outMigrated) {
  if (outMigrated) {
    *outMigrated = false;
  }

  if (profile != CardTypeAuthProfile::DEFAULT_FF) {
    return true;
  }

  // Authenticate sector trailer with default key before migration.
  if (!authenticateBlock(uid, uidLen, kCardTypeTrailerBlock, kKeyTypeA,
                         kDefaultTestKeyA)) {
    uint8_t dummyUid[7] = {0};
    uint8_t dummyUidLen = 0;
    g_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, dummyUid, &dummyUidLen,
                              50);

    if (!authenticateBlock(uid, uidLen, kCardTypeTrailerBlock, kKeyTypeB,
                           kDefaultTestKeyA)) {
      Serial.println("key-migrate: auth failed on block 7 with FF Key A/B.");
      return false;
    }
  }

  uint8_t trailer[16] = {0};
  if (!g_nfc.mifareclassic_ReadDataBlock(kCardTypeTrailerBlock, trailer)) {
    // Fallback to transport access bytes if trailer read is blocked.
    trailer[6] = 0xFF;
    trailer[7] = 0x07;
    trailer[8] = 0x80;
    trailer[9] = 0x69;
  }

  memcpy(trailer, kCardTypeKeyA, sizeof(kCardTypeKeyA));
  memcpy(trailer + 10, kCardTypeKeyB, sizeof(kCardTypeKeyB));

  if (!g_nfc.mifareclassic_WriteDataBlock(kCardTypeTrailerBlock, trailer)) {
    Serial.println("key-migrate: write trailer block 7 failed.");
    return false;
  }

  // After trailer key update, re-select card to reset crypto state.
  uint8_t dummyUid[7] = {0};
  uint8_t dummyUidLen = 0;
  g_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, dummyUid, &dummyUidLen, 50);

  if (!authenticateBlock(uid, uidLen, kCardTypeBlock, kKeyTypeA,
                         kCardTypeKeyA)) {
    Serial.println("key-migrate: verify Key A with AFBIBA failed.");
    return false;
  }

  g_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, dummyUid, &dummyUidLen, 50);

  if (!authenticateBlock(uid, uidLen, kCardTypeBlock, kKeyTypeB,
                         kCardTypeKeyB)) {
    Serial.println("key-migrate: verify Key B with AFBIBA failed.");
    return false;
  }

  if (outMigrated) {
    *outMigrated = true;
  }
  return true;
}

bool authenticateTenantVerifyBlock(const uint8_t *uid, uint8_t uidLen) {
  return g_nfc.mifareclassic_AuthenticateBlock(
      const_cast<uint8_t *>(uid), uidLen, kTenantVerifyBlock, kKeyTypeA,
      const_cast<uint8_t *>(kTenantVerifyKeyA));
}

bool readTypeByte(const uint8_t *uid, uint8_t uidLen, uint8_t *outType,
                  uint8_t *outRawBlock) {
  if (!authenticateCardTypeBlock(uid, uidLen, nullptr)) {
    return false;
  }

  uint8_t block[16] = {0};
  if (!g_nfc.mifareclassic_ReadDataBlock(kCardTypeBlock, block)) {
    return false;
  }

  if (outRawBlock) {
    memcpy(outRawBlock, block, sizeof(block));
  }
  if (outType) {
    *outType = block[0];
  }
  return true;
}

bool writeTypeByte(const uint8_t *uid, uint8_t uidLen, uint8_t typeByte,
                   uint8_t *outWrittenBlock,
                   CardTypeAuthProfile *outAuthProfile) {
  CardTypeAuthProfile authProfile = CardTypeAuthProfile::UNKNOWN;
  if (!authenticateCardTypeBlock(uid, uidLen, &authProfile)) {
    Serial.println("writeTypeByte: auth failed on block 4.");
    return false;
  }

  uint8_t block[16] = {0};
  if (!g_nfc.mifareclassic_ReadDataBlock(kCardTypeBlock, block)) {
    Serial.println("writeTypeByte: initial read block 4 failed, trying blank write.");
    memset(block, 0, sizeof(block));

    // Read fail can halt card session; re-select before re-auth.
    uint8_t dummyUid[7] = {0};
    uint8_t dummyUidLen = 0;
    g_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, dummyUid, &dummyUidLen,
                              50);

    if (!authenticateCardTypeBlock(uid, uidLen, nullptr)) {
      Serial.println("writeTypeByte: re-auth after read failure failed.");
      return false;
    }
  }

  block[0] = typeByte;
  if (!g_nfc.mifareclassic_WriteDataBlock(kCardTypeBlock, block)) {
    Serial.println("writeTypeByte: write block 4 failed.");
    return false;
  }

  uint8_t verifyType = 0;
  uint8_t verifyBlock[16] = {0};
  if (!readTypeByte(uid, uidLen, &verifyType, verifyBlock)) {
    Serial.println("writeTypeByte: verify read block 4 failed.");
    return false;
  }

  if (verifyType != typeByte) {
    Serial.printf("writeTypeByte: verify mismatch expected=0x%02X got=0x%02X\n",
                  typeByte, verifyType);
    return false;
  }

  if (outWrittenBlock) {
    memcpy(outWrittenBlock, verifyBlock, sizeof(verifyBlock));
  }
  if (outAuthProfile) {
    *outAuthProfile = authProfile;
  }
  return true;
}

bool writeTenantVerifyBlock(const uint8_t *uid, uint8_t uidLen,
                            const char *tenant) {
  if (!tenant) {
    return false;
  }

  if (!authenticateTenantVerifyBlock(uid, uidLen)) {
    return false;
  }

  uint8_t block[16] = {0};
  size_t len = strnlen(tenant, sizeof(block));
  memcpy(block, tenant, len);

  if (!g_nfc.mifareclassic_WriteDataBlock(kTenantVerifyBlock, block)) {
    return false;
  }

  if (!authenticateTenantVerifyBlock(uid, uidLen)) {
    return false;
  }

  uint8_t verify[16] = {0};
  if (!g_nfc.mifareclassic_ReadDataBlock(kTenantVerifyBlock, verify)) {
    return false;
  }

  return memcmp(block, verify, sizeof(block)) == 0;
}

void armWrite(uint8_t typeByte) {
  g_pendingType = typeByte;
  g_pendingAction = PendingAction::WRITE_TYPE;
  Serial.printf("Ready to write type %s (0x%02X).\n", cardTypeToString(typeByte),
                typeByte);
  Serial.println("Tap a Mifare Classic card...");
}

void armRead() {
  g_pendingAction = PendingAction::READ_TYPE;
  Serial.println("Ready to read card type.");
  Serial.println("Tap a Mifare Classic card...");
}

void cancelPendingAction() {
  g_pendingAction = PendingAction::NONE;
  g_pendingType = 0;
  Serial.println("Pending action canceled.");
}

void normalize(char *text) {
  if (!text) {
    return;
  }

  // Trim left
  size_t start = 0;
  while (text[start] == ' ' || text[start] == '\t') {
    ++start;
  }
  if (start > 0) {
    memmove(text, text + start, strlen(text + start) + 1);
  }

  // Trim right
  size_t len = strlen(text);
  while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t' ||
                     text[len - 1] == '\r' || text[len - 1] == '\n')) {
    text[len - 1] = '\0';
    --len;
  }

  for (size_t i = 0; text[i] != '\0'; ++i) {
    text[i] = static_cast<char>(tolower(static_cast<unsigned char>(text[i])));
  }
}

void handleCommand(char *line) {
  normalize(line);
  if (line[0] == '\0') {
    return;
  }

  if (strcmp(line, "help") == 0) {
    printHelp();
    return;
  }

  if (strcmp(line, "read") == 0) {
    armRead();
    return;
  }

  if (strcmp(line, "setup") == 0 || strcmp(line, "kurulum") == 0 ||
      strcmp(line, "1") == 0) {
    armWrite(0x01);
    return;
  }

  if (strcmp(line, "customer") == 0 || strcmp(line, "musteri") == 0 ||
      strcmp(line, "2") == 0) {
    armWrite(0x02);
    return;
  }

  if (strcmp(line, "gift") == 0 || strcmp(line, "hediye") == 0 ||
      strcmp(line, "3") == 0) {
    armWrite(0x03);
    return;
  }

  if (strcmp(line, "service") == 0 || strcmp(line, "servis") == 0 ||
      strcmp(line, "4") == 0) {
    armWrite(0x04);
    return;
  }

  if (strcmp(line, "cancel") == 0) {
    cancelPendingAction();
    return;
  }

  Serial.print("Unknown command: ");
  Serial.println(line);
  printHelp();
}

void pollSerialInput() {
  while (Serial.available() > 0) {
    int value = Serial.read();
    if (value < 0) {
      return;
    }

    char c = static_cast<char>(value);
    if (c == '\n' || c == '\r') {
      if (g_inputLen > 0) {
        g_inputBuffer[g_inputLen] = '\0';
        handleCommand(g_inputBuffer);
        g_inputLen = 0;
      }
      continue;
    }

    if (g_inputLen < (kInputBufferSize - 1)) {
      g_inputBuffer[g_inputLen++] = c;
    }
  }
}

void processPendingAction() {
  if (g_pendingAction == PendingAction::NONE) {
    return;
  }

  if (!g_nfcReady) {
    Serial.println("PN532 is not ready.");
    cancelPendingAction();
    return;
  }

  uint8_t uid[7] = {0};
  uint8_t uidLen = 0;

  if (!g_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen,
                                 kTagDetectTimeoutMs)) {
    return;
  }

  char uidText[24] = {0};
  uidToString(uid, uidLen, uidText, sizeof(uidText));
  Serial.printf("Card detected. UID=%s, UID_LEN=%u\n", uidText, uidLen);

  if (g_pendingAction == PendingAction::READ_TYPE) {
    uint8_t type = 0;
    if (!readTypeByte(uid, uidLen, &type, nullptr)) {
      Serial.println("Read failed. Auth/read error on block 4.");
    } else {
      Serial.printf("Card type: %s (0x%02X)\n", cardTypeToString(type), type);
    }
    cancelPendingAction();
    return;
  }

  if (g_pendingAction == PendingAction::WRITE_TYPE) {
    CardTypeAuthProfile authProfile = CardTypeAuthProfile::UNKNOWN;
    if (!writeTypeByte(uid, uidLen, g_pendingType, nullptr, &authProfile)) {
      Serial.println("Write failed. Auth/read/write/verify error on block 4.");
      cancelPendingAction();
      return;
    }

    bool migratedKeys = false;
    if (!migrateCardTypeKeysIfNeeded(uid, uidLen, authProfile, &migratedKeys)) {
      Serial.println("Type written but key update failed on block 7.");
      cancelPendingAction();
      return;
    }
    if (migratedKeys) {
      Serial.println("Sector 1 keys updated: Key A+B FF -> AFBIBA.");
    }

    if (g_pendingType != 0x01) {
      if (!writeTenantVerifyBlock(uid, uidLen, kTenantName)) {
        Serial.println(
            "Type written but tenant verify block write failed (block 36).");
        cancelPendingAction();
        return;
      }
      Serial.printf("Tenant verify block written: block=%u tenant='%s'\n",
                    kTenantVerifyBlock, kTenantName);
    } else {
      Serial.println("Setup card selected: tenant verify block skipped.");
    }

    Serial.printf("Write success. New type: %s (0x%02X)\n",
                  cardTypeToString(g_pendingType), g_pendingType);
    cancelPendingAction();
  }
}
} // namespace

void setup() {
  Serial.begin(kSerialBaud);
  delay(300);
  Serial.println();
  Serial.println("Booting   Card Writer...");

  Serial2.begin(kPn532Baud, SERIAL_8N1, kPn532RxPin, kPn532TxPin);

  g_nfc.begin();
  const uint32_t version = g_nfc.getFirmwareVersion();
  if (!version) {
    Serial.println("PN532 not found.");
    Serial.println("Check HSU mode and TX/RX wiring.");
    g_nfcReady = false;
  } else {
    Serial.printf("PN532 FW: %lu.%lu.%lu.%lu\n", (version >> 24) & 0xFF,
                  (version >> 16) & 0xFF, (version >> 8) & 0xFF, version & 0xFF);
    if (!g_nfc.SAMConfig()) {
      Serial.println("SAMConfig failed.");
      g_nfcReady = false;
    } else {
      g_nfc.setPassiveActivationRetries(0xFF);
      g_nfcReady = true;
      Serial.println("PN532 ready.");
    }
  }

  printHelp();
}

void loop() {
  pollSerialInput();
  processPendingAction();
}
