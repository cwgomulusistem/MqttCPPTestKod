#include "FuntoriaScreens.h"
#include "funtoria_screen_style.h"

void FuntoriaScreen_showWaiting(const FuntoriaScreenContext *ctx) {
  funtoria_screen_apply_waiting_qr(ctx);
}
