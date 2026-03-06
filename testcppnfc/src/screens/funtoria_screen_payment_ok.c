#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

#include <stdio.h>

void FuntoriaScreen_showPaymentOk(const FuntoriaScreenContext *ctx,
                                  const char *userId, uint32_t balance) {
  char bodyText[128] = {0};

  if (balance > 0) {
    const float balanceTl = balance / 100.0f;
    snprintf(bodyText, sizeof(bodyText),
             "ODEMENIZ ALINDI. BAKIYE: %.2f TL\nKULLANICI: %s", balanceTl,
             userId ? userId : "-");
  } else {
    snprintf(bodyText, sizeof(bodyText), "ODEMENIZ ALINDI. OYUN HAZIR.\nKULLANICI: %s",
             userId ? userId : "-");
  }

  funtoria_screen_apply_status(
      ctx, 0x00D084, 0x061B15, 0x0F2B23, LV_SYMBOL_OK, "ODEME BASARILI",
      bodyText, "DURUM: ONAYLANDI", "", "", false, true);
}
