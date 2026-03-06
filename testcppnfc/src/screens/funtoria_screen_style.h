#ifndef FUNTORIA_SCREEN_STYLE_H
#define FUNTORIA_SCREEN_STYLE_H

#include "FuntoriaScreenContext.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline const char *funtoria_safe_text(const char *text) {
  return text ? text : "";
}

static inline void funtoria_screen_apply(const FuntoriaScreenContext *ctx,
                                         lv_color_t bgColor,
                                         lv_color_t titleColor,
                                         lv_color_t bodyColor,
                                         lv_color_t footerColor,
                                         const char *titleText,
                                         const char *bodyText,
                                         const char *footerText) {
  if (!ctx || !ctx->root || !ctx->title || !ctx->body || !ctx->footer) {
    return;
  }

  lv_obj_set_style_bg_color(ctx->root, bgColor, 0);
  lv_obj_set_style_text_color(ctx->title, titleColor, 0);
  lv_obj_set_style_text_color(ctx->body, bodyColor, 0);
  lv_obj_set_style_text_color(ctx->footer, footerColor, 0);

  lv_label_set_text(ctx->title, funtoria_safe_text(titleText));
  lv_label_set_text(ctx->body, funtoria_safe_text(bodyText));
  lv_label_set_text(ctx->footer, funtoria_safe_text(footerText));
}

#ifdef __cplusplus
}
#endif

#endif // FUNTORIA_SCREEN_STYLE_H
