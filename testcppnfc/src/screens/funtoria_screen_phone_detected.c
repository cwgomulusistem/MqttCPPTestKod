#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

void FuntoriaScreen_showPhoneDetected(const FuntoriaScreenContext *ctx) {
  funtoria_screen_apply(ctx, lv_color_hex(0x0D1C33), lv_color_hex(0x3B82F6),
                        lv_color_hex(0xEFF6FF), lv_color_hex(0x93C5FD),
                        LV_SYMBOL_CALL " TELEFON ALGILANDI",
                        "Uygulamayi acin\nve tekrar yaklastirin",
                        "PHONE DETECTED SCREEN");
}
