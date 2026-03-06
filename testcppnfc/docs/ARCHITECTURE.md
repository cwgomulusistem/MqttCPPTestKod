# Funtoria Hibrit Mimari (Thin Abstraction Layer)

## İçindekiler

1. [Genel Bakış](#genel-bakış)
2. [Mimari Felsefesi](#mimari-felsefesi)
3. [Katman Yapısı](#katman-yapısı)
4. [Dosya Organizasyonu](#dosya-organizasyonu)
5. [Tasarım Desenleri](#tasarım-desenleri)
6. [Thread Safety](#thread-safety)
7. [Bellek Yönetimi](#bellek-yönetimi)
8. [Veri Akışı](#veri-akışı)

---

## Genel Bakış

Funtoria, ESP32 tabanlı bir NFC ödeme terminali yazılımıdır. **Hibrit Mimari (Thin Abstraction Layer)** prensibiyle tasarlanmıştır.

### Temel Özellikler

- **PN532 NFC Okuyucu** (I2C üzerinden)
- **Mifare Classic 1K/4K** kart desteği
- **Mobil Ödeme (HCE)** - Android/iOS telefonlar
- **FreeRTOS** task yönetimi
- **RAII** ile otomatik kaynak yönetimi
- **Deadlock-free** thread safety

### Teknoloji Stack

| Bileşen | Teknoloji |
|---------|-----------|
| MCU | ESP32 (Dual Core 240MHz) |
| RTOS | FreeRTOS |
| Framework | Arduino (ESP32) |
| Dil | C++17 + C-style structs |
| Build | PlatformIO |
| NFC | Adafruit PN532 |

---

## Mimari Felsefesi

### "C++ Güvenliği + C Performansı"

```
+-------------------------------------------------------------+
|                    APPLICATION LAYER                         |
|                      (main.cpp)                              |
+-------------------------------------------------------------+
|                    SERVICE LAYER (C++)                       |
|   NfcService class - C struct + Mutex RAII birleşimi         |
+-------------------------------------------------------------+
|                    SYSTEM LAYER (C++)                        |
|   OsWrappers.h - Mutex, ScopedLock, Task RAII wrappers       |
+-------------------------------------------------------------+
|                    DRIVER LAYER (Pure C)                     |
|   PN532Driver - typedef struct + namespace functions         |
+-------------------------------------------------------------+
|                    TYPES LAYER (Pure C)                      |
|   NfcTypes.h, Config.h - enum, struct tanımları              |
+-------------------------------------------------------------+
```

### Neden Hibrit?

| Katman | Stil | Neden |
|--------|------|-------|
| Driver | Pure C | Vtable yok, her byte'ın yeri belli, maksimum performans |
| System | C++ RAII | Otomatik kaynak yönetimi, deadlock riski sıfır |
| Service | Hibrit | C struct veri + C++ Mutex güvenliği |

### Karşılaştırma

| Özellik | Saf C++ STL | Saf C | Hibrit (Biz) |
|---------|-------------|-------|--------------|
| RAM Kullanımı | Yüksek (~10KB) | Düşük (~5KB) | Düşük (~5KB) |
| Mutex Güvenliği | Manuel | Manuel (riskli) | Otomatik (RAII) |
| vtable Overhead | Var | Yok | Yok |
| Deadlock Riski | Yüksek | Yüksek | SIFIR |
| Kod Güvenliği | Orta | Düşük | Yüksek |

---

## Katman Yapısı

### 1. Types Layer (Pure C)

Temel veri yapıları ve enum tanımları.

**Config.h** - Sistem sabitleri:
```cpp
typedef struct {
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint32_t frequency;
} I2CConfig;

typedef struct {
    I2CConfig i2c;
    NFCConfig nfc;
    MifareConfig mifare;
    SystemConfig system;
    uint8_t log_level;
} AppConfig;

// Constructor yerine init fonksiyonu
static inline void Config_setDefaults(AppConfig* config);
```

**NfcTypes.h** - NFC veri tipleri:
```cpp
// Detaylı hata kodları (bool yerine)
typedef enum {
    NFC_SUCCESS = 0,
    NFC_ERR_NOT_INITIALIZED,
    NFC_ERR_TIMEOUT,
    NFC_ERR_COMMUNICATION,
    NFC_ERR_AUTH_FAILED,
    NFC_ERR_NO_TAG,
    // ...
} NfcResult;

// Fixed-size struct (heap yok)
typedef struct {
    uint8_t uid[10];
    uint8_t uid_length;
    NfcTagType type;
} NfcTagInfo;
```

**Logger.h** - Makro tabanlı loglama:
```cpp
#if LOG_LEVEL >= LOG_LEVEL_ERROR
    #define LOG_E(fmt, ...) Serial.printf("[E] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_E(...)  // Release modda tamamen kaldırılır
#endif
```

---

### 2. System Layer (C++ RAII)

FreeRTOS objelerini saran hafif C++ wrappers.

**OsWrappers.h** (Header-only, zero overhead):

```cpp
class Mutex {
public:
    Mutex() { _handle = xSemaphoreCreateMutex(); }
    ~Mutex() { vSemaphoreDelete(_handle); }
    
    bool lock(TickType_t timeout = portMAX_DELAY);
    void unlock();
    
    // EN ÖNEMLİ KISIM - Scope-based locking
    class ScopedLock {
    public:
        explicit ScopedLock(Mutex& m) : _mutex(m), _locked(m.lock()) {}
        ~ScopedLock() { if (_locked) _mutex.unlock(); }
        
        bool isLocked() const { return _locked; }
    };
};

class Task {
public:
    Task() = default;
    ~Task() { stop(); }  // RAII - otomatik cleanup
    
    bool start(const char* name, TaskFunction_t func, void* param, ...);
    void stop();
};
```

**Kullanım:**
```cpp
void myFunction() {
    Mutex::ScopedLock lock(myMutex);
    if (!lock.isLocked()) return;  // Timeout kontrolü
    
    // ... güvenli işlemler ...
    
}  // Scope bitince OTOMATİK unlock - exception, return ne olursa olsun
```

---

### 3. Driver Layer (Pure C)

Donanım sürücüleri - typedef struct + namespace fonksiyonları.

**PN532Driver.h:**
```cpp
// Saf C struct - Mutex YOK (Service katmanı yönetir)
typedef struct {
    Adafruit_PN532* hw;
    NfcTagInfo current_tag;
    ApduBuffer* apdu_buffer;
    bool initialized;
} PN532DriverState;

// Namespace fonksiyonları
namespace PN532Driver {
    NfcResult init(PN532DriverState* state, Adafruit_PN532* hw, ApduBuffer* buffer);
    NfcResult waitForTag(PN532DriverState* state, uint16_t timeout_ms);
    NfcResult authenticateMifare(PN532DriverState* state, uint8_t block, const uint8_t* key, bool use_key_a);
    NfcResult readMifareBlock(PN532DriverState* state, uint8_t block, uint8_t* buffer);
    // ...
}
```

**Neden Mutex driver'da değil?**
- Separation of Concerns - driver sadece donanımla konuşur
- Tek sorumluluk prensibi
- Service katmanı thread safety'yi yönetir

---

### 4. Service Layer (Hibrit C++)

C struct + C++ RAII birleşimi.

**NfcService.h:**
```cpp
class NfcService {
private:
    // C-Style state (Composition)
    PN532DriverState _driverState;
    
    // C++ RAII objeler
    Mutex _driverLock;
    Task _task;
    
    // C-style callback
    OnCardReadCallback _callback;
    
public:
    NfcResult init(Adafruit_PN532* hw, ApduBuffer* buffer);
    void setCallback(OnCardReadCallback cb);
    NfcResult start();
    void stop();
    
    // Thread-safe public API
    NfcResult readTagSafe(uint16_t timeout_ms);
    NfcResult readMifareBlockSafe(uint8_t block, const uint8_t* key, uint8_t* buffer);
};
```

---

## Dosya Organizasyonu

```
Funtoria/src/
├── config/
│   ├── Config.h          # typedef struct - sistem sabitleri
│   └── Logger.h          # LOG_E/W/I/D makroları
├── drivers/
│   ├── NfcTypes.h        # enum, struct tanımları
│   ├── PN532Driver.h     # C-style struct + namespace
│   └── PN532Driver.cpp
├── system/
│   └── OsWrappers.h      # Mutex, Task RAII (header-only)
├── services/
│   ├── NfcService.h      # Hibrit C++ class
│   └── NfcService.cpp
├── logic/
│   ├── MifareCardHandler.h   # C-style namespace
│   └── MifareCardHandler.cpp
└── main.cpp
```

---

## Tasarım Desenleri

### 1. RAII (Resource Acquisition Is Initialization)

Kaynaklar constructor'da alınır, destructor'da bırakılır.

```cpp
// OsWrappers.h
class Mutex {
public:
    Mutex() { _handle = xSemaphoreCreateMutex(); }   // Constructor: oluştur
    ~Mutex() { vSemaphoreDelete(_handle); }          // Destructor: sil
};
```

### 2. Scoped Locking (Kritik)

Fonksiyon bitince mutex OTOMATİK açılır.

```cpp
void NfcService::processLoop() {
    {  // Scope başlangıcı
        Mutex::ScopedLock lock(_driverLock);
        // ... işlemler ...
    }  // Scope sonu -> UNLOCK OTOMATİK
}
```

### 3. Unlock Before Callback

Callback çağrılmadan önce mutex serbest bırakılır.

```cpp
void NfcService::processLoop() {
    NfcCardData cardData;
    bool cardDetected = false;
    
    {  // MUTEX KİLİTLİ
        Mutex::ScopedLock lock(_driverLock);
        if (PN532Driver::waitForTag(&_driverState, 500) == NFC_SUCCESS) {
            // Veriyi YEREL değişkene kopyala
            memcpy(&cardData, ...);
            cardDetected = true;
        }
    }  // MUTEX AÇILDI
    
    // Callback kilit AÇIKKEN çağrılır
    if (cardDetected && _callback) {
        _callback(&cardData);  // Ne kadar uzun sürerse sürsün OK
    }
}
```

**Neden önemli?**
- Callback içinde uzun işlem yapılabilir (WiFi, MQTT)
- Deadlock riski yok
- Diğer task'lar driver'a erişebilir

### 4. Composition over Inheritance

Service, driver'ı miras almak yerine içinde barındırır.

```cpp
class NfcService {
private:
    PN532DriverState _driverState;  // Composition - içeride
    // DEĞİL: class NfcService : public PN532Driver
};
```

---

## Thread Safety

### Mutex::ScopedLock Kullanımı

```cpp
// YANLIŞ - Manuel lock/unlock (unutma riski)
_mutex.lock();
// ... işlem ...
if (error) return;  // BUG! Mutex açılmadı
_mutex.unlock();

// DOĞRU - ScopedLock (otomatik)
{
    Mutex::ScopedLock lock(_mutex);
    // ... işlem ...
    if (error) return;  // OK! Destructor unlock çağırır
}
```

### Thread-Safe Public API

```cpp
// Service dışından güvenli erişim
NfcResult NfcService::readMifareBlockSafe(uint8_t block, const uint8_t* key, uint8_t* buffer) {
    Mutex::ScopedLock lock(_driverLock, pdMS_TO_TICKS(500));
    if (!lock.isLocked()) return NFC_ERR_TIMEOUT;
    
    NfcResult res = PN532Driver::authenticateMifare(&_driverState, block, key, true);
    if (res != NFC_SUCCESS) return res;
    
    return PN532Driver::readMifareBlock(&_driverState, block, buffer);
}
```

---

## Bellek Yönetimi

### Global Objeler (BSS Section)

```cpp
// main.cpp - Heap allocation YOK
Adafruit_PN532 g_pn532Hw(PN532_IRQ, PN532_RESET);
ApduBuffer g_apduBuffer;           // 262 bytes
NfcService g_nfcService;           // ~100 bytes
AppConfig g_appConfig;             // ~50 bytes
```

### Bellek Kullanımı

```
RAM:   6.8% (22,376 / 327,680 bytes)
Flash: 23.0% (301,877 / 1,310,720 bytes)
```

| Bileşen | Boyut |
|---------|-------|
| Adafruit_PN532 | ~300 bytes |
| ApduBuffer | ~262 bytes |
| NfcService (+ Mutex + Task) | ~150 bytes |
| AppConfig | ~50 bytes |
| NfcTask stack | 4096 bytes |
| **Toplam** | **~5 KB** |

---

## Veri Akışı

```
┌──────────┐     ┌──────────┐     ┌──────────────┐     ┌──────────┐
│   Kart   │────▶│  PN532   │────▶│  NfcService  │────▶│ Callback │
│ /Telefon │     │ Hardware │     │   (Task)     │     │          │
└──────────┘     └──────────┘     └──────────────┘     └──────────┘
                      │                  │                   │
                      ▼                  ▼                   ▼
                 ┌─────────┐       ┌──────────┐       ┌───────────┐
                 │   I2C   │       │  Mutex   │       │ İş Mantığı│
                 │   Bus   │       │ ScopedLock│       │ (User)    │
                 └─────────┘       └──────────┘       └───────────┘
```

### Detaylı Akış

1. **Kart/Telefon RF alanına girer**
2. **NfcService.processLoop()** çalışır (FreeRTOS task)
3. **Mutex::ScopedLock** ile driver kilitlenir
4. **PN532Driver::waitForTag()** tag algılar
5. **Veri yerel değişkene kopyalanır**
6. **Scope biter → Mutex OTOMATİK açılır**
7. **Callback çağrılır** (kilit açık)
8. **Kullanıcı iş mantığı çalışır**

---

## Gelecek Geliştirmeler

- [ ] **MqttService** - Sunucu haberleşmesi
- [ ] **DisplayService** - Ekran kontrolü (Nextion/TFT)
- [ ] **LedBuzzerService** - Görsel/işitsel geri bildirim
- [ ] **OtaService** - Uzaktan güncelleme
- [ ] **SecureElement** - Anahtar yönetimi

---

## Kaynaklar

- [FreeRTOS Resmi Dokümantasyon](https://www.freertos.org/Documentation)
- [ESP32 Arduino Core](https://docs.espressif.com/projects/arduino-esp32)
- [Adafruit PN532 Kütüphanesi](https://github.com/adafruit/Adafruit-PN532)
- [ISO 14443 Standartları](https://en.wikipedia.org/wiki/ISO/IEC_14443)

---

*Funtoria Embedded Team - 2026*
*Hibrit Mimari v2.0.0*
