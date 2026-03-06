#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

#include <stdio.h>

void FuntoriaScreen_showSetupRequired(const FuntoriaScreenContext *ctx,
                                      const char *deviceId, const char *mac) {
  char bodyText[96] = {0};
  char footerText[96] = {0};

  snprintf(bodyText, sizeof(bodyText), "Setup kart bekleniyor\nID: %s",
           deviceId ? deviceId : "-");
  snprintf(footerText, sizeof(footerText), "MAC: %s", mac ? mac : "-");

  funtoria_screen_apply(ctx, lv_color_hex(0x131A2C), lv_color_hex(0x38BDF8),
                        lv_color_hex(0xF8FAFC), lv_color_hex(0x93C5FD),
                        "ILK KURULUM", bodyText, footerText);
}
