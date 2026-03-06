#ifndef FUNTORIA_SCREENS_H
#define FUNTORIA_SCREENS_H

#include "FuntoriaScreenContext.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void FuntoriaScreen_showWaiting(const FuntoriaScreenContext *ctx);
void FuntoriaScreen_showNfcRead(const FuntoriaScreenContext *ctx, const char *uid,
                                const char *type);
void FuntoriaScreen_showPaymentOk(const FuntoriaScreenContext *ctx,
                                  const char *userId, uint32_t balance);
void FuntoriaScreen_showPaymentFail(const FuntoriaScreenContext *ctx,
                                    const char *reason);
void FuntoriaScreen_showError(const FuntoriaScreenContext *ctx,
                              const char *errorText);
void FuntoriaScreen_showPhoneDetected(const FuntoriaScreenContext *ctx);
void FuntoriaScreen_showSetupRequired(const FuntoriaScreenContext *ctx,
                                      const char *deviceId, const char *mac);
void FuntoriaScreen_showCustomText(const FuntoriaScreenContext *ctx,
                                   const char *line1, const char *line2);

#ifdef __cplusplus
}
#endif

#endif // FUNTORIA_SCREENS_H
