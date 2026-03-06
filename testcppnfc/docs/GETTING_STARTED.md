# Funtoria - Hızlı Başlangıç Rehberi

## Gereksinimler

### Donanım
- ESP32 DevKit v1 (veya uyumlu)
- PN532 NFC Modülü (I2C modu)
- USB Kablosu

### Yazılım
- [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) (VS Code eklentisi)
- veya [PlatformIO Core](https://platformio.org/install/cli) (CLI)

---

## Donanım Bağlantısı

### PN532 - ESP32 Bağlantı Şeması

```
PN532 Modülü          ESP32 DevKit
┌──────────┐          ┌──────────┐
│   VCC    │──────────│   3.3V   │
│   GND    │──────────│   GND    │
│   SDA    │──────────│  GPIO21  │
│   SCL    │──────────│  GPIO22  │
└──────────┘          └──────────┘
```

### PN532 I2C Modu Ayarı

PN532 modülündeki DIP switch'leri ayarlayın:

| Switch 1 | Switch 2 | Mod |
|----------|----------|-----|
| OFF | ON | **I2C** |
| ON | OFF | SPI |
| ON | ON | HSU |

---

## Kurulum

### 1. Projeyi Klonla

```bash
git clone <repo-url> Funtoria
cd Funtoria/Funtoria
```

### 2. Bağımlılıkları Yükle

```bash
pio lib install
```

### 3. Derleme

```bash
pio run
```

### 4. Yükleme

```bash
pio run --target upload
```

### 5. Seri Monitör

```bash
pio device monitor --baud 115200
```

---

## Beklenen Çıktı

```
==============================================
        FUNTORIA EMBEDDED SYSTEM v2.0.0
        NFC Payment Terminal (Hybrid)
==============================================

[I] I2C initialized (SDA:21, SCL:22)
[I] PN532 found! Firmware: 1.6
[I] NfcService initialized
[I] FreeRTOS task started: NfcTask (Core 0)

==============================================
  System ready. FreeRTOS running.
  Waiting for card or mobile...
==============================================
```

---

## Kod Yapısı

### main.cpp Temel Yapı

```cpp
#include "config/Config.h"
#include "config/Logger.h"
#include "drivers/PN532Driver.h"
#include "services/NfcService.h"

// Global objeler (BSS section - heap yok)
Adafruit_PN532 g_pn532Hw(PN532_IRQ, PN532_RESET);
ApduBuffer g_apduBuffer;
NfcService g_nfcService;
AppConfig g_appConfig;

// Callback fonksiyonu
void onCardRead(const NfcCardData* data) {
    LOG_I("Kart okundu! UID: ");
    for (int i = 0; i < data->tag_info.uid_length; i++) {
        Serial.printf("%02X", data->tag_info.uid[i]);
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    
    // I2C başlat
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_CLOCK_SPEED);
    
    // NfcService başlat
    if (g_nfcService.init(&g_pn532Hw, &g_apduBuffer) != NFC_SUCCESS) {
        LOG_E("NfcService init failed!");
        return;
    }
    
    // Callback ayarla
    g_nfcService.setCallback(onCardRead);
    
    // Task başlat
    g_nfcService.start();
}

void loop() {
    vTaskDelete(NULL);  // loop() kullanılmaz
}
```

---

## Test Etme

### Mifare Classic Kart Testi

1. Bir Mifare Classic 1K kartı PN532'ye yaklaştırın
2. Beklenen çıktı:

```
[I] Kart okundu! UID: 04A1B2C3
[I] Tag tipi: MIFARE_CLASSIC
[D] Doğrulama başarılı
[I] Blok 4 okundu
```

### Mobil Telefon Testi (HCE)

1. Android telefonda NFC'yi açın
2. Funtoria uygulamasını yükleyin (gerekli)
3. Telefonu PN532'ye yaklaştırın

```
[I] Kart okundu! UID: 08RANDOM
[I] Tag tipi: ISO14443_4
[D] SELECT AID gönderiliyor...
[I] Funtoria uygulaması seçildi!
```

---

## Konfigürasyon Değişiklikleri

### Pin Değiştirme

`src/config/Config.h` dosyasını düzenleyin:

```cpp
// I2C Pinleri
#define I2C_SDA_PIN     21    // Değiştirin
#define I2C_SCL_PIN     22    // Değiştirin

// PN532 Pinleri
#define PN532_IRQ       -1    // Kullanılmıyor
#define PN532_RESET     -1    // Kullanılmıyor
```

### Log Seviyesi

`src/config/Config.h` dosyasında:

```cpp
// Log seviyeleri (0: kapalı, 4: debug)
#define LOG_LEVEL       LOG_LEVEL_DEBUG  // Geliştirme
// #define LOG_LEVEL    LOG_LEVEL_INFO   // Üretim
// #define LOG_LEVEL    LOG_LEVEL_NONE   // Sessiz
```

### Mifare Anahtarı

`src/config/Config.h` dosyasında:

```cpp
// Mifare anahtarları
#define MIFARE_KEY_FUNTORIA  {0x6F, 0x7A, 0x76, 0x65, 0x72, 0x69}
#define MIFARE_KEY_DEFAULT   {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
```

---

## API Kullanımı

### NfcService API

```cpp
// Başlatma
NfcResult result = g_nfcService.init(&hardware, &buffer);
if (result != NFC_SUCCESS) {
    LOG_E("Init hatası: %s", NfcResult_toString(result));
}

// Callback ayarlama
g_nfcService.setCallback(myCallbackFunction);

// Task başlatma/durdurma
g_nfcService.start();
g_nfcService.stop();

// Thread-safe okumalar
NfcResult res = g_nfcService.readTagSafe(500);
NfcResult res = g_nfcService.readMifareBlockSafe(4, key, buffer);
```

### PN532Driver API (Doğrudan kullanım)

```cpp
// Not: Direkt kullanım yerine NfcService tercih edilmeli
// Thread-safety garanti edilmez!

PN532DriverState state;
PN532Driver::init(&state, &hw, &buffer);

if (PN532Driver::waitForTag(&state, 1000) == NFC_SUCCESS) {
    // Tag algılandı
    NfcTagInfo* tag = &state.current_tag;
}
```

### MifareHandler API

```cpp
MifareOpConfig config;
MifareHandler::initConfig(&config);

// Bakiye okuma
uint32_t balance;
NfcResult res = MifareHandler::readBalance(&driverState, &config, &balance);

// Bakiye yazma
res = MifareHandler::writeBalance(&driverState, &config, newBalance);
```

---

## Hata Kodları

| Kod | İsim | Açıklama |
|-----|------|----------|
| 0 | NFC_SUCCESS | Başarılı |
| 1 | NFC_ERR_NOT_INITIALIZED | Driver başlatılmamış |
| 2 | NFC_ERR_TIMEOUT | Zaman aşımı |
| 3 | NFC_ERR_COMMUNICATION | I2C iletişim hatası |
| 4 | NFC_ERR_CRC | CRC hatası |
| 5 | NFC_ERR_AUTH_FAILED | Mifare doğrulama başarısız |
| 6 | NFC_ERR_NO_TAG | Tag algılanmadı |
| 7 | NFC_ERR_TAG_LOST | Tag kayboldu |
| 8 | NFC_ERR_INVALID_PARAM | Geçersiz parametre |
| 9 | NFC_ERR_BUFFER_OVERFLOW | Buffer taşması |
| 10 | NFC_ERR_UNSUPPORTED | Desteklenmeyen işlem |

---

## Sorun Giderme

### PN532 Bulunamadı

```
[E] PN532 not found!
```

**Çözüm:**
1. Kablo bağlantılarını kontrol edin
2. PN532 DIP switch'lerinin I2C modunda olduğundan emin olun
3. 3.3V güç kaynağını kontrol edin

### Mutex Timeout

```
[W] Mutex lock timeout
```

**Çözüm:**
1. Callback fonksiyonu çok uzun sürmüyor mu kontrol edin
2. Başka bir task driver'ı bloke etmiyor mu kontrol edin
3. Stack overflow kontrolü yapın

### Kart Okunamıyor

```
[W] Auth failed!
```

**Çözüm:**
1. Kart Mifare Classic 1K/4K mi kontrol edin
2. `Config.h` içindeki anahtar ayarlarını kontrol edin
3. Kartın hasarlı olmadığından emin olun

---

## Sonraki Adımlar

1. [ARCHITECTURE.md](ARCHITECTURE.md) - Detaylı hibrit mimari dokümantasyonu
2. [HARDWARE_WIRING.md](HARDWARE_WIRING.md) - Detaylı donanım bağlantısı
3. Kendi callback fonksiyonunuzu yazın
4. MQTT/WiFi entegrasyonu ekleyin

---

*Funtoria Embedded Team - 2026*
*Hibrit Mimari v2.0.0*
