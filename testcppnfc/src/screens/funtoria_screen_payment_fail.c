#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

void FuntoriaScreen_showPaymentFail(const FuntoriaScreenContext *ctx,
                                    const char *reason) {
  funtoria_screen_apply(ctx, lv_color_hex(0x1A0509), lv_color_hex(0xFF1144),
                        lv_color_hex(0xFECACA), lv_color_hex(0xFCA5A5),
                        LV_SYMBOL_CLOSE " ODEME REDDEDILDI",
                        reason ? reason : "Islem reddedildi",
                        "PAYMENT FAIL SCREEN");
}
