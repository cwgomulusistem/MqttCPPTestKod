# PN532 & ESP32-WROOM Bağlantı Şeması

## İçindekiler

1. [Gerekli Malzemeler](#gerekli-malzemeler)
2. [PN532 Modül Tanıtımı](#pn532-modül-tanıtımı)
3. [ESP32-WROOM Pin Diyagramı](#esp32-wroom-pin-diyagramı)
4. [Bağlantı Şeması](#bağlantı-şeması)
5. [I2C Modu Ayarı](#i2c-modu-ayarı)
6. [Detaylı Pin Açıklamaları](#detaylı-pin-açıklamaları)
7. [Breadboard Bağlantısı](#breadboard-bağlantısı)
8. [PCB Tasarım Notları](#pcb-tasarım-notları)
9. [Sorun Giderme](#sorun-giderme)

---

## Gerekli Malzemeler

| Malzeme | Adet | Açıklama |
|---------|------|----------|
| ESP32-WROOM-32 DevKit | 1 | 30 veya 38 pin versiyon |
| PN532 NFC Modülü | 1 | Elechouse veya uyumlu |
| Jumper Kablo (Dişi-Dişi) | 4 | Bağlantı için |
| Breadboard | 1 | Prototipleme için |
| USB Kablo (Micro/Type-C) | 1 | Programlama ve güç |

---

## PN532 Modül Tanıtımı

```
                    PN532 NFC MODÜL (Üstten Görünüm)
    ┌─────────────────────────────────────────────────────────┐
    │                                                         │
    │    ┌─────────────────────────────────────────────┐     │
    │    │                                             │     │
    │    │              NXP PN532                      │     │
    │    │           NFC Controller                   │     │
    │    │                                             │     │
    │    └─────────────────────────────────────────────┘     │
    │                                                         │
    │    ┌─────┐  ┌─────┐                                    │
    │    │ SW1 │  │ SW2 │   ◄── DIP Switch (Mod Seçimi)      │
    │    │ OFF │  │ ON  │       I2C için bu ayar             │
    │    └─────┘  └─────┘                                    │
    │                                                         │
    │    ╔═══════════════════════════════════════════════╗   │
    │    ║  ┌────────────────────────────────────────┐   ║   │
    │    ║  │           NFC ANTENİ                  │   ║   │
    │    ║  │        (13.56 MHz PCB Coil)          │   ║   │
    │    ║  │                                       │   ║   │
    │    ║  └────────────────────────────────────────┘   ║   │
    │    ╚═══════════════════════════════════════════════╝   │
    │                                                         │
    └──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┘
       │  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │
      VCC GND SDA SCL IRQ RST MISO MOSI SCK SS  NC  NC  NC  NC

       ▲   ▲   ▲   ▲
       │   │   │   │
       └───┴───┴───┴──── I2C Bağlantısı için kullanılacak pinler
```

### PN532 Pin Tablosu

| Pin | İsim | Açıklama | I2C'de Kullanım |
|-----|------|----------|-----------------|
| 1 | VCC | Güç girişi (3.3V - 5V) | ✅ Gerekli |
| 2 | GND | Toprak | ✅ Gerekli |
| 3 | SDA | I2C Data / SPI MOSI | ✅ Gerekli |
| 4 | SCL | I2C Clock / SPI SCK | ✅ Gerekli |
| 5 | IRQ | Interrupt çıkışı | ❌ Opsiyonel |
| 6 | RST | Reset girişi | ❌ Opsiyonel |
| 7 | MISO | SPI Data Out | ❌ Kullanılmaz |
| 8 | MOSI | SPI Data In | ❌ Kullanılmaz |
| 9 | SCK | SPI Clock | ❌ Kullanılmaz |
| 10 | SS | SPI Chip Select | ❌ Kullanılmaz |

---

## ESP32-WROOM Pin Diyagramı

```
                      ESP32-WROOM-32 DevKit V1
                         (38 Pin Versiyon)
    
                              USB Port
                            ┌────────┐
                       ┌────┤  USB   ├────┐
                       │    └────────┘    │
               EN ─────┤ [1]        [38] ├───── VIN
          GPIO36/VP ───┤ [2]        [37] ├───── GND
          GPIO39/VN ───┤ [3]        [36] ├───── 3V3 ◄── Güç Çıkışı
            GPIO34 ────┤ [4]        [35] ├───── GPIO23
            GPIO35 ────┤ [5]        [34] ├───── GPIO22 ◄── I2C SCL
            GPIO32 ────┤ [6]        [33] ├───── GPIO1/TX0
            GPIO33 ────┤ [7]        [32] ├───── GPIO3/RX0
            GPIO25 ────┤ [8]        [31] ├───── GPIO21 ◄── I2C SDA
            GPIO26 ────┤ [9]        [30] ├───── GND
            GPIO27 ────┤ [10]       [29] ├───── GPIO19
            GPIO14 ────┤ [11]       [28] ├───── GPIO18
            GPIO12 ────┤ [12]       [27] ├───── GPIO5
               GND ────┤ [13]       [26] ├───── GPIO17
            GPIO13 ────┤ [14]       [25] ├───── GPIO16
         GPIO9/SD2 ────┤ [15]       [24] ├───── GPIO4
        GPIO10/SD3 ────┤ [16]       [23] ├───── GPIO0
        GPIO11/CMD ────┤ [17]       [22] ├───── GPIO2
              3V3  ────┤ [18]       [21] ├───── GPIO15
         GPIO6/CLK ────┤ [19]       [20] ├───── GPIO8/SD1
                       └──────────────────┘
    
    ◄── PN532 bağlantısı için kullanılacak pinler
```

### ESP32 I2C Pin Özeti

| ESP32 Pin | GPIO | I2C Fonksiyon | Açıklama |
|-----------|------|---------------|----------|
| Pin 31 | GPIO21 | SDA | Varsayılan I2C Data |
| Pin 34 | GPIO22 | SCL | Varsayılan I2C Clock |
| Pin 36 | 3V3 | VCC | 3.3V güç çıkışı |
| Pin 37 | GND | GND | Toprak |

---

## Bağlantı Şeması

### Temel Bağlantı (4 Kablo)

```
    ┌─────────────────────────────────────────────────────────────┐
    │                      BAĞLANTI ŞEMASI                        │
    └─────────────────────────────────────────────────────────────┘

         ESP32-WROOM                           PN532 NFC
        ┌───────────┐                        ┌───────────┐
        │           │                        │           │
        │    3V3 ───┼────── KIRMIZI ─────────┼─── VCC    │
        │   (36)    │       (3.3V)           │   (1)     │
        │           │                        │           │
        │    GND ───┼────── SİYAH ───────────┼─── GND    │
        │   (37)    │       (Toprak)         │   (2)     │
        │           │                        │           │
        │ GPIO21 ───┼────── MAVİ ────────────┼─── SDA    │
        │   (31)    │       (Data)           │   (3)     │
        │           │                        │           │
        │ GPIO22 ───┼────── SARI ────────────┼─── SCL    │
        │   (34)    │       (Clock)          │   (4)     │
        │           │                        │           │
        └───────────┘                        └───────────┘
```

### Detaylı Bağlantı Tablosu

| # | ESP32 Pin | ESP32 GPIO | Kablo Rengi | PN532 Pin | Açıklama |
|---|-----------|------------|-------------|-----------|----------|
| 1 | Pin 36 | 3V3 | 🔴 Kırmızı | VCC | Güç (3.3V) |
| 2 | Pin 37 | GND | ⚫ Siyah | GND | Toprak |
| 3 | Pin 31 | GPIO21 | 🔵 Mavi | SDA | I2C Data |
| 4 | Pin 34 | GPIO22 | 🟡 Sarı | SCL | I2C Clock |

### Opsiyonel Bağlantılar (IRQ ve RST)

```
         ESP32-WROOM                           PN532 NFC
        ┌───────────┐                        ┌───────────┐
        │           │                        │           │
        │ GPIO4  ───┼────── YEŞİL ───────────┼─── IRQ    │
        │   (24)    │       (Interrupt)      │   (5)     │
        │           │                        │           │
        │ GPIO5  ───┼────── TURUNCU ─────────┼─── RST    │
        │   (27)    │       (Reset)          │   (6)     │
        │           │                        │           │
        └───────────┘                        └───────────┘
```

> **Not:** IRQ ve RST pinleri Funtoria projesinde kullanılmamaktadır. 
> Driver'da `-1` olarak tanımlanmıştır.

---

## I2C Modu Ayarı

PN532 modülü birden fazla haberleşme modunu destekler. **I2C modu** için DIP switch'leri aşağıdaki gibi ayarlayın:

```
    DIP Switch Ayarları
    
    ┌─────────────────────────────────────────┐
    │                                         │
    │   I2C MODU (Funtoria için gerekli)     │
    │                                         │
    │   ┌─────────┐   ┌─────────┐            │
    │   │   SW1   │   │   SW2   │            │
    │   │  ┌───┐  │   │  ┌───┐  │            │
    │   │  │   │  │   │  │ █ │  │            │
    │   │  │   │  │   │  │ █ │  │            │
    │   │  │   │  │   │  └───┘  │            │
    │   │  │   │  │   │   ON    │            │
    │   │  │ █ │  │   │         │            │
    │   │  │ █ │  │   │         │            │
    │   │  └───┘  │   │         │            │
    │   │   OFF   │   │         │            │
    │   └─────────┘   └─────────┘            │
    │                                         │
    │   SW1: OFF (Aşağıda)                   │
    │   SW2: ON  (Yukarıda)                  │
    │                                         │
    └─────────────────────────────────────────┘
```

### Tüm Modlar

| SW1 | SW2 | Mod | Açıklama |
|-----|-----|-----|----------|
| OFF | ON | **I2C** | ✅ Funtoria için kullan |
| ON | OFF | SPI | Yüksek hız gerekirse |
| ON | ON | HSU (UART) | Seri haberleşme |
| OFF | OFF | - | Geçersiz |

---

## Detaylı Pin Açıklamaları

### VCC (Güç)

```
    ESP32 3V3 ────────────────────► PN532 VCC
    
    • Voltaj: 3.3V DC
    • Akım: ~150mA (max)
    • NOT: PN532 5V toleranslı ama 3.3V önerilir
```

### GND (Toprak)

```
    ESP32 GND ────────────────────► PN532 GND
    
    • Ortak referans noktası
    • Tüm GND pinleri bağlanmalı
```

### SDA (I2C Data)

```
    ESP32 GPIO21 ──────┬──────────► PN532 SDA
                       │
                      [R]  ◄── 4.7kΩ Pull-up (Opsiyonel)
                       │
                      VCC
    
    • Çift yönlü data hattı
    • Açık-drain konfigürasyon
    • I2C adresi: 0x24 (varsayılan)
```

### SCL (I2C Clock)

```
    ESP32 GPIO22 ──────┬──────────► PN532 SCL
                       │
                      [R]  ◄── 4.7kΩ Pull-up (Opsiyonel)
                       │
                      VCC
    
    • Clock hattı (Master: ESP32)
    • Frekans: 400kHz (Fast Mode)
```

---

## Breadboard Bağlantısı

```
    ┌─────────────────────────────────────────────────────────────────────┐
    │                         BREADBOARD GÖRÜNÜMÜ                         │
    └─────────────────────────────────────────────────────────────────────┘

         1   5   10  15  20  25  30  35  40  45  50  55  60
       ┌─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐
    +  │●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│ VCC Rail
       ├─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┤
    -  │●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│ GND Rail
       ├─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┤
       │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
    a  │ │ │ │ │█│█│█│█│█│█│█│█│█│█│█│█│█│█│█│ │ │ │ │█│█│█│█│█│█│ │ │
       │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
    b  │ │ │ │ │█│█│█│█│█│█│█│█│█│█│█│█│█│█│█│ │ │ │ │█│█│█│█│█│█│ │ │
       │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
    c  │ │ │ │ │█│█│█│█│█│█│█│█│█│█│█│█│█│█│█│ │ │ │ │█│█│█│█│█│█│ │ │
       │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
    d  │ │ │ │ │█│█│█│█│█│█│█│█│█│█│█│█│█│█│█│ │ │ │ │█│█│█│█│█│█│ │ │
       │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
    e  │ │ │ │ │█│█│█│█│█│█│█│█│█│█│█│█│█│█│█│ │ │ │ │█│█│█│█│█│█│ │ │
       ├─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┤
       │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
    f  │ │ │ │ │█│█│█│█│█│█│█│█│█│█│█│█│█│█│█│ │ │ │ │█│█│█│█│█│█│ │ │
       │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
    g  │ │ │ │ │█│█│█│█│█│█│█│█│█│█│█│█│█│█│█│ │ │ │ │█│█│█│█│█│█│ │ │
       │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
    h  │ │ │ │ │█│█│█│█│█│█│█│█│█│█│█│█│█│█│█│ │ │ │ │█│█│█│█│█│█│ │ │
       │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
    i  │ │ │ │ │█│█│█│█│█│█│█│█│█│█│█│█│█│█│█│ │ │ │ │█│█│█│█│█│█│ │ │
       │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
    j  │ │ │ │ │█│█│█│█│█│█│█│█│█│█│█│█│█│█│█│ │ │ │ │█│█│█│█│█│█│ │ │
       ├─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┤
    +  │●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│ VCC Rail
       ├─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┼─┤
    -  │●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│●│ GND Rail
       └─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┘
              ▲                                    ▲
              │                                    │
         ESP32-WROOM                          PN532 Modül
         (5-19 arası)                        (24-29 arası)


    KABLOLAR:
    ════════════
    
    🔴 VCC:  ESP32 3V3 (Pin 18) ──────────────► PN532 VCC ──► + Rail
    ⚫ GND:  ESP32 GND (Pin 13) ──────────────► PN532 GND ──► - Rail
    🔵 SDA:  ESP32 GPIO21 (Pin 11) ───────────► PN532 SDA
    🟡 SCL:  ESP32 GPIO22 (Pin 14) ───────────► PN532 SCL
```

---

## PCB Tasarım Notları

### Önerilen PCB Layout

```
    ┌─────────────────────────────────────────────────────────────┐
    │                      PCB ÜST KATMAN                         │
    │                                                             │
    │   ┌───────────────┐              ┌───────────────────────┐ │
    │   │               │              │                       │ │
    │   │    ESP32      │   I2C Bus    │       PN532           │ │
    │   │    Module     │ ═══════════► │       Module          │ │
    │   │               │              │                       │ │
    │   │   GPIO21 ─────┼──────────────┼───── SDA              │ │
    │   │   GPIO22 ─────┼──────────────┼───── SCL              │ │
    │   │               │              │                       │ │
    │   └───────────────┘              └───────────────────────┘ │
    │                                                             │
    │   ┌─────────────────────────────────────────────────────┐  │
    │   │                    GND PLANE                         │  │
    │   └─────────────────────────────────────────────────────┘  │
    │                                                             │
    └─────────────────────────────────────────────────────────────┘
```

### Tasarım Kuralları

| Parametre | Değer | Açıklama |
|-----------|-------|----------|
| I2C trace uzunluğu | < 30cm | Sinyal bütünlüğü |
| Pull-up direnç | 4.7kΩ | SDA ve SCL için |
| Bypass kapasitör | 100nF | PN532 VCC yakınında |
| GND plane | Gerekli | EMI azaltma |

### Şematik Sembol

```
                                    VCC (3.3V)
                                        │
                                       ─┴─
                                       ─┬─  100nF
                                        │
    ┌───────────────────────────────────┼───────────────────────┐
    │                                   │                       │
    │   ESP32-WROOM                     │          PN532        │
    │   ┌─────────┐                     │       ┌─────────┐     │
    │   │         │                     └───────┤ VCC     │     │
    │   │   3V3 ──┼─────────────────────────────┤         │     │
    │   │         │                             │         │     │
    │   │   GND ──┼─────────────────────────────┤ GND     │     │
    │   │         │          ┌───[4.7kΩ]───VCC  │         │     │
    │   │ GPIO21 ─┼──────────┴──────────────────┤ SDA     │     │
    │   │         │          ┌───[4.7kΩ]───VCC  │         │     │
    │   │ GPIO22 ─┼──────────┴──────────────────┤ SCL     │     │
    │   │         │                             │         │     │
    │   └─────────┘                             └─────────┘     │
    │                                                           │
    └───────────────────────────────────────────────────────────┘
```

---

## Sorun Giderme

### Problem: PN532 Bulunamadı

```
[E][PN532] Kart bulunamadı!
```

**Kontrol Listesi:**

- [ ] VCC bağlantısı doğru mu? (3.3V)
- [ ] GND bağlantısı var mı?
- [ ] SDA → GPIO21 bağlı mı?
- [ ] SCL → GPIO22 bağlı mı?
- [ ] DIP switch I2C modunda mı? (SW1:OFF, SW2:ON)
- [ ] Jumper kablolar sağlam mı?

**I2C Tarama Kodu:**

```cpp
#include <Wire.h>

void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);
    
    Serial.println("I2C Tarama başlıyor...");
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("Cihaz bulundu: 0x%02X\n", addr);
        }
    }
}

void loop() {}
```

**Beklenen çıktı:**
```
I2C Tarama başlıyor...
Cihaz bulundu: 0x24  ◄── PN532 varsayılan adresi
```

### Problem: Kararsız Bağlantı

**Belirtiler:**
- Aralıklı okuma hataları
- Kart bazen tanınmıyor

**Çözümler:**

1. **Pull-up direnç ekleyin:**
   ```
   SDA ──┬── 4.7kΩ ── VCC
   SCL ──┬── 4.7kΩ ── VCC
   ```

2. **Kablo uzunluğunu kısaltın** (< 20cm önerilir)

3. **Bypass kapasitör ekleyin:**
   ```
   PN532 VCC ──┬── 100nF ── GND
   ```

### Problem: I2C Hız Uyumsuzluğu

**Config.h'da frekansı düşürün:**

```cpp
// src/config/Config.h
#define I2C_CLOCK_SPEED     100000    // 100kHz (Standard Mode)
// #define I2C_CLOCK_SPEED  400000    // 400kHz (Fast Mode)
```

---

## Özet Bağlantı Kartı

```
┌─────────────────────────────────────────────────────────────────┐
│                    HIZLI REFERANS KARTI                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ESP32-WROOM          Kablo           PN532                    │
│   ───────────          ─────           ─────                    │
│                                                                 │
│   3V3 (Pin 36)    🔴 Kırmızı    →    VCC (Pin 1)              │
│   GND (Pin 37)    ⚫ Siyah      →    GND (Pin 2)              │
│   GPIO21 (Pin 31) 🔵 Mavi       →    SDA (Pin 3)              │
│   GPIO22 (Pin 34) 🟡 Sarı       →    SCL (Pin 4)              │
│                                                                 │
│   DIP Switch: SW1=OFF, SW2=ON (I2C Modu)                       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

*Funtoria Embedded Team - 2026*
*Hibrit Mimari v2.0.0*
