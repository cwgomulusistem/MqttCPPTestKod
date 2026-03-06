#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

void FuntoriaScreen_showCustomText(const FuntoriaScreenContext *ctx,
                                   const char *line1, const char *line2) {
  funtoria_screen_apply(ctx, lv_color_hex(0x120A26), lv_color_hex(0xA855F7),
                        lv_color_hex(0xF8FAFC), lv_color_hex(0xC4B5FD),
                        "Funtoria OS", line1 ? line1 : "", line2 ? line2 : "");
}
