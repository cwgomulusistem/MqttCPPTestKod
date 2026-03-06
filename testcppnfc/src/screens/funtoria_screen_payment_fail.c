#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

void FuntoriaScreen_showPaymentFail(const FuntoriaScreenContext *ctx,
                                    const char *reason) {
  funtoria_screen_apply_status(
      ctx, 0xFF1144, 0x0A0204, 0x1A0509, LV_SYMBOL_CLOSE, "ODEME REDDEDILDI",
      reason ? reason : "ISLEM REDDEDILDI", "HATA KODU: AUTH_401", "", "",
      false, true);
}
