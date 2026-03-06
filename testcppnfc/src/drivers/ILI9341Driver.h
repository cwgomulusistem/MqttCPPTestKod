/**
 * @file ILI9341Driver.h
 * @brief ILI9341 LCD Driver (TFT_eSPI + LVGL Bridge)
 * @version 1.0.0
 *
 * TFT_eSPI donanım sürücüsünü LVGL display driver'ına bağlar.
 * C-Style state struct + C++ init fonksiyonu.
 */

#ifndef ILI9341_DRIVER_H
#define ILI9341_DRIVER_H

#include "../config/Config.h"
#include <TFT_eSPI.h>
#include <lvgl.h>

// LVGL display buffer boyutu (240 × LCD_BUF_LINES piksel × 2 byte/piksel)
#define LVGL_BUF_SIZE (LCD_WIDTH * LCD_BUF_LINES)

/**
 * @brief ILI9341 Driver State (C-Style)
 *
 * TFT_eSPI instance ve LVGL display bağlantısı.
 * Global objede BSS section'da tutulur.
 */
struct ILI9341DriverState {
  TFT_eSPI *tft;         // TFT_eSPI hardware pointer
#if LVGL_VERSION_MAJOR >= 9
  lv_display_t *display; // LVGL v9 display objesi
#else
  lv_disp_t *display;              // LVGL v8 display objesi
  lv_disp_draw_buf_t draw_buf;     // LVGL v8 draw buffer descriptor
  lv_disp_drv_t disp_drv;          // LVGL v8 display driver descriptor
#endif
  uint8_t bl_pin;        // Backlight pin
  bool initialized;      // Başlatıldı mı?

  // LVGL display buffer (partial rendering için)
  lv_color_t buf1[LVGL_BUF_SIZE];
};

// =============================================================================
// DRIVER FONKSİYONLARI
// =============================================================================

/**
 * @brief Driver'ı başlat
 * @param state Driver state pointer
 * @param tft TFT_eSPI instance pointer
 * @param config LCD konfigürasyonu
 * @return true başarılıysa
 */
bool ILI9341Driver_init(ILI9341DriverState *state, TFT_eSPI *tft,
                        const LcdConfig *config);

/**
 * @brief Backlight parlaklığını ayarla
 * @param state Driver state pointer
 * @param brightness 0-255 PWM değeri
 */
void ILI9341Driver_setBacklight(ILI9341DriverState *state, uint8_t brightness);

/**
 * @brief Ekran rotasyonunu ayarla
 * @param state Driver state pointer
 * @param rotation 0-3 değeri
 */
void ILI9341Driver_setRotation(ILI9341DriverState *state, uint8_t rotation);

#endif // ILI9341_DRIVER_H
