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

namespace {
constexpr uint8_t kBacklightChannel = 0;
constexpr uint16_t kBacklightFreq = 5000;
constexpr uint8_t kBacklightResBits = 10;
alignas(4) static lv_color_t g_lvgl_buf1[LVGL_BUF_SIZE];

uint32_t backlightDutyFromByte(uint8_t brightness) {
  const uint32_t maxDuty = (1U << kBacklightResBits) - 1U;
  const uint32_t rawDuty = (maxDuty * brightness) / 255U;
#if defined(TFT_BACKLIGHT_ON) && (TFT_BACKLIGHT_ON == LOW)
  return maxDuty - rawDuty;
#else
  return rawDuty;
#endif
}
} // namespace

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
  state->tft->begin();
  state->tft->setRotation(config->rotation);
  state->tft->fillScreen(TFT_BLACK);
  LOG_I("ILI9341: TFT_eSPI initialized (%dx%d, rotation=%d)", config->width,
        config->height, config->rotation);

  // Backlight PWM baslat (testesp32LCD ile ayni LEDC akisi)
  ledcSetup(kBacklightChannel, kBacklightFreq, kBacklightResBits);
  ledcAttachPin(config->bl_pin, kBacklightChannel);
  ledcWrite(kBacklightChannel, backlightDutyFromByte(config->bl_brightness));
  LOG_D("ILI9341: Backlight pin=%d, brightness=%d, duty=%lu", config->bl_pin,
        config->bl_brightness,
        static_cast<unsigned long>(backlightDutyFromByte(config->bl_brightness)));

  // LVGL baslat (tek seferlik)
  LOG_I("ILI9341: LVGL init start");
  lv_init();
  LOG_I("ILI9341: LVGL init done");

#if LVGL_VERSION_MAJOR >= 9
  // LVGL v9 display oluştur
  LOG_I("ILI9341: lv_display_create start");
  state->display = lv_display_create(config->width, config->height);
  if (!state->display) {
    LOG_E("ILI9341: lv_display_create failed!");
    return false;
  }
  LOG_I("ILI9341: lv_display_create ok");

  LOG_I("ILI9341: lv_display_set_buffers start");
  lv_display_set_buffers(state->display, g_lvgl_buf1, NULL,
                         sizeof(g_lvgl_buf1),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  LOG_I("ILI9341: lv_display_set_buffers ok");
  lv_display_set_color_format(state->display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(state->display, lvgl_flush_cb);
  lv_display_set_user_data(state->display, state);
#else
  // LVGL v8 display driver oluştur
  lv_disp_draw_buf_init(&state->draw_buf, g_lvgl_buf1, NULL, LVGL_BUF_SIZE);
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
  ledcWrite(kBacklightChannel, backlightDutyFromByte(brightness));
}

void ILI9341Driver_setRotation(ILI9341DriverState *state, uint8_t rotation) {
  if (!state || !state->tft)
    return;
  state->tft->setRotation(rotation);
  LOG_D("ILI9341: Rotation set to %d", rotation);
}
