#include <cstdint>

#include <lvgl.h>
#include <lvgl/src/drivers/sdl/lv_sdl_window.h>

extern "C" {
#include "../src/screens/FuntoriaScreenContext.h"
#include "../src/screens/FuntoriaScreens.h"
}

namespace {
constexpr int32_t kScreenWidth = 240;
constexpr int32_t kScreenHeight = 320;
constexpr uint32_t kScreenCycleMs = 1800;

struct UiState {
  lv_obj_t *root;
  lv_obj_t *title;
  lv_obj_t *body;
  lv_obj_t *footer;
};

UiState g_ui = {};

FuntoriaScreenContext makeContext() {
  FuntoriaScreenContext ctx = {};
  ctx.root = g_ui.root;
  ctx.title = g_ui.title;
  ctx.body = g_ui.body;
  ctx.footer = g_ui.footer;
  return ctx;
}

void createUi() {
  lv_obj_t *active = nullptr;
#if LVGL_VERSION_MAJOR >= 9
  active = lv_screen_active();
#else
  active = lv_scr_act();
#endif

  g_ui.root = lv_obj_create(active);
  lv_obj_remove_style_all(g_ui.root);
  lv_obj_set_size(g_ui.root, kScreenWidth, kScreenHeight);
  lv_obj_set_style_bg_opa(g_ui.root, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(g_ui.root, lv_color_hex(0x13071D), 0);
  lv_obj_center(g_ui.root);

  g_ui.title = lv_label_create(g_ui.root);
  lv_obj_set_width(g_ui.title, kScreenWidth - 20);
  lv_obj_set_style_text_align(g_ui.title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(g_ui.title, lv_color_hex(0xA855F7), 0);
  lv_obj_set_style_text_font(g_ui.title, &lv_font_montserrat_20, 0);
  lv_obj_align(g_ui.title, LV_ALIGN_TOP_MID, 0, 30);

  g_ui.body = lv_label_create(g_ui.root);
  lv_obj_set_width(g_ui.body, kScreenWidth - 20);
  lv_obj_set_style_text_align(g_ui.body, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(g_ui.body, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(g_ui.body, &lv_font_montserrat_16, 0);
  lv_obj_align(g_ui.body, LV_ALIGN_CENTER, 0, 0);

  g_ui.footer = lv_label_create(g_ui.root);
  lv_obj_set_width(g_ui.footer, kScreenWidth - 20);
  lv_obj_set_style_text_align(g_ui.footer, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(g_ui.footer, lv_color_hex(0x94A3B8), 0);
  lv_obj_set_style_text_font(g_ui.footer, &lv_font_montserrat_14, 0);
  lv_obj_align(g_ui.footer, LV_ALIGN_BOTTOM_MID, 0, -30);
}

void showByStep(uint8_t step) {
  FuntoriaScreenContext ctx = makeContext();

  switch (step % 7U) {
  case 0:
    FuntoriaScreen_showWaiting(&ctx);
    break;
  case 1:
    FuntoriaScreen_showNfcRead(&ctx, "04A1B2C3", "MIFARE_CLASSIC");
    break;
  case 2:
    FuntoriaScreen_showPaymentOk(&ctx, "U1024", 12450);
    break;
  case 3:
    FuntoriaScreen_showPaymentFail(&ctx, "YETERSIZ BAKIYE");
    break;
  case 4:
    FuntoriaScreen_showPhoneDetected(&ctx);
    break;
  case 5:
    FuntoriaScreen_showSetupRequired(&ctx, "FN-ESP32-001", "A1B2C3D4E5F6");
    break;
  default:
    FuntoriaScreen_showError(&ctx, "NFC TIMEOUT");
    break;
  }
}

void cycleScreens(lv_timer_t *timer) {
  uint8_t *step = static_cast<uint8_t *>(lv_timer_get_user_data(timer));
  if (!step) {
    return;
  }

  showByStep(*step);
  *step = static_cast<uint8_t>((*step + 1U) % 7U);
}
} // namespace

int main() {
  lv_init();

  lv_display_t *display = lv_sdl_window_create(kScreenWidth, kScreenHeight);
  if (!display) {
    return 1;
  }

  lv_sdl_window_set_title(display, "Funtoria SDL Screen Simulator");
  lv_sdl_window_set_resizeable(display, false);

  createUi();

  static uint8_t screenStep = 0;
  showByStep(screenStep);
  screenStep = 1;
  lv_timer_create(cycleScreens, kScreenCycleMs, &screenStep);

  while (true) {
    uint32_t sleepMs = lv_timer_handler();
    if (sleepMs > 16U) {
      sleepMs = 16U;
    }
    lv_delay_ms(sleepMs);
  }
}

