/**
 * @file ILI9341Driver.cpp
 * @brief ILI9341 LCD Driver Implementasyonu
 * @version 1.0.0
 *
 * TFT_eSPI ile LVGL arasındaki köprü.
 * Flush callback ile LVGL render → SPI transfer.
 */

#include "ILI9341Driver.h"
#include "../config/Logger.h"

// =============================================================================
// LVGL FLUSH CALLBACK (Static C-Style)
// =============================================================================

/**
 * @brief LVGL → TFT_eSPI flush callback
 *
 * LVGL bir alanı renderlediğinde bu fonksiyon çağrılır.
 * TFT_eSPI ile SPI üzerinden ekrana aktarır.
 */
#if LVGL_VERSION_MAJOR >= 9
static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area,
                          uint8_t *px_map) {
  ILI9341DriverState *state =
      (ILI9341DriverState *)lv_display_get_user_data(display);

  if (!state || !state->tft) {
    lv_display_flush_ready(display);
    return;
  }

  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  state->tft->startWrite();
  state->tft->setAddrWindow(area->x1, area->y1, w, h);
  state->tft->pushColors((uint16_t *)px_map, w * h, true);
  state->tft->endWrite();

  lv_display_flush_ready(display);
}
#else
static void lvgl_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area,
                          lv_color_t *color_p) {
  ILI9341DriverState *state = (ILI9341DriverState *)disp_drv->user_data;

  if (!state || !state->tft) {
    lv_disp_flush_ready(disp_drv);
    return;
  }

  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  state->tft->startWrite();
  state->tft->setAddrWindow(area->x1, area->y1, w, h);
  state->tft->pushColors((uint16_t *)color_p, w * h, true);
  state->tft->endWrite();

  lv_disp_flush_ready(disp_drv);
}
#endif

// =============================================================================
// DRIVER FONKSİYONLARI
// =============================================================================

bool ILI9341Driver_init(ILI9341DriverState *state, TFT_eSPI *tft,
                        const LcdConfig *config) {
  if (!state || !tft || !config) {
    LOG_E("ILI9341: NULL pointer!");
    return false;
  }

  state->tft = tft;
  state->bl_pin = config->bl_pin;
  state->initialized = false;

  // TFT_eSPI başlat
  state->tft->init();
  state->tft->setRotation(config->rotation);
  state->tft->fillScreen(TFT_BLACK);
  LOG_I("ILI9341: TFT_eSPI initialized (%dx%d, rotation=%d)", config->width,
        config->height, config->rotation);

  // Backlight PWM başlat
  pinMode(config->bl_pin, OUTPUT);
  analogWrite(config->bl_pin, config->bl_brightness);
  LOG_D("ILI9341: Backlight pin=%d, brightness=%d", config->bl_pin,
        config->bl_brightness);

  // LVGL başlat (tek seferlik)
  lv_init();
  LOG_D("ILI9341: LVGL initialized");

#if LVGL_VERSION_MAJOR >= 9
  // LVGL v9 display oluştur
  state->display = lv_display_create(config->width, config->height);
  if (!state->display) {
    LOG_E("ILI9341: lv_display_create failed!");
    return false;
  }

  lv_display_set_buffers(state->display, state->buf1, NULL, sizeof(state->buf1),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(state->display, lvgl_flush_cb);
  lv_display_set_user_data(state->display, state);
#else
  // LVGL v8 display driver oluştur
  lv_disp_draw_buf_init(&state->draw_buf, state->buf1, NULL, LVGL_BUF_SIZE);
  lv_disp_drv_init(&state->disp_drv);
  state->disp_drv.hor_res = config->width;
  state->disp_drv.ver_res = config->height;
  state->disp_drv.flush_cb = lvgl_flush_cb;
  state->disp_drv.draw_buf = &state->draw_buf;
  state->disp_drv.user_data = state;
  state->display = lv_disp_drv_register(&state->disp_drv);
  if (!state->display) {
    LOG_E("ILI9341: lv_disp_drv_register failed!");
    return false;
  }
#endif

  state->initialized = true;
  LOG_I("ILI9341: Driver fully initialized");

  return true;
}

void ILI9341Driver_setBacklight(ILI9341DriverState *state, uint8_t brightness) {
  if (!state)
    return;
  analogWrite(state->bl_pin, brightness);
}

void ILI9341Driver_setRotation(ILI9341DriverState *state, uint8_t rotation) {
  if (!state || !state->tft)
    return;
  state->tft->setRotation(rotation);
  LOG_D("ILI9341: Rotation set to %d", rotation);
}
