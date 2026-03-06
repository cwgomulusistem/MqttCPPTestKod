#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

#include <stdio.h>

void FuntoriaScreen_showSetupRequired(const FuntoriaScreenContext *ctx,
                                      const char *deviceId, const char *mac) {
  char bodyText[160] = {0};

  snprintf(bodyText, sizeof(bodyText),
           "SETUP KARTI BEKLENIYOR.\nDEVICE ID: %s\nMAC: %s",
           deviceId ? deviceId : "-", mac ? mac : "-");

  funtoria_screen_apply_status(ctx, 0x38BDF8, 0x131A2C, 0x1D2D45,
                               LV_SYMBOL_SETTINGS, "ILK KURULUM", bodyText,
                               "DURUM: SETUP GEREKLI", "", "", false, true);
}
