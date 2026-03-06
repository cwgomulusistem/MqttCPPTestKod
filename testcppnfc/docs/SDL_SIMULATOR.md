# SDL Ekran Simulasyonu Kullanim Kilavuzu (Fedora)

Bu kilavuz, `src/screens` altindaki LVGL ekranlarini bilgisayarda SDL penceresinde calistirmak icindir.

## Hizli Baslangic

Proje kok dizininde su komutlari calistir:

```bash
sudo dnf install -y cmake gcc-c++ make pkgconf-pkg-config SDL2-devel
cmake -S simulator -B simulator/build
cmake --build simulator/build -j
./simulator/build/funtoria_sdl_sim
```

## Adim Adim Kullanim

1. Proje klasorune gir:

```bash
cd /home/ismetkabatepe/MqttCPPTestKod/testcppnfc
```

2. Gereksinimleri kur:

```bash
sudo dnf install -y cmake gcc-c++ make pkgconf-pkg-config SDL2-devel
```

3. Simulasyon projesini configure et:

```bash
cmake -S simulator -B simulator/build
```

4. Simulatordu derle:

```bash
cmake --build simulator/build -j
```

5. Simulasyonu ac:

```bash
./simulator/build/funtoria_sdl_sim
```

6. Kapatmak icin SDL penceresini kapatman yeterli.

## Guncelleme Sonrasi Yeniden Derleme

`src/screens` altinda degisiklik yaptiktan sonra:

```bash
cmake --build simulator/build -j
./simulator/build/funtoria_sdl_sim
```

## Temiz Derleme

Build klasorunu sifirdan almak istersen:

```bash
rm -rf simulator/build
cmake -S simulator -B simulator/build
cmake --build simulator/build -j
```

## Sik Karsilasilan Hatalar

`Package 'sdl2' not found`:

```bash
sudo dnf install -y SDL2-devel pkgconf-pkg-config
```

`CMake configure` asamasinda derleyici hatasi:

```bash
cmake -S simulator -B simulator/build \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++
```

## Kapsam

- Bu simulator sadece UI ekranlarini test eder.
- Donanima bagli kisimlar (PN532, WiFi, MQTT, SPIFFS) simulatorde calismaz.
- Simulator ayarlari `simulator/lv_conf_sim.h` dosyasindadir.
- ESP32 firmware derlemesi (`pio run`) simulatorden bagimsizdir.
