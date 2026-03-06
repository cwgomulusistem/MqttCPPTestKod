#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

#include <stdio.h>

void FuntoriaScreen_showNfcRead(const FuntoriaScreenContext *ctx, const char *uid,
                                const char *type) {
  char detailText[128] = {0};

  snprintf(detailText, sizeof(detailText), "UID: %s\nTIP: %s", uid ? uid : "-",
           type ? type : "-");

  funtoria_screen_apply_status(
      ctx, 0x8B5CF6, 0x120A26, 0x221031, LV_SYMBOL_WIFI, "KART ALGILANDI",
      detailText, "DURUM: KART OKUNDU", "", "", false, true);
}
