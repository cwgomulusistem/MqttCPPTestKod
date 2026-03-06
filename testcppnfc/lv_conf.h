/**
 * @file lv_conf.h
 * @brief LVGL Konfigürasyonu
 *
 * LV_CONF_INCLUDE_SIMPLE build flag ile LVGL bu dosyayı otomatik bulur.
 * Shell-safe olmayan makrolar (parantez içeren) burada tanımlanır.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

// Tick kaynağı: Arduino millis() kullan
// lv_tick_inc() çağırmaya gerek yok
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

// Renk derinliği: 16-bit (RGB565) - ILI9341 native format
#define LV_COLOR_DEPTH 16

// LVGL OSAL: FreeRTOS kullan
// (Bu proje ESP32 + FreeRTOS task mimarisi ile calisiyor)
#define LV_USE_OS LV_OS_FREERTOS

// Bellek: LVGL'in kendi malloc/free'sini kullanma, sistem malloc kullansın
#define LV_MEM_CUSTOM 1

// Font: Montserrat fontları
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1

// Varsayılan font
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#endif // LV_CONF_H
