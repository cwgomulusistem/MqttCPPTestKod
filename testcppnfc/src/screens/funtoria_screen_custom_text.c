#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

void FuntoriaScreen_showCustomText(const FuntoriaScreenContext *ctx,
                                   const char *line1, const char *line2) {
  funtoria_screen_apply_status(
      ctx, 0xA855F7, 0x120A26, 0x221031, LV_SYMBOL_BELL,
      line1 ? line1 : "Funtoria OS", line2 ? line2 : "", "DURUM: OZEL MESAJ",
      "", "", false, true);
}
