#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <WiFi.h>

TFT_eSPI tft = TFT_eSPI();

static uint32_t last_tick_ms = 0;
static uint8_t g_screen_index = 0;
static lv_obj_t *g_wifi_bars[3] = {nullptr, nullptr, nullptr};
constexpr uint8_t BOOT_SCREEN_INDEX = 0;
constexpr uint8_t ACTIVE_SCREEN_COUNT = 8;
constexpr uint32_t SCREEN_SWAP_MS = 2000;

struct CloudBlob {
  lv_obj_t *obj;
  int16_t cx;
  int16_t cy;
  int16_t min_size;
  int16_t max_size;
};

static CloudBlob g_cloud_blobs[6] = {};
static uint8_t g_cloud_blob_count = 0;
static lv_timer_t *g_cloud_color_timer = nullptr;

constexpr uint8_t BACKLIGHT_CHANNEL = 0;
constexpr uint16_t BACKLIGHT_FREQ = 5000;
constexpr uint8_t BACKLIGHT_RES_BITS = 10;
static uint8_t g_backlight_percent = 100;
static lv_obj_t *g_brightness_value_label = nullptr;

static void set_backlight_percent(uint8_t percent) {
  if (percent > 100U) {
    percent = 100U;
  }
#ifdef TFT_BL
  const uint32_t max_duty = (1U << BACKLIGHT_RES_BITS) - 1U;
  const uint32_t raw_duty = (max_duty * percent) / 100U;

#if defined(TFT_BACKLIGHT_ON) && (TFT_BACKLIGHT_ON == LOW)
  const uint32_t duty = max_duty - raw_duty;
#else
  const uint32_t duty = raw_duty;
#endif

  ledcWrite(BACKLIGHT_CHANNEL, duty);
#else
  LV_UNUSED(percent);
#endif
}

static void run_tft_startup_probe() {
  // Direct TFT smoke test before LVGL rendering starts.
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 12);
  tft.println("TFT SPI TEST");
  tft.println("ILI9341 init OK");
  delay(500);

  tft.fillScreen(TFT_RED);
  delay(250);
  tft.fillScreen(TFT_GREEN);
  delay(250);
  tft.fillScreen(TFT_BLUE);
  delay(250);
  tft.fillScreen(TFT_BLACK);
}

static lv_opa_t opacity_from_percent(uint8_t percent) {
  return static_cast<lv_opa_t>((255U * percent) / 100U);
}

static lv_color_t pick_contrast_circle_color(lv_color_t bg_color, lv_color_t seed_color, uint8_t strength_percent) {
  const lv_opa_t strength = opacity_from_percent(strength_percent);
  if (lv_color_brightness(bg_color) >= 128U) {
    return lv_color_darken(seed_color, strength);
  }
  return lv_color_lighten(seed_color, strength);
}

static void apply_contrast_circle_style(lv_obj_t *obj, lv_color_t bg_color, lv_color_t seed_color,
                                        uint8_t fill_strength_percent, uint8_t border_strength_percent) {
  const lv_color_t fill_color = pick_contrast_circle_color(bg_color, seed_color, fill_strength_percent);
  const lv_color_t border_color = pick_contrast_circle_color(bg_color, seed_color, border_strength_percent);

  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(obj, fill_color, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, border_color, 0);
  lv_obj_set_style_border_width(obj, 2, 0);
  lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
}

static void clear_wifi_indicator_targets() {
  g_wifi_bars[0] = nullptr;
  g_wifi_bars[1] = nullptr;
  g_wifi_bars[2] = nullptr;
}

static uint8_t get_wifi_level() {
  if (WiFi.status() != WL_CONNECTED) {
    return 0;
  }

  const int32_t rssi = WiFi.RSSI();
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

static void apply_wifi_level_to_indicator(uint8_t level) {
  for (uint8_t i = 0; i < 3; i++) {
    if (g_wifi_bars[i] == nullptr) {
      continue;
    }
    lv_obj_set_style_bg_opa(g_wifi_bars[i], (level > i) ? LV_OPA_COVER : LV_OPA_20, 0);
  }
}

static void wifi_update_timer_cb(lv_timer_t *timer) {
  LV_UNUSED(timer);
  apply_wifi_level_to_indicator(get_wifi_level());
}

static void clear_cloud_animation_targets() {
  g_cloud_blob_count = 0;
  for (uint8_t i = 0; i < 6; i++) {
    g_cloud_blobs[i].obj = nullptr;
  }
  if (g_cloud_color_timer != nullptr) {
    lv_timer_del(g_cloud_color_timer);
    g_cloud_color_timer = nullptr;
  }
}

static void cloud_blob_anim_exec(void *var, int32_t value) {
  CloudBlob *blob = static_cast<CloudBlob *>(var);
  if (blob == nullptr || blob->obj == nullptr) {
    return;
  }

  lv_obj_set_size(blob->obj, value, value);
  lv_obj_set_pos(blob->obj, blob->cx - (value / 2), blob->cy - (value / 2));
  lv_obj_set_style_bg_opa(blob->obj, LV_OPA_COVER, 0);
}

static void cloud_color_timer_cb(lv_timer_t *timer) {
  LV_UNUSED(timer);
  static const uint32_t palette[] = {0x6D28D9, 0x8B5CF6, 0xA855F7, 0xEC4899, 0x22D3EE, 0x14B8A6};
  static uint8_t phase = 0;
  const uint8_t palette_size = sizeof(palette) / sizeof(palette[0]);

  for (uint8_t i = 0; i < g_cloud_blob_count; i++) {
    if (g_cloud_blobs[i].obj == nullptr) {
      continue;
    }
    const lv_color_t c = lv_color_hex(palette[(phase + i * 2U) % palette_size]);
    lv_obj_set_style_bg_color(g_cloud_blobs[i].obj, c, 0);
    lv_obj_set_style_shadow_color(g_cloud_blobs[i].obj, c, 0);
  }

  phase = static_cast<uint8_t>((phase + 1U) % palette_size);
}

static void create_wifi_indicator(lv_obj_t *parent, lv_color_t color) {
  lv_obj_t *wifi_wrap = lv_obj_create(parent);
  lv_obj_remove_style_all(wifi_wrap);
  lv_obj_set_size(wifi_wrap, 34, 18);
  lv_obj_align(wifi_wrap, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_radius(wifi_wrap, 5, 0);
  lv_obj_set_style_bg_color(wifi_wrap, color, 0);
  lv_obj_set_style_bg_opa(wifi_wrap, LV_OPA_10, 0);
  lv_obj_set_style_border_color(wifi_wrap, color, 0);
  lv_obj_set_style_border_opa(wifi_wrap, LV_OPA_40, 0);
  lv_obj_set_style_border_width(wifi_wrap, 1, 0);
  lv_obj_set_style_pad_all(wifi_wrap, 0, 0);

  lv_obj_t *bar0 = lv_obj_create(wifi_wrap);
  lv_obj_remove_style_all(bar0);
  lv_obj_set_size(bar0, 6, 6);
  lv_obj_align(bar0, LV_ALIGN_BOTTOM_LEFT, 4, -1);
  lv_obj_set_style_radius(bar0, 2, 0);
  lv_obj_set_style_bg_color(bar0, color, 0);

  lv_obj_t *bar1 = lv_obj_create(wifi_wrap);
  lv_obj_remove_style_all(bar1);
  lv_obj_set_size(bar1, 6, 10);
  lv_obj_align(bar1, LV_ALIGN_BOTTOM_LEFT, 14, -1);
  lv_obj_set_style_radius(bar1, 2, 0);
  lv_obj_set_style_bg_color(bar1, color, 0);

  lv_obj_t *bar2 = lv_obj_create(wifi_wrap);
  lv_obj_remove_style_all(bar2);
  lv_obj_set_size(bar2, 6, 14);
  lv_obj_align(bar2, LV_ALIGN_BOTTOM_LEFT, 24, -1);
  lv_obj_set_style_radius(bar2, 2, 0);
  lv_obj_set_style_bg_color(bar2, color, 0);

  g_wifi_bars[0] = bar0;
  g_wifi_bars[1] = bar1;
  g_wifi_bars[2] = bar2;
  apply_wifi_level_to_indicator(get_wifi_level());
}

static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  const uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  const uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors(reinterpret_cast<uint16_t *>(px_map), w * h, true);
  tft.endWrite();

  lv_display_flush_ready(disp);
}

static void update_brightness_label(uint8_t percent) {
  if (g_brightness_value_label == nullptr) {
    return;
  }

  char value_text[8];
  snprintf(value_text, sizeof(value_text), "%u%%", percent);
  lv_label_set_text(g_brightness_value_label, value_text);
}

static void change_backlight_by(int8_t delta) {
  int16_t value = static_cast<int16_t>(g_backlight_percent) + delta;
  if (value < 0) {
    value = 0;
  } else if (value > 100) {
    value = 100;
  }

  g_backlight_percent = static_cast<uint8_t>(value);
  set_backlight_percent(g_backlight_percent);
  update_brightness_label(g_backlight_percent);
}

static void brightness_step_button_event_cb(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED && code != LV_EVENT_LONG_PRESSED_REPEAT) {
    return;
  }

  const int8_t *delta = static_cast<int8_t *>(lv_event_get_user_data(e));
  if (delta == nullptr) {
    return;
  }

  change_backlight_by(*delta);
}

static lv_obj_t *build_brightness_touch_screen() {
  clear_cloud_animation_targets();
  clear_wifi_indicator_targets();

  lv_obj_t *scr = lv_obj_create(nullptr);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(scr, LV_DIR_NONE);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_radius(scr, 0, 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_NONE, 0);

  lv_obj_t *panel = lv_obj_create(scr);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(panel, LV_DIR_NONE);
  lv_obj_set_size(panel, 212, 220);
  lv_obj_align(panel, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_style_radius(panel, 18, 0);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_shadow_opa(panel, LV_OPA_TRANSP, 0);

  lv_obj_t *icon = lv_label_create(panel);
  lv_label_set_text(icon, LV_SYMBOL_SETTINGS);
  lv_obj_set_style_text_color(icon, lv_color_hex(0x000000), 0);
  lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
  lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 14);

  lv_obj_t *title = lv_label_create(panel);
  lv_label_set_text(title, "PARLAKLIK");
  lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 42);

  lv_obj_t *hint = lv_label_create(panel);
  lv_label_set_text(hint, "Butonla azalt / artir");
  lv_obj_set_style_text_color(hint, lv_color_hex(0x000000), 0);
  lv_obj_set_style_text_opa(hint, LV_OPA_90, 0);
  lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 70);

  g_brightness_value_label = lv_label_create(panel);
  lv_obj_set_style_text_color(g_brightness_value_label, lv_color_hex(0x000000), 0);
  lv_obj_set_style_text_font(g_brightness_value_label, &lv_font_montserrat_32, 0);
  lv_obj_align(g_brightness_value_label, LV_ALIGN_TOP_MID, 0, 92);
  update_brightness_label(g_backlight_percent);

  static int8_t brightness_step_down = -10;
  static int8_t brightness_step_up = 10;

  lv_obj_t *minus_btn = lv_btn_create(panel);
  lv_obj_set_size(minus_btn, 78, 52);
  lv_obj_align(minus_btn, LV_ALIGN_BOTTOM_LEFT, 18, -34);
  lv_obj_set_style_radius(minus_btn, 12, 0);
  lv_obj_set_style_bg_color(minus_btn, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(minus_btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(minus_btn, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(minus_btn, 1, 0);
  lv_obj_add_event_cb(minus_btn, brightness_step_button_event_cb, LV_EVENT_CLICKED, &brightness_step_down);
  lv_obj_add_event_cb(minus_btn, brightness_step_button_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, &brightness_step_down);

  lv_obj_t *minus_lbl = lv_label_create(minus_btn);
  lv_label_set_text(minus_lbl, "-");
  lv_obj_set_style_text_font(minus_lbl, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(minus_lbl, lv_color_hex(0x000000), 0);
  lv_obj_center(minus_lbl);

  lv_obj_t *plus_btn = lv_btn_create(panel);
  lv_obj_set_size(plus_btn, 78, 52);
  lv_obj_align(plus_btn, LV_ALIGN_BOTTOM_RIGHT, -18, -34);
  lv_obj_set_style_radius(plus_btn, 12, 0);
  lv_obj_set_style_bg_color(plus_btn, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(plus_btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(plus_btn, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(plus_btn, 1, 0);
  lv_obj_add_event_cb(plus_btn, brightness_step_button_event_cb, LV_EVENT_CLICKED, &brightness_step_up);
  lv_obj_add_event_cb(plus_btn, brightness_step_button_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, &brightness_step_up);

  lv_obj_t *plus_lbl = lv_label_create(plus_btn);
  lv_label_set_text(plus_lbl, "+");
  lv_obj_set_style_text_font(plus_lbl, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(plus_lbl, lv_color_hex(0x000000), 0);
  lv_obj_center(plus_lbl);

  lv_obj_t *range_lbl = lv_label_create(panel);
  lv_label_set_text(range_lbl, "Adim: 10%    Aralik: 0-100%");
  lv_obj_set_style_text_color(range_lbl, lv_color_hex(0x000000), 0);
  lv_obj_align(range_lbl, LV_ALIGN_BOTTOM_MID, 0, -10);

  lv_obj_t *footer = lv_label_create(scr);
  lv_label_set_text(footer, "Sadece parlaklik ayar ekrani aktif");
  lv_obj_set_style_text_color(footer, lv_color_hex(0x000000), 0);
  lv_obj_set_style_text_opa(footer, LV_OPA_90, 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -12);

  return scr;
}

static bool finder_module_on(uint8_t x, uint8_t y, uint8_t ox, uint8_t oy) {
  if (x < ox || x > (ox + 6) || y < oy || y > (oy + 6)) {
    return false;
  }

  const uint8_t lx = x - ox;
  const uint8_t ly = y - oy;
  const bool outer_ring = (lx == 0 || lx == 6 || ly == 0 || ly == 6);
  const bool inner_dot = (lx >= 2 && lx <= 4 && ly >= 2 && ly <= 4);
  return outer_ring || inner_dot;
}

static bool qr_module_on(uint8_t x, uint8_t y) {
  if (finder_module_on(x, y, 0, 0) || finder_module_on(x, y, 14, 0) || finder_module_on(x, y, 0, 14)) {
    return true;
  }

  if ((x <= 7 && y <= 7) || (x >= 13 && y <= 7) || (x <= 7 && y >= 13)) {
    return false;
  }

  if ((x == 6 && y >= 8 && y <= 12) || (y == 6 && x >= 8 && x <= 12)) {
    return ((x + y) & 1U) == 0U;
  }

  const uint16_t seed = static_cast<uint16_t>(x * 31U + y * 17U + x * y * 7U);
  return (seed % 11U == 0U) || (((x + y) % 7U == 0U) && (seed % 3U == 0U));
}

static lv_obj_t *create_fake_qr_scaled(lv_obj_t *parent, uint8_t pixel_size, uint8_t gap) {
  const uint8_t modules = 21;
  const uint8_t cell = pixel_size + gap;
  const lv_coord_t grid_size = modules * cell - gap;

  lv_obj_t *grid = lv_obj_create(parent);
  lv_obj_remove_style_all(grid);
  lv_obj_set_size(grid, grid_size, grid_size);

  for (uint8_t y = 0; y < modules; y++) {
    for (uint8_t x = 0; x < modules; x++) {
      if (!qr_module_on(x, y)) {
        continue;
      }

      lv_obj_t *px = lv_obj_create(grid);
      lv_obj_remove_style_all(px);
      lv_obj_set_size(px, pixel_size, pixel_size);
      lv_obj_set_pos(px, x * cell, y * cell);
      lv_obj_set_style_radius(px, 0, 0);
      lv_obj_set_style_bg_color(px, lv_color_hex(0x111111), 0);
      lv_obj_set_style_bg_opa(px, LV_OPA_COVER, 0);
    }
  }

  return grid;
}

static lv_obj_t *create_fake_qr(lv_obj_t *parent) {
  return create_fake_qr_scaled(parent, 4, 1);
}

static void create_nav_item(lv_obj_t *parent, int16_t x_off, const char *icon, const char *text, bool active) {
  lv_obj_t *item = lv_obj_create(parent);
  lv_obj_remove_style_all(item);
  lv_obj_set_size(item, 68, 42);
  lv_obj_align(item, LV_ALIGN_CENTER, x_off, 0);

  lv_obj_t *icon_lbl = lv_label_create(item);
  lv_label_set_text(icon_lbl, icon);
  lv_obj_set_style_text_color(icon_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_opa(icon_lbl, active ? LV_OPA_100 : LV_OPA_60, 0);
  lv_obj_align(icon_lbl, LV_ALIGN_TOP_MID, 0, 2);

  lv_obj_t *txt_lbl = lv_label_create(item);
  lv_label_set_text(txt_lbl, text);
  lv_obj_set_style_text_color(txt_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_opa(txt_lbl, active ? LV_OPA_100 : LV_OPA_60, 0);
  lv_obj_align(txt_lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
}

static lv_obj_t *build_scan_screen() {
  lv_obj_t *scr = lv_obj_create(nullptr);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_radius(scr, 0, 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x8B5CF6), 0);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0x4C1D95), 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);

  lv_obj_t *orb_cyan = lv_obj_create(scr);
  lv_obj_remove_style_all(orb_cyan);
  lv_obj_set_size(orb_cyan, 120, 120);
  lv_obj_align(orb_cyan, LV_ALIGN_BOTTOM_LEFT, -55, 45);
  lv_obj_set_style_radius(orb_cyan, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(orb_cyan, lv_color_hex(0x06B6D4), 0);
  lv_obj_set_style_bg_opa(orb_cyan, LV_OPA_20, 0);

  lv_obj_t *orb_pink = lv_obj_create(scr);
  lv_obj_remove_style_all(orb_pink);
  lv_obj_set_size(orb_pink, 120, 120);
  lv_obj_align(orb_pink, LV_ALIGN_TOP_RIGHT, 55, -45);
  lv_obj_set_style_radius(orb_pink, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(orb_pink, lv_color_hex(0xEC4899), 0);
  lv_obj_set_style_bg_opa(orb_pink, LV_OPA_20, 0);

  lv_obj_t *header = lv_obj_create(scr);
  lv_obj_remove_style_all(header);
  lv_obj_set_size(header, 220, 22);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t *left_icon = lv_label_create(header);
  lv_label_set_text(left_icon, LV_SYMBOL_PLAY);
  lv_obj_set_style_text_color(left_icon, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(left_icon, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *left_title = lv_label_create(header);
  lv_label_set_text(left_title, "ARCADE OS");
  lv_obj_set_style_text_color(left_title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_opa(left_title, LV_OPA_90, 0);
  lv_obj_align(left_title, LV_ALIGN_LEFT_MID, 18, 0);

  lv_obj_t *wifi_icon = lv_label_create(header);
  lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(wifi_icon, LV_ALIGN_RIGHT_MID, -18, 0);

  lv_obj_t *bat_icon = lv_label_create(header);
  lv_label_set_text(bat_icon, LV_SYMBOL_BATTERY_FULL);
  lv_obj_set_style_text_color(bat_icon, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(bat_icon, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "OYNAMAK ICIN TARA");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 42);

  lv_obj_t *subtitle = lv_label_create(scr);
  lv_label_set_text(subtitle, "Scan to Play");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_opa(subtitle, LV_OPA_70, 0);
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 64);

  lv_obj_t *qr_card = lv_obj_create(scr);
  lv_obj_set_size(qr_card, 164, 164);
  lv_obj_align(qr_card, LV_ALIGN_TOP_MID, 0, 82);
  lv_obj_set_style_bg_color(qr_card, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(qr_card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(qr_card, 14, 0);
  lv_obj_set_style_border_width(qr_card, 0, 0);
  lv_obj_set_style_pad_all(qr_card, 10, 0);
  lv_obj_set_style_shadow_color(qr_card, lv_color_hex(0x06B6D4), 0);
  lv_obj_set_style_shadow_width(qr_card, 24, 0);
  lv_obj_set_style_shadow_opa(qr_card, LV_OPA_30, 0);

  lv_obj_t *qr_zone = lv_obj_create(qr_card);
  lv_obj_set_size(qr_zone, 142, 142);
  lv_obj_center(qr_zone);
  lv_obj_set_style_bg_color(qr_zone, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(qr_zone, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(qr_zone, 10, 0);
  lv_obj_set_style_border_color(qr_zone, lv_color_hex(0xDDD6FE), 0);
  lv_obj_set_style_border_width(qr_zone, 2, 0);
  lv_obj_set_style_pad_all(qr_zone, 0, 0);

  lv_obj_t *qr = create_fake_qr(qr_zone);
  lv_obj_center(qr);


  lv_obj_t *balance = lv_obj_create(scr);
  lv_obj_set_size(balance, 140, 26);
  lv_obj_align(balance, LV_ALIGN_TOP_MID, 0, 252);
  lv_obj_set_style_radius(balance, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(balance, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(balance, LV_OPA_30, 0);
  lv_obj_set_style_border_color(balance, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_opa(balance, LV_OPA_40, 0);
  lv_obj_set_style_border_width(balance, 1, 0);
  lv_obj_set_style_pad_all(balance, 0, 0);

  lv_obj_t *balance_lbl = lv_label_create(balance);
  lv_label_set_text(balance_lbl, LV_SYMBOL_CHARGE " BAKIYE: TL45.00");
  lv_obj_set_style_text_color(balance_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_center(balance_lbl);

  lv_obj_t *footer = lv_obj_create(scr);
  lv_obj_set_size(footer, 220, 46);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_radius(footer, 12, 0);
  lv_obj_set_style_bg_color(footer, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(footer, LV_OPA_20, 0);
  lv_obj_set_style_border_color(footer, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_opa(footer, LV_OPA_40, 0);
  lv_obj_set_style_border_width(footer, 1, 0);
  lv_obj_set_style_pad_all(footer, 0, 0);

  create_nav_item(footer, -72, LV_SYMBOL_WARNING, "BILGI", false);
  create_nav_item(footer, 0, LV_SYMBOL_IMAGE, "TARA", true);
  create_nav_item(footer, 72, LV_SYMBOL_HOME, "PROFIL", false);

  return scr;
}

static lv_obj_t *build_success_v2_screen() {
  const lv_color_t primary = lv_color_hex(0x1313EC);
  const lv_color_t bg_dark = lv_color_hex(0x101022);
  const lv_color_t success = lv_color_hex(0x00FF88);

  lv_obj_t *scr = lv_obj_create(nullptr);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_radius(scr, 0, 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_set_style_bg_color(scr, bg_dark, 0);

  lv_obj_t *header = lv_obj_create(scr);
  lv_obj_set_size(header, 240, 30);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_bg_color(header, primary, 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_20, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);

  lv_obj_t *back_icon = lv_label_create(header);
  lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_color(back_icon, success, 0);
  lv_obj_align(back_icon, LV_ALIGN_LEFT_MID, 8, 0);

  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "ARCADE OS V2.0");
  lv_obj_set_style_text_color(title, primary, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_opa(title, LV_OPA_90, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t *status_ring = lv_obj_create(header);
  lv_obj_set_size(status_ring, 14, 14);
  lv_obj_align(status_ring, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_obj_set_style_radius(status_ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(status_ring, success, 0);
  lv_obj_set_style_bg_opa(status_ring, LV_OPA_20, 0);
  lv_obj_set_style_border_width(status_ring, 0, 0);
  lv_obj_set_style_pad_all(status_ring, 0, 0);

  lv_obj_t *status_dot = lv_obj_create(status_ring);
  lv_obj_set_size(status_dot, 6, 6);
  lv_obj_center(status_dot);
  lv_obj_set_style_radius(status_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(status_dot, success, 0);
  lv_obj_set_style_bg_opa(status_dot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(status_dot, 0, 0);

  lv_obj_t *main_area = lv_obj_create(scr);
  lv_obj_remove_style_all(main_area);
  lv_obj_set_size(main_area, 240, 170);
  lv_obj_align(main_area, LV_ALIGN_TOP_MID, 0, 30);

  lv_obj_t *ring = lv_obj_create(main_area);
  lv_obj_set_size(ring, 160, 160);
  lv_obj_center(ring);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(ring, success, 0);
  lv_obj_set_style_border_opa(ring, LV_OPA_20, 0);
  lv_obj_set_style_border_width(ring, 10, 0);
  lv_obj_set_style_pad_all(ring, 0, 0);

  lv_obj_t *check_wrap = lv_obj_create(main_area);
  lv_obj_set_size(check_wrap, 80, 80);
  lv_obj_align(check_wrap, LV_ALIGN_TOP_MID, 0, 20);
  lv_obj_set_style_radius(check_wrap, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(check_wrap, success, 0);
  lv_obj_set_style_bg_opa(check_wrap, LV_OPA_10, 0);
  lv_obj_set_style_border_color(check_wrap, success, 0);
  lv_obj_set_style_border_width(check_wrap, 2, 0);
  lv_obj_set_style_border_opa(check_wrap, LV_OPA_100, 0);
  lv_obj_set_style_shadow_color(check_wrap, success, 0);
  lv_obj_set_style_shadow_width(check_wrap, 18, 0);
  lv_obj_set_style_shadow_opa(check_wrap, LV_OPA_40, 0);
  lv_obj_set_style_pad_all(check_wrap, 0, 0);

  lv_obj_t *check_icon = lv_label_create(check_wrap);
  lv_label_set_text(check_icon, LV_SYMBOL_OK);
  lv_obj_set_style_text_color(check_icon, success, 0);
  lv_obj_set_style_text_font(check_icon, &lv_font_montserrat_20, 0);
  lv_obj_center(check_icon);

  lv_obj_t *success_title = lv_label_create(main_area);
  lv_label_set_text(success_title, "ODEME BASARILI");
  lv_obj_set_style_text_color(success_title, success, 0);
  lv_obj_set_style_text_font(success_title, &lv_font_montserrat_16, 0);
  lv_obj_align(success_title, LV_ALIGN_TOP_MID, 0, 108);

  lv_obj_t *success_subtitle = lv_label_create(main_area);
  lv_label_set_text(success_subtitle, "IYI OYUNLAR!");
  lv_obj_set_style_text_color(success_subtitle, lv_color_hex(0x94A3B8), 0);
  lv_obj_align(success_subtitle, LV_ALIGN_TOP_MID, 0, 132);

  lv_obj_t *bottom = lv_obj_create(scr);
  lv_obj_set_size(bottom, 240, 120);
  lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_radius(bottom, 0, 0);
  lv_obj_set_style_bg_color(bottom, lv_color_hex(0x0B0B15), 0);
  lv_obj_set_style_bg_opa(bottom, LV_OPA_80, 0);
  lv_obj_set_style_border_width(bottom, 0, 0);
  lv_obj_set_style_pad_all(bottom, 0, 0);

  lv_obj_t *card = lv_obj_create(bottom);
  lv_obj_set_size(card, 216, 58);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_radius(card, 10, 0);
  lv_obj_set_style_bg_color(card, primary, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_10, 0);
  lv_obj_set_style_border_color(card, primary, 0);
  lv_obj_set_style_border_opa(card, LV_OPA_30, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_pad_all(card, 0, 0);

  lv_obj_t *amount_lbl = lv_label_create(card);
  lv_label_set_text(amount_lbl, "YUKLENEN MIKTAR");
  lv_obj_set_style_text_color(amount_lbl, lv_color_hex(0x64748B), 0);
  lv_obj_align(amount_lbl, LV_ALIGN_TOP_LEFT, 10, 8);

  lv_obj_t *xp_lbl = lv_label_create(card);
  lv_label_set_text(xp_lbl, "500 XP");
  lv_obj_set_style_text_color(xp_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(xp_lbl, &lv_font_montserrat_20, 0);
  lv_obj_align(xp_lbl, LV_ALIGN_BOTTOM_LEFT, 10, -4);

  lv_obj_t *bonus = lv_obj_create(card);
  lv_obj_set_size(bonus, 72, 16);
  lv_obj_align(bonus, LV_ALIGN_TOP_RIGHT, -8, 8);
  lv_obj_set_style_radius(bonus, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(bonus, success, 0);
  lv_obj_set_style_bg_opa(bonus, LV_OPA_10, 0);
  lv_obj_set_style_border_color(bonus, success, 0);
  lv_obj_set_style_border_width(bonus, 1, 0);
  lv_obj_set_style_border_opa(bonus, LV_OPA_30, 0);
  lv_obj_set_style_pad_all(bonus, 0, 0);

  lv_obj_t *bonus_lbl = lv_label_create(bonus);
  lv_label_set_text(bonus_lbl, "+10% BONUS");
  lv_obj_set_style_text_color(bonus_lbl, success, 0);
  lv_obj_center(bonus_lbl);

  lv_obj_t *btn = lv_btn_create(bottom);
  lv_obj_set_size(btn, 216, 32);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_bg_color(btn, primary, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_radius(btn, 8, 0);

  lv_obj_t *btn_lbl = lv_label_create(btn);
  lv_label_set_text(btn_lbl, "BASLAT " LV_SYMBOL_PLAY);
  lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_center(btn_lbl);

  return scr;
}

static lv_obj_t *build_status_screen(const lv_color_t primary, const lv_color_t bg_dark, const lv_color_t bg_soft,
                                     const char *icon_symbol, const char *title_text, const char *detail_text,
                                     const char *badge_text, const char *primary_btn_text,
                                     const char *secondary_btn_text, bool show_buttons, bool use_funtoria_header) {
  clear_cloud_animation_targets();

  lv_obj_t *scr = lv_obj_create(nullptr);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_radius(scr, 0, 0);
  lv_obj_set_style_bg_color(scr, bg_dark, 0);
  lv_obj_set_style_border_color(scr, primary, 0);
  lv_obj_set_style_border_width(scr, 1, 0);
  lv_obj_set_style_border_opa(scr, LV_OPA_30, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);

  lv_obj_t *glow_tl = lv_obj_create(scr);
  lv_obj_remove_style_all(glow_tl);
  lv_obj_set_size(glow_tl, 100, 100);
  lv_obj_align(glow_tl, LV_ALIGN_TOP_LEFT, -40, -35);
  apply_contrast_circle_style(glow_tl, bg_dark, primary, 34, 50);

  lv_obj_t *glow_br = lv_obj_create(scr);
  lv_obj_remove_style_all(glow_br);
  lv_obj_set_size(glow_br, 100, 100);
  lv_obj_align(glow_br, LV_ALIGN_BOTTOM_RIGHT, 40, 35);
  apply_contrast_circle_style(glow_br, bg_dark, primary, 44, 60);

  lv_obj_t *header = lv_obj_create(scr);
  lv_obj_remove_style_all(header);
  lv_obj_set_size(header, 220, 24);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);
  if (use_funtoria_header) {
    lv_obj_t *logo = lv_label_create(header);
    lv_label_set_text(logo, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(logo, primary, 0);
    lv_obj_align(logo, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *header_title = lv_label_create(header);
    lv_label_set_text(header_title, "Funtoria OS");
    lv_obj_set_style_text_color(header_title, primary, 0);
    lv_obj_set_style_text_opa(header_title, LV_OPA_90, 0);
    lv_obj_align(header_title, LV_ALIGN_LEFT_MID, 16, 0);

    create_wifi_indicator(header, primary);
  } else {
    clear_wifi_indicator_targets();

    lv_obj_t *back_icon = lv_label_create(header);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_icon, primary, 0);
    lv_obj_align(back_icon, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *header_title = lv_label_create(header);
    lv_label_set_text(header_title, "ARCADE SYSTEM");
    lv_obj_set_style_text_color(header_title, primary, 0);
    lv_obj_set_style_text_opa(header_title, LV_OPA_90, 0);
    lv_obj_align(header_title, LV_ALIGN_CENTER, 0, 0);
  }

  lv_obj_t *content = lv_obj_create(scr);
  lv_obj_remove_style_all(content);
  lv_obj_set_size(content, 220, 174);
  lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 34);

  lv_obj_t *outer = lv_obj_create(content);
  lv_obj_set_size(outer, 96, 96);
  lv_obj_align(outer, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_radius(outer, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(outer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(outer, primary, 0);
  lv_obj_set_style_border_opa(outer, LV_OPA_20, 0);
  lv_obj_set_style_border_width(outer, 1, 0);

  lv_obj_t *mid = lv_obj_create(content);
  lv_obj_set_size(mid, 80, 80);
  lv_obj_align(mid, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_radius(mid, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(mid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(mid, primary, 0);
  lv_obj_set_style_border_opa(mid, LV_OPA_40, 0);
  lv_obj_set_style_border_width(mid, 1, 0);

  lv_obj_t *icon_wrap = lv_obj_create(content);
  lv_obj_set_size(icon_wrap, 64, 64);
  lv_obj_align(icon_wrap, LV_ALIGN_TOP_MID, 0, 16);
  apply_contrast_circle_style(icon_wrap, bg_dark, primary, 24, 40);
  lv_obj_set_style_shadow_color(icon_wrap, primary, 0);
  lv_obj_set_style_shadow_width(icon_wrap, 14, 0);
  lv_obj_set_style_shadow_opa(icon_wrap, LV_OPA_40, 0);

  lv_obj_t *icon_lbl = lv_label_create(icon_wrap);
  lv_label_set_text(icon_lbl, icon_symbol);
  lv_obj_set_style_text_color(icon_lbl, primary, 0);
  lv_obj_set_style_text_font(icon_lbl, &lv_font_montserrat_20, 0);
  lv_obj_center(icon_lbl);

  lv_obj_t *title = lv_label_create(content);
  lv_label_set_text(title, title_text);
  lv_obj_set_style_text_color(title, primary, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 92);

  lv_obj_t *detail = lv_label_create(content);
  lv_label_set_text(detail, detail_text);
  lv_obj_set_width(detail, 198);
  lv_obj_set_style_text_color(detail, lv_color_hex(0x94A3B8), 0);
  lv_obj_set_style_text_opa(detail, LV_OPA_90, 0);
  lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
  lv_obj_align(detail, LV_ALIGN_TOP_MID, 0, 124);

  lv_obj_t *err_badge = lv_obj_create(scr);
  lv_obj_set_size(err_badge, 172, 24);
  lv_obj_align(err_badge, LV_ALIGN_TOP_MID, 0, show_buttons ? 216 : 266);
  lv_obj_set_style_radius(err_badge, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(err_badge, primary, 0);
  lv_obj_set_style_bg_opa(err_badge, LV_OPA_10, 0);
  lv_obj_set_style_border_color(err_badge, primary, 0);
  lv_obj_set_style_border_opa(err_badge, LV_OPA_20, 0);
  lv_obj_set_style_border_width(err_badge, 1, 0);
  lv_obj_set_style_pad_all(err_badge, 0, 0);

  lv_obj_t *err_lbl = lv_label_create(err_badge);
  lv_label_set_text(err_lbl, badge_text);
  lv_obj_set_style_text_color(err_lbl, primary, 0);
  lv_obj_set_style_text_opa(err_lbl, LV_OPA_80, 0);
  lv_obj_center(err_lbl);

  if (show_buttons) {
    lv_obj_t *retry_btn = lv_btn_create(scr);
    lv_obj_set_size(retry_btn, 208, 32);
    lv_obj_align(retry_btn, LV_ALIGN_BOTTOM_MID, 0, -46);
    lv_obj_set_style_radius(retry_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(retry_btn, primary, 0);
    lv_obj_set_style_bg_opa(retry_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(retry_btn, 0, 0);
    lv_obj_set_style_shadow_color(retry_btn, primary, 0);
    lv_obj_set_style_shadow_width(retry_btn, 16, 0);
    lv_obj_set_style_shadow_opa(retry_btn, LV_OPA_30, 0);

    lv_obj_t *retry_lbl = lv_label_create(retry_btn);
    lv_label_set_text(retry_lbl, primary_btn_text);
    lv_obj_set_style_text_color(retry_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(retry_lbl);

    lv_obj_t *menu_btn = lv_btn_create(scr);
    lv_obj_set_size(menu_btn, 208, 26);
    lv_obj_align(menu_btn, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_radius(menu_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(menu_btn, bg_soft, 0);
    lv_obj_set_style_bg_opa(menu_btn, LV_OPA_30, 0);
    lv_obj_set_style_border_color(menu_btn, primary, 0);
    lv_obj_set_style_border_opa(menu_btn, LV_OPA_30, 0);
    lv_obj_set_style_border_width(menu_btn, 1, 0);

    lv_obj_t *menu_lbl = lv_label_create(menu_btn);
    lv_label_set_text(menu_lbl, secondary_btn_text);
    lv_obj_set_style_text_color(menu_lbl, primary, 0);
    lv_obj_set_style_text_opa(menu_lbl, LV_OPA_70, 0);
    lv_obj_center(menu_lbl);
  }

  return scr;
}

static lv_obj_t *build_rejected_screen(const char *err_code, const char *detail_text) {
  const lv_color_t primary = lv_color_hex(0xFF1144);
  const lv_color_t bg_dark = lv_color_hex(0x0A0204);
  const lv_color_t bg_soft = lv_color_hex(0x1A0509);

  char badge_text[40];
  snprintf(badge_text, sizeof(badge_text), "HATA KODU: %s", err_code);

  return build_status_screen(primary, bg_dark, bg_soft, LV_SYMBOL_CLOSE, "ODEME REDDEDILDI", detail_text,
                             badge_text, "", "", false, true);
}

static lv_obj_t *build_scan_info_screen() {
  const lv_color_t primary = lv_color_hex(0x8B5CF6);
  const lv_color_t bg_dark = lv_color_hex(0x120A26);
  clear_cloud_animation_targets();

  lv_obj_t *scr = lv_obj_create(nullptr);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_radius(scr, 0, 0);
  lv_obj_set_style_bg_color(scr, bg_dark, 0);
  lv_obj_set_style_border_color(scr, primary, 0);
  lv_obj_set_style_border_width(scr, 1, 0);
  lv_obj_set_style_border_opa(scr, LV_OPA_30, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);

  lv_obj_t *glow_tl = lv_obj_create(scr);
  lv_obj_remove_style_all(glow_tl);
  lv_obj_set_size(glow_tl, 110, 110);
  lv_obj_align(glow_tl, LV_ALIGN_TOP_LEFT, -40, -35);
  apply_contrast_circle_style(glow_tl, bg_dark, primary, 34, 50);

  lv_obj_t *glow_br = lv_obj_create(scr);
  lv_obj_remove_style_all(glow_br);
  lv_obj_set_size(glow_br, 110, 110);
  lv_obj_align(glow_br, LV_ALIGN_BOTTOM_RIGHT, 40, 35);
  apply_contrast_circle_style(glow_br, bg_dark, primary, 44, 60);

  lv_obj_t *header = lv_obj_create(scr);
  lv_obj_remove_style_all(header);
  lv_obj_set_size(header, 220, 24);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t *logo = lv_label_create(header);
  lv_label_set_text(logo, LV_SYMBOL_PLAY);
  lv_obj_set_style_text_color(logo, primary, 0);
  lv_obj_align(logo, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *header_title = lv_label_create(header);
  lv_label_set_text(header_title, "Funtoria OS");
  lv_obj_set_style_text_color(header_title, primary, 0);
  lv_obj_set_style_text_opa(header_title, LV_OPA_90, 0);
  lv_obj_align(header_title, LV_ALIGN_LEFT_MID, 16, 0);

  create_wifi_indicator(header, primary);

  lv_obj_t *qr_card = lv_obj_create(scr);
  lv_obj_set_size(qr_card, 190, 190);
  lv_obj_align(qr_card, LV_ALIGN_TOP_MID, 0, 44);
  lv_obj_set_style_bg_color(qr_card, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(qr_card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(qr_card, 16, 0);
  lv_obj_set_style_border_color(qr_card, primary, 0);
  lv_obj_set_style_border_width(qr_card, 2, 0);
  lv_obj_set_style_border_opa(qr_card, LV_OPA_30, 0);
  lv_obj_set_style_shadow_color(qr_card, primary, 0);
  lv_obj_set_style_shadow_width(qr_card, 20, 0);
  lv_obj_set_style_shadow_opa(qr_card, LV_OPA_30, 0);
  lv_obj_set_style_pad_all(qr_card, 0, 0);

  lv_obj_t *qr_zone = lv_obj_create(qr_card);
  lv_obj_set_size(qr_zone, 170, 170);
  lv_obj_center(qr_zone);
  lv_obj_set_style_bg_color(qr_zone, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(qr_zone, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(qr_zone, 10, 0);
  lv_obj_set_style_border_color(qr_zone, lv_color_hex(0xDDD6FE), 0);
  lv_obj_set_style_border_width(qr_zone, 2, 0);
  lv_obj_set_style_pad_all(qr_zone, 0, 0);

  lv_obj_t *qr = lv_qrcode_create(qr_zone);
  lv_qrcode_set_size(qr, 150);
  lv_qrcode_set_dark_color(qr, lv_color_hex(0x111111));
  lv_qrcode_set_light_color(qr, lv_color_hex(0xFFFFFF));
  lv_qrcode_update(qr, "funtoria", 8U);
  lv_obj_center(qr);

  lv_obj_t *price_lbl = lv_label_create(scr);
  lv_label_set_text(price_lbl, "45 TL");
  lv_obj_set_style_text_color(price_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(price_lbl, &lv_font_montserrat_32, 0);
  lv_obj_align(price_lbl, LV_ALIGN_TOP_MID, 0, 250);

  return scr;
}

static lv_obj_t *build_scan_anim_screen() {
  const lv_color_t primary = lv_color_hex(0x8B5CF6);
  const lv_color_t bg_dark = lv_color_hex(0x120A26);
  clear_cloud_animation_targets();

  lv_obj_t *scr = lv_obj_create(nullptr);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_radius(scr, 0, 0);
  lv_obj_set_style_bg_color(scr, bg_dark, 0);
  lv_obj_set_style_border_color(scr, primary, 0);
  lv_obj_set_style_border_width(scr, 1, 0);
  lv_obj_set_style_border_opa(scr, LV_OPA_30, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);

  lv_obj_t *glow_tl = lv_obj_create(scr);
  lv_obj_remove_style_all(glow_tl);
  lv_obj_set_size(glow_tl, 110, 110);
  lv_obj_align(glow_tl, LV_ALIGN_TOP_LEFT, -40, -35);
  apply_contrast_circle_style(glow_tl, bg_dark, primary, 34, 50);

  lv_obj_t *glow_br = lv_obj_create(scr);
  lv_obj_remove_style_all(glow_br);
  lv_obj_set_size(glow_br, 110, 110);
  lv_obj_align(glow_br, LV_ALIGN_BOTTOM_RIGHT, 40, 35);
  apply_contrast_circle_style(glow_br, bg_dark, primary, 44, 60);

  lv_obj_t *header = lv_obj_create(scr);
  lv_obj_remove_style_all(header);
  lv_obj_set_size(header, 220, 24);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t *logo = lv_label_create(header);
  lv_label_set_text(logo, LV_SYMBOL_PLAY);
  lv_obj_set_style_text_color(logo, primary, 0);
  lv_obj_align(logo, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *header_title = lv_label_create(header);
  lv_label_set_text(header_title, "Funtoria OS");
  lv_obj_set_style_text_color(header_title, primary, 0);
  lv_obj_set_style_text_opa(header_title, LV_OPA_90, 0);
  lv_obj_align(header_title, LV_ALIGN_LEFT_MID, 16, 0);

  create_wifi_indicator(header, primary);

  lv_obj_t *anim_zone = lv_obj_create(scr);
  lv_obj_remove_style_all(anim_zone);
  lv_obj_set_size(anim_zone, 190, 190);
  lv_obj_align(anim_zone, LV_ALIGN_TOP_MID, 0, 44);

  struct BlobPreset {
    int16_t cx;
    int16_t cy;
    int16_t min_size;
    int16_t max_size;
    lv_opa_t start_opa;
  };

  const BlobPreset presets[] = {
      {66, 72, 58, 92, LV_OPA_50},
      {122, 70, 52, 86, LV_OPA_40},
      {56, 114, 54, 90, LV_OPA_50},
      {128, 116, 56, 94, LV_OPA_50},
      {92, 86, 62, 104, LV_OPA_60},
      {92, 132, 50, 82, LV_OPA_40},
  };

  g_cloud_blob_count = static_cast<uint8_t>(sizeof(presets) / sizeof(presets[0]));
  for (uint8_t i = 0; i < g_cloud_blob_count; i++) {
    lv_obj_t *blob = lv_obj_create(anim_zone);
    lv_obj_remove_style_all(blob);
    lv_obj_set_style_radius(blob, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(blob, 0, 0);
    lv_obj_set_style_bg_color(blob, lv_color_hex(0x8B5CF6), 0);
    lv_obj_set_style_bg_opa(blob, presets[i].start_opa, 0);
    lv_obj_set_style_shadow_color(blob, lv_color_hex(0x8B5CF6), 0);
    lv_obj_set_style_shadow_width(blob, 24, 0);
    lv_obj_set_style_shadow_opa(blob, LV_OPA_30, 0);

    g_cloud_blobs[i] = {blob, presets[i].cx, presets[i].cy, presets[i].min_size, presets[i].max_size};
    cloud_blob_anim_exec(&g_cloud_blobs[i], presets[i].min_size);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &g_cloud_blobs[i]);
    lv_anim_set_exec_cb(&a, cloud_blob_anim_exec);
    lv_anim_set_values(&a, presets[i].min_size, presets[i].max_size);
    lv_anim_set_time(&a, 1500 + i * 180);
    lv_anim_set_playback_time(&a, 1450 + i * 160);
    lv_anim_set_delay(&a, i * 90);
    lv_anim_set_repeat_delay(&a, i * 20);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
  }

  g_cloud_color_timer = lv_timer_create(cloud_color_timer_cb, 260, nullptr);
  cloud_color_timer_cb(nullptr);

  lv_obj_t *price_lbl = lv_label_create(scr);
  lv_label_set_text(price_lbl, "45 TL");
  lv_obj_set_style_text_color(price_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(price_lbl, &lv_font_montserrat_32, 0);
  lv_obj_align(price_lbl, LV_ALIGN_TOP_MID, 0, 250);

  return scr;
}

static lv_obj_t *build_success_info_screen() {
  return build_status_screen(lv_color_hex(0x00D084), lv_color_hex(0x061B15), lv_color_hex(0x0F2B23), LV_SYMBOL_OK,
                             "ODEME BASARILI", "ODEMENIZ ALINDI. OYUN HAZIR.",
                             "DURUM: ONAYLANDI", "", "", false, true);
}

static lv_obj_t *build_processing_screen() {
  return build_status_screen(lv_color_hex(0x3B82F6), lv_color_hex(0x040A16), lv_color_hex(0x0D1C33),
                             LV_SYMBOL_REFRESH, "ODEME ISLENIYOR", "LUTFEN BEKLEYIN, ISLEM DEVAM EDIYOR.",
                             "DURUM: ISLENIYOR", "", "", false, true);
}

static lv_obj_t *build_waiting_card_screen() {
  return build_status_screen(lv_color_hex(0xA855F7), lv_color_hex(0x13071D), lv_color_hex(0x221031), LV_SYMBOL_USB,
                             "KART OKUNUYOR", "KART BILGISI DOGRULANIYOR. CIKARMAYIN.",
                             "DURUM: KART KONTROL", "", "", false, true);
}

static lv_obj_t *build_network_error_screen() {
  return build_status_screen(lv_color_hex(0xF97316), lv_color_hex(0x1B0D05), lv_color_hex(0x2E1A0D),
                             LV_SYMBOL_WARNING, "AG HATASI", "SUNUCUYA ULASILAMADI. BAGLANTIYI KONTROL EDIN.",
                             "HATA KODU: NET_001", "", "", false, true);
}

static lv_obj_t *build_session_done_screen() {
  return build_status_screen(lv_color_hex(0x14B8A6), lv_color_hex(0x041411), lv_color_hex(0x0D2420), LV_SYMBOL_OK,
                             "SEANS TAMAMLANDI", "OTURUMUNUZ BASARIYLA TAMAMLANDI.",
                             "DURUM: TAMAMLANDI", "", "", false, true);
}

static uint32_t lighten_hex_color(uint32_t color, uint8_t percent) {
  const uint8_t r = static_cast<uint8_t>((color >> 16) & 0xFF);
  const uint8_t g = static_cast<uint8_t>((color >> 8) & 0xFF);
  const uint8_t b = static_cast<uint8_t>(color & 0xFF);

  const uint8_t out_r = static_cast<uint8_t>(r + ((255U - r) * percent) / 100U);
  const uint8_t out_g = static_cast<uint8_t>(g + ((255U - g) * percent) / 100U);
  const uint8_t out_b = static_cast<uint8_t>(b + ((255U - b) * percent) / 100U);
  return (static_cast<uint32_t>(out_r) << 16) | (static_cast<uint32_t>(out_g) << 8) | out_b;
}

static uint32_t darken_hex_color(uint32_t color, uint8_t percent) {
  const uint8_t r = static_cast<uint8_t>((color >> 16) & 0xFF);
  const uint8_t g = static_cast<uint8_t>((color >> 8) & 0xFF);
  const uint8_t b = static_cast<uint8_t>(color & 0xFF);

  const uint8_t out_r = static_cast<uint8_t>((r * (100U - percent)) / 100U);
  const uint8_t out_g = static_cast<uint8_t>((g * (100U - percent)) / 100U);
  const uint8_t out_b = static_cast<uint8_t>((b * (100U - percent)) / 100U);
  return (static_cast<uint32_t>(out_r) << 16) | (static_cast<uint32_t>(out_g) << 8) | out_b;
}

static uint8_t luma_from_hex_color(uint32_t color) {
  const uint16_t r = static_cast<uint16_t>((color >> 16) & 0xFF);
  const uint16_t g = static_cast<uint16_t>((color >> 8) & 0xFF);
  const uint16_t b = static_cast<uint16_t>(color & 0xFF);
  return static_cast<uint8_t>((r * 30U + g * 59U + b * 11U) / 100U);
}

static lv_obj_t *build_pattern_color_screen(uint32_t base_hex) {
  clear_cloud_animation_targets();
  clear_wifi_indicator_targets();

  const bool use_darker_tones = luma_from_hex_color(base_hex) >= 140U;
  const uint32_t tone_1 = use_darker_tones ? darken_hex_color(base_hex, 16) : lighten_hex_color(base_hex, 18);
  const uint32_t tone_2 = use_darker_tones ? darken_hex_color(base_hex, 30) : lighten_hex_color(base_hex, 32);
  const uint32_t tone_3 = use_darker_tones ? darken_hex_color(base_hex, 44) : lighten_hex_color(base_hex, 46);

  lv_obj_t *scr = lv_obj_create(nullptr);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_radius(scr, 0, 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_set_style_bg_color(scr, lv_color_hex(base_hex), 0);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(base_hex), 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_NONE, 0);

  lv_obj_t *blob_top = lv_obj_create(scr);
  lv_obj_remove_style_all(blob_top);
  lv_obj_set_size(blob_top, 210, 210);
  lv_obj_align(blob_top, LV_ALIGN_TOP_RIGHT, 88, -100);
  lv_obj_set_style_radius(blob_top, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(blob_top, lv_color_hex(tone_2), 0);
  lv_obj_set_style_bg_opa(blob_top, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(blob_top, lv_color_hex(tone_3), 0);
  lv_obj_set_style_border_width(blob_top, 2, 0);
  lv_obj_set_style_border_opa(blob_top, LV_OPA_COVER, 0);

  lv_obj_t *blob_bottom = lv_obj_create(scr);
  lv_obj_remove_style_all(blob_bottom);
  lv_obj_set_size(blob_bottom, 200, 200);
  lv_obj_align(blob_bottom, LV_ALIGN_BOTTOM_LEFT, -84, 92);
  lv_obj_set_style_radius(blob_bottom, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(blob_bottom, lv_color_hex(tone_3), 0);
  lv_obj_set_style_bg_opa(blob_bottom, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(blob_bottom, lv_color_hex(tone_1), 0);
  lv_obj_set_style_border_width(blob_bottom, 2, 0);
  lv_obj_set_style_border_opa(blob_bottom, LV_OPA_COVER, 0);

  lv_obj_t *blob_mid = lv_obj_create(scr);
  lv_obj_remove_style_all(blob_mid);
  lv_obj_set_size(blob_mid, 130, 130);
  lv_obj_align(blob_mid, LV_ALIGN_CENTER, 34, -10);
  lv_obj_set_style_radius(blob_mid, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(blob_mid, lv_color_hex(tone_1), 0);
  lv_obj_set_style_bg_opa(blob_mid, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(blob_mid, lv_color_hex(tone_3), 0);
  lv_obj_set_style_border_width(blob_mid, 2, 0);
  lv_obj_set_style_border_opa(blob_mid, LV_OPA_COVER, 0);

  lv_obj_t *blob_small = lv_obj_create(scr);
  lv_obj_remove_style_all(blob_small);
  lv_obj_set_size(blob_small, 82, 82);
  lv_obj_align(blob_small, LV_ALIGN_CENTER, -64, 64);
  lv_obj_set_style_radius(blob_small, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(blob_small, lv_color_hex(tone_3), 0);
  lv_obj_set_style_bg_opa(blob_small, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(blob_small, lv_color_hex(tone_1), 0);
  lv_obj_set_style_border_width(blob_small, 2, 0);
  lv_obj_set_style_border_opa(blob_small, LV_OPA_COVER, 0);

  return scr;
}

static lv_obj_t *create_screen_by_index(uint8_t index) {
  clear_cloud_animation_targets();
  clear_wifi_indicator_targets();

  switch (index % ACTIVE_SCREEN_COUNT) {
    case 0:
      return build_scan_info_screen();
    case 1:
      return build_scan_anim_screen();
    case 2:
      return build_processing_screen();
    case 3:
      return build_waiting_card_screen();
    case 4:
      return build_success_info_screen();
    case 5:
      return build_session_done_screen();
    case 6:
      return build_network_error_screen();
    case 7:
      return build_rejected_screen("PAY_402", "ISLEM LIMITI ASILDI.");
    default:
      return build_pattern_color_screen(0x8B5CF6);
  }
}

static void screen_swap_cb(lv_timer_t *timer) {
  LV_UNUSED(timer);
  lv_obj_t *old = lv_scr_act();
  g_screen_index = static_cast<uint8_t>((g_screen_index + 1U) % ACTIVE_SCREEN_COUNT);
  lv_obj_t *next = create_screen_by_index(g_screen_index);
  if (next == nullptr) {
    return;
  }

  lv_scr_load(next);
  if (old != nullptr) {
    lv_obj_del(old);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  lv_init();

  tft.begin();
  tft.setRotation(0);

#ifdef TFT_BL
  ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQ, BACKLIGHT_RES_BITS);
  ledcAttachPin(TFT_BL, BACKLIGHT_CHANNEL);
  set_backlight_percent(100);
#endif

  run_tft_startup_probe();
  Serial.println("TFT startup probe completed");

  static uint16_t buf[240 * 20];
  lv_display_t *disp = lv_display_create(240, 320);
  if (disp == nullptr) {
    return;
  }
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, buf, nullptr, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

  g_screen_index = BOOT_SCREEN_INDEX;
  lv_obj_t *first = create_screen_by_index(g_screen_index);
  if (first != nullptr) {
    lv_scr_load(first);
  }
  lv_timer_create(screen_swap_cb, SCREEN_SWAP_MS, nullptr);

  last_tick_ms = millis();
}

void loop() {
  const uint32_t now = millis();
  lv_tick_inc(now - last_tick_ms);
  last_tick_ms = now;

  lv_timer_handler();
  delay(5);
}
