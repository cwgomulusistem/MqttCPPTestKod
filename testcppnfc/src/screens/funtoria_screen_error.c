#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

void FuntoriaScreen_showError(const FuntoriaScreenContext *ctx,
                              const char *errorText) {
  funtoria_screen_apply(ctx, lv_color_hex(0x1B0D05), lv_color_hex(0xF97316),
                        lv_color_hex(0xFFEDD5), lv_color_hex(0xFDBA74),
                        LV_SYMBOL_WARNING " AG/SISTEM HATASI",
                        errorText ? errorText : "Bilinmeyen hata",
                        "ERROR SCREEN");
}
