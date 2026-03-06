#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

#include <stdio.h>

void FuntoriaScreen_showNfcRead(const FuntoriaScreenContext *ctx, const char *uid,
                                const char *type) {
  char bodyText[96] = {0};
  char footerText[64] = {0};

  snprintf(bodyText, sizeof(bodyText), "UID:\n%s", uid ? uid : "-");
  snprintf(footerText, sizeof(footerText), "NFC READ | %s", type ? type : "-");

  funtoria_screen_apply(ctx, lv_color_hex(0x130A2A), lv_color_hex(0x8B5CF6),
                        lv_color_hex(0xFFFFFF), lv_color_hex(0xC4B5FD),
                        LV_SYMBOL_WIFI " KART OKUNDU", bodyText, footerText);
}
