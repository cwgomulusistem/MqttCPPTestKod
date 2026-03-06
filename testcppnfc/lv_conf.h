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

// LVGL v9: bellek yonetimi
// Dahili 64KB havuz yerine ESP32 sistem heap'i kullan.
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

// LVGL OSAL:
// Bu projede tum LVGL islemleri tek LcdTask icinde yurudugu icin LV_OS_NONE
// daha stabil calisir.
#define LV_USE_OS LV_OS_NONE

// Font: Montserrat fontları
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1

// Varsayılan font
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#endif // LV_CONF_H
