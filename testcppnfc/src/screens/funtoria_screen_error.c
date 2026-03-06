#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

void FuntoriaScreen_showError(const FuntoriaScreenContext *ctx,
                              const char *errorText) {
  funtoria_screen_apply_status(
      ctx, 0xF97316, 0x1B0D05, 0x2E1A0D, LV_SYMBOL_WARNING, "AG HATASI",
      errorText ? errorText : "SUNUCUYA ULASILAMADI. BAGLANTIYI KONTROL EDIN.",
      "HATA KODU: NET_001", "", "", false, true);
}
