#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

void FuntoriaScreen_showWaiting(const FuntoriaScreenContext *ctx) {
  funtoria_screen_apply(ctx, lv_color_hex(0x13071D), lv_color_hex(0xA855F7),
                        lv_color_hex(0xF8FAFC), lv_color_hex(0xC4B5FD),
                        "Funtoria OS", LV_SYMBOL_USB " Kartinizi Okutun",
                        "WAITING SCREEN");
}
