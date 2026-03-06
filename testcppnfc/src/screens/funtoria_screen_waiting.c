#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

void FuntoriaScreen_showWaiting(const FuntoriaScreenContext *ctx) {
  funtoria_screen_apply_status(
      ctx, 0xA855F7, 0x13071D, 0x221031, LV_SYMBOL_USB, "KART OKUNUYOR",
      "KART BILGISI DOGRULANIYOR. CIKARMAYIN.", "DURUM: KART KONTROL", "", "",
      false, true);
}
