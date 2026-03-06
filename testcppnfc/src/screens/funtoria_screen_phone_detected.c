#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

void FuntoriaScreen_showPhoneDetected(const FuntoriaScreenContext *ctx) {
  funtoria_screen_apply_status(
      ctx, 0x3B82F6, 0x040A16, 0x0D1C33, LV_SYMBOL_REFRESH,
      "TELEFON ALGILANDI", "FUNTORIA APP ACILISI BEKLENIYOR.",
      "DURUM: BEKLENIYOR", "", "", false, true);
}
