#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

#include <stdio.h>

void FuntoriaScreen_showPaymentOk(const FuntoriaScreenContext *ctx,
                                  const char *userId, uint32_t balance) {
  char bodyText[96] = {0};
  char footerText[64] = {0};

  if (balance > 0) {
    const float balanceTl = balance / 100.0f;
    snprintf(bodyText, sizeof(bodyText), "Bakiye: %.2f TL", balanceTl);
  } else {
    snprintf(bodyText, sizeof(bodyText), "Islem tamamlandi");
  }

  snprintf(footerText, sizeof(footerText), "PAYMENT OK | %s",
           userId ? userId : "-");

  funtoria_screen_apply(ctx, lv_color_hex(0x061B15), lv_color_hex(0x00D084),
                        lv_color_hex(0xF8FAFC), lv_color_hex(0x86EFAC),
                        LV_SYMBOL_OK " ODEME BASARILI", bodyText, footerText);
}
