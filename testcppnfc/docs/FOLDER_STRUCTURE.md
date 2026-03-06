# Funtoria - Klasör Yapısı Referansı

## Tam Klasör Ağacı

```
Funtoria/
│
├── platformio.ini           # PlatformIO build konfigürasyonu
├── .gitignore               # Git ignore kuralları
│
├── docs/                    # DOKÜMANTASYON
│   ├── ARCHITECTURE.md      # Hibrit mimari açıklaması
│   ├── GETTING_STARTED.md   # Hızlı başlangıç rehberi
│   ├── FOLDER_STRUCTURE.md  # Bu dosya
│   └── HARDWARE_WIRING.md   # Donanım bağlantı şeması
│
├── src/                     # KAYNAK KODLAR
│   │
│   ├── main.cpp             # Ana giriş noktası
│   │                        # - Global objeler (BSS section)
│   │                        # - setup(): Sistem başlatma
│   │                        # - Callback fonksiyonları
│   │
│   ├── config/              # KONFİGÜRASYON (Pure C)
│   │   ├── Config.h         # typedef struct ile sistem sabitleri
│   │   │                    # - I2CConfig, NFCConfig, AppConfig
│   │   │                    # - Config_setDefaults() fonksiyonu
│   │   │                    # - Pin tanımları (#define)
│   │   │
│   │   └── Logger.h         # Makro tabanlı loglama
│   │                        # - LOG_E, LOG_W, LOG_I, LOG_D
│   │                        # - Release modda devre dışı
│   │
│   ├── drivers/             # SÜRÜCÜLER (Pure C)
│   │   ├── NfcTypes.h       # Ortak veri tipleri
│   │   │                    # - NfcResult enum (hata kodları)
│   │   │                    # - NfcTagType enum
│   │   │                    # - NfcTagInfo struct
│   │   │                    # - NfcCardData struct
│   │   │
│   │   ├── PN532Driver.h    # PN532 driver header
│   │   │                    # - PN532DriverState struct
│   │   │                    # - namespace fonksiyon deklarasyonları
│   │   │
│   │   └── PN532Driver.cpp  # PN532 driver implementasyonu
│   │                        # - init(), waitForTag()
│   │                        # - authenticateMifare()
│   │                        # - readMifareBlock(), writeMifareBlock()
│   │                        # - sendApdu() (mobil ödeme)
│   │
│   ├── system/              # SİSTEM ALTYAPISI (C++ RAII)
│   │   └── OsWrappers.h     # FreeRTOS RAII wrappers (header-only)
│   │                        # - Mutex class + ScopedLock
│   │                        # - Task class
│   │                        # - Zero overhead (inline)
│   │
│   ├── services/            # SERVİSLER (Hibrit C++)
│   │   ├── NfcService.h     # NFC servis header
│   │   │                    # - NfcService class
│   │   │                    # - C struct + Mutex RAII
│   │   │                    # - OnCardReadCallback typedef
│   │   │
│   │   └── NfcService.cpp   # NFC servis implementasyonu
│   │                        # - processLoop() - Unlock Before Callback
│   │                        # - Thread-safe public API
│   │                        # - Task yönetimi
│   │
│   └── logic/               # İŞ MANTIĞI (Pure C)
│       ├── MifareCardHandler.h   # Mifare işlemleri header
│       │                    # - MifareOpConfig struct
│       │                    # - namespace fonksiyon deklarasyonları
│       │
│       └── MifareCardHandler.cpp # Mifare işlemleri impl
│                            # - process(), readBalance()
│                            # - writeBalance()
│
├── lib/                     # HARİCİ KÜTÜPHANELER
│   └── (PlatformIO otomatik)# Adafruit PN532, BusIO
│
├── include/                 # GLOBAL HEADER'LAR
│   └── (Boş - gerektiğinde)
│
└── test/                    # UNIT TESTLER
    └── (Gelecek geliştirme)
```

---

## Klasör Detayları

### `src/config/`

| Dosya | Stil | Açıklama |
|-------|------|----------|
| `Config.h` | Pure C | typedef struct ile sistem sabitleri |
| `Logger.h` | Pure C | Makro tabanlı loglama (release'de devre dışı) |

**Config.h İçeriği:**
```
I2CConfig       - I2C pin ve frekans ayarları
NFCConfig       - NFC timeout ve retry ayarları
MifareConfig    - Mifare anahtarları ve blok numaraları
SystemConfig    - Task stack ve öncelik ayarları
AppConfig       - Tüm ayarları birleştiren ana struct
```

**Logger.h Makroları:**
```
LOG_E(fmt, ...)  - Error (LOG_LEVEL >= 1)
LOG_W(fmt, ...)  - Warning (LOG_LEVEL >= 2)
LOG_I(fmt, ...)  - Info (LOG_LEVEL >= 3)
LOG_D(fmt, ...)  - Debug (LOG_LEVEL >= 4)
LOG_HEX(label, data, len) - Hex dump (DEBUG)
```

---

### `src/drivers/`

| Dosya | Stil | Açıklama |
|-------|------|----------|
| `NfcTypes.h` | Pure C | enum ve struct tanımları |
| `PN532Driver.h` | Pure C | Driver state struct + namespace |
| `PN532Driver.cpp` | Pure C | Driver implementasyonu |

**NfcTypes.h Tipleri:**
```
NfcResult       - Detaylı hata kodları (bool yerine)
NfcTagType      - Tag tipleri (Mifare, ISO14443-4, vb.)
NfcState        - Durum makinesi
NfcTagInfo      - Tag bilgisi (fixed-size)
NfcCardData     - Callback'e gönderilen veri
ApduBuffer      - APDU iletişim tamponu
```

**PN532Driver Fonksiyonları:**
```
init()              - Driver başlatma (memset ile zero-init)
waitForTag()        - Tag algılama
authenticateMifare() - Sektör doğrulama
readMifareBlock()   - Blok okuma
writeMifareBlock()  - Blok yazma
sendApdu()          - ISO14443-4 APDU değişimi
```

---

### `src/system/`

| Dosya | Stil | Açıklama |
|-------|------|----------|
| `OsWrappers.h` | C++ RAII | FreeRTOS wrappers (header-only) |

**OsWrappers.h Sınıfları:**
```
Mutex           - FreeRTOS mutex RAII wrapper
├── lock()      - Kilitle
├── unlock()    - Aç
└── ScopedLock  - Otomatik lock/unlock (EN ÖNEMLİ)

Task            - FreeRTOS task RAII wrapper
├── start()     - Task başlat
├── stop()      - Task durdur (destructor'da da çağrılır)
└── handle()    - TaskHandle_t al
```

---

### `src/services/`

| Dosya | Stil | Açıklama |
|-------|------|----------|
| `NfcService.h` | Hibrit C++ | C struct + Mutex RAII |
| `NfcService.cpp` | Hibrit C++ | Unlock Before Callback pattern |

**NfcService Özellikleri:**
```
- PN532DriverState içerir (Composition)
- Mutex ile thread-safe
- Task ile FreeRTOS entegrasyonu
- C-style callback (std::function yerine)
- Unlock Before Callback pattern
```

**Public API:**
```
init()              - Servis başlatma
setCallback()       - Kart okunduğunda çağrılacak fonksiyon
start()             - Task başlat
stop()              - Task durdur
readTagSafe()       - Thread-safe tag okuma
readMifareBlockSafe() - Thread-safe blok okuma
```

---

### `src/logic/`

| Dosya | Stil | Açıklama |
|-------|------|----------|
| `MifareCardHandler.h` | Pure C | Mifare işlemleri header |
| `MifareCardHandler.cpp` | Pure C | Mifare işlemleri impl |

**MifareHandler Fonksiyonları:**
```
initConfig()    - MifareOpConfig'i varsayılanlarla başlat
process()       - Kart işleme (auth + okuma)
readBalance()   - Bakiye okuma
writeBalance()  - Bakiye yazma
```

---

## Dosya Bağımlılık Grafiği

```
                    main.cpp
                       │
         ┌─────────────┼─────────────┐
         ▼             ▼             ▼
    Config.h      NfcService    MifareHandler
         │             │             │
         │      ┌──────┴──────┐      │
         │      ▼             ▼      │
         │   Mutex        PN532Driver
         │   Task              │
         │      │              │
         └──────┴──────────────┘
                    │
                    ▼
               NfcTypes.h
```

---

## Eski vs Yeni Yapı Karşılaştırması

### Silinen Dosyalar (Eski Mimari)

```
src/interfaces/
├── INfcReader.h        # KALDIRILDI - virtual function overhead
└── INfcTagHandler.h    # KALDIRILDI - gereksiz soyutlama

src/system/
├── TaskBase.h/.cpp     # KALDIRILDI - OsWrappers.h ile değiştirildi
└── I2CMutex.h/.cpp     # KALDIRILDI - Mutex class ile değiştirildi

src/services/
└── AppTask.h/.cpp      # KALDIRILDI - gereksiz katman

src/utils/
└── Logger.h/.cpp       # KALDIRILDI - makro tabanlı Logger.h ile değişti

src/logic/
└── MobilePaymentHandler.h  # KALDIRILDI - NfcTypes.h'a taşındı
```

### Yeni Dosyalar (Hibrit Mimari)

```
src/drivers/
└── NfcTypes.h          # YENİ - ortak enum/struct tanımları

src/system/
└── OsWrappers.h        # YENİ - RAII wrappers (header-only)

src/config/
└── Logger.h            # YENİ - makro tabanlı (Logger.cpp yok)

src/logic/
└── MifareCardHandler.cpp  # YENİ - implementasyon dosyası
```

---

## Yeni Dosya Ekleme Rehberi

### Yeni NFC Okuyucu Eklemek

1. `src/drivers/` altında dosya oluştur:
   - `NewReader.h` - typedef struct + namespace
   - `NewReader.cpp`
2. `PN532Driver` ile aynı fonksiyon imzalarını kullan
3. `main.cpp`'de driver'ı değiştir

### Yeni Servis Eklemek

1. `src/services/` altında dosya oluştur:
   - `NewService.h`
   - `NewService.cpp`
2. `NfcService` yapısını örnek al:
   - State struct içer (Composition)
   - Mutex kullan (ScopedLock)
   - Task kullan (RAII)
3. `main.cpp`'de başlat

### Yeni Logic Handler Eklemek

1. `src/logic/` altında dosya oluştur:
   - `NewHandler.h`
   - `NewHandler.cpp`
2. namespace fonksiyonları kullan
3. `NfcService` veya `main.cpp`'den çağır

---

## Bellek Etkisi

| Eski Mimari | Yeni Mimari | Fark |
|-------------|-------------|------|
| ~10 KB RAM | ~5 KB RAM | -50% |
| Heap fragmentation VAR | Heap fragmentation YOK | ✓ |
| vtable overhead VAR | vtable overhead YOK | ✓ |

---

*Funtoria Embedded Team - 2026*
*Hibrit Mimari v2.0.0*
