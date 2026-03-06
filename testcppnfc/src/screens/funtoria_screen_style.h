#ifndef FUNTORIA_SCREEN_STYLE_H
#define FUNTORIA_SCREEN_STYLE_H

#include "FuntoriaScreenContext.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline const char *funtoria_safe_text(const char *text) {
  return text ? text : "";
}

static inline bool funtoria_is_reserved_label(const FuntoriaScreenContext *ctx,
                                              const lv_obj_t *obj) {
  return obj == ctx->title || obj == ctx->body || obj == ctx->footer;
}

static inline void
funtoria_set_base_labels_hidden(const FuntoriaScreenContext *ctx, bool hidden) {
  if (!ctx) {
    return;
  }

  lv_obj_t *labels[] = {ctx->title, ctx->body, ctx->footer};
  for (uint8_t i = 0; i < 3; i++) {
    if (!labels[i]) {
      continue;
    }

    if (hidden) {
      lv_obj_add_flag(labels[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(labels[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static inline void funtoria_clear_dynamic_children(const FuntoriaScreenContext *ctx) {
  if (!ctx || !ctx->root) {
    return;
  }

  uint32_t child_count = lv_obj_get_child_count(ctx->root);
  while (child_count > 0U) {
    child_count--;
    lv_obj_t *child = lv_obj_get_child(ctx->root, child_count);
    if (!funtoria_is_reserved_label(ctx, child)) {
      lv_obj_del(child);
    }
  }
}

static inline void funtoria_create_wifi_indicator(lv_obj_t *parent, lv_color_t color) {
  lv_obj_t *wifi_wrap = lv_obj_create(parent);
  lv_obj_remove_style_all(wifi_wrap);
  lv_obj_set_size(wifi_wrap, 34, 18);
  lv_obj_align(wifi_wrap, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_radius(wifi_wrap, 5, 0);
  lv_obj_set_style_bg_color(wifi_wrap, color, 0);
  lv_obj_set_style_bg_opa(wifi_wrap, LV_OPA_10, 0);
  lv_obj_set_style_border_color(wifi_wrap, color, 0);
  lv_obj_set_style_border_opa(wifi_wrap, LV_OPA_40, 0);
  lv_obj_set_style_border_width(wifi_wrap, 1, 0);
  lv_obj_set_style_pad_all(wifi_wrap, 0, 0);

  lv_obj_t *bar0 = lv_obj_create(wifi_wrap);
  lv_obj_remove_style_all(bar0);
  lv_obj_set_size(bar0, 6, 6);
  lv_obj_align(bar0, LV_ALIGN_BOTTOM_LEFT, 4, -1);
  lv_obj_set_style_radius(bar0, 2, 0);
  lv_obj_set_style_bg_color(bar0, color, 0);

  lv_obj_t *bar1 = lv_obj_create(wifi_wrap);
  lv_obj_remove_style_all(bar1);
  lv_obj_set_size(bar1, 6, 10);
  lv_obj_align(bar1, LV_ALIGN_BOTTOM_LEFT, 14, -1);
  lv_obj_set_style_radius(bar1, 2, 0);
  lv_obj_set_style_bg_color(bar1, color, 0);

  lv_obj_t *bar2 = lv_obj_create(wifi_wrap);
  lv_obj_remove_style_all(bar2);
  lv_obj_set_size(bar2, 6, 14);
  lv_obj_align(bar2, LV_ALIGN_BOTTOM_LEFT, 24, -1);
  lv_obj_set_style_radius(bar2, 2, 0);
  lv_obj_set_style_bg_color(bar2, color, 0);
}

static inline void funtoria_apply_contrast_circle_style(lv_obj_t *obj, lv_color_t bg_color,
                                                        lv_color_t seed_color,
                                                        uint8_t fill_strength_percent,
                                                        uint8_t border_strength_percent) {
  const lv_opa_t fill_strength = (lv_opa_t)((255U * fill_strength_percent) / 100U);
  const lv_opa_t border_strength = (lv_opa_t)((255U * border_strength_percent) / 100U);
  lv_color_t fill_color = seed_color;
  lv_color_t border_color = seed_color;

  if (lv_color_brightness(bg_color) >= 128U) {
    fill_color = lv_color_darken(seed_color, fill_strength);
    border_color = lv_color_darken(seed_color, border_strength);
  } else {
    fill_color = lv_color_lighten(seed_color, fill_strength);
    border_color = lv_color_lighten(seed_color, border_strength);
  }

  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(obj, fill_color, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, border_color, 0);
  lv_obj_set_style_border_width(obj, 2, 0);
  lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
}

static inline void
funtoria_screen_apply_status(const FuntoriaScreenContext *ctx, uint32_t primary_hex,
                             uint32_t bg_dark_hex, uint32_t bg_soft_hex,
                             const char *icon_symbol, const char *title_text,
                             const char *detail_text, const char *badge_text,
                             const char *primary_btn_text,
                             const char *secondary_btn_text, bool show_buttons,
                             bool use_funtoria_header) {
  if (!ctx || !ctx->root) {
    return;
  }

  const lv_color_t primary = lv_color_hex(primary_hex);
  const lv_color_t bg_dark = lv_color_hex(bg_dark_hex);
  const lv_color_t bg_soft = lv_color_hex(bg_soft_hex);

  funtoria_set_base_labels_hidden(ctx, true);
  funtoria_clear_dynamic_children(ctx);

  lv_obj_set_style_radius(ctx->root, 0, 0);
  lv_obj_set_style_bg_color(ctx->root, bg_dark, 0);
  lv_obj_set_style_border_color(ctx->root, primary, 0);
  lv_obj_set_style_border_width(ctx->root, 1, 0);
  lv_obj_set_style_border_opa(ctx->root, LV_OPA_30, 0);
  lv_obj_set_style_pad_all(ctx->root, 0, 0);

  lv_obj_t *glow_tl = lv_obj_create(ctx->root);
  lv_obj_remove_style_all(glow_tl);
  lv_obj_set_size(glow_tl, 110, 110);
  lv_obj_align(glow_tl, LV_ALIGN_TOP_LEFT, -40, -35);
  funtoria_apply_contrast_circle_style(glow_tl, bg_dark, primary, 34, 50);

  lv_obj_t *glow_br = lv_obj_create(ctx->root);
  lv_obj_remove_style_all(glow_br);
  lv_obj_set_size(glow_br, 110, 110);
  lv_obj_align(glow_br, LV_ALIGN_BOTTOM_RIGHT, 40, 35);
  funtoria_apply_contrast_circle_style(glow_br, bg_dark, primary, 44, 60);

  lv_obj_t *header = lv_obj_create(ctx->root);
  lv_obj_remove_style_all(header);
  lv_obj_set_size(header, 220, 24);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);

  if (use_funtoria_header) {
    lv_obj_t *logo = lv_label_create(header);
    lv_label_set_text(logo, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(logo, primary, 0);
    lv_obj_align(logo, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *header_title = lv_label_create(header);
    lv_label_set_text(header_title, "Funtoria OS");
    lv_obj_set_style_text_color(header_title, primary, 0);
    lv_obj_set_style_text_opa(header_title, LV_OPA_90, 0);
    lv_obj_align(header_title, LV_ALIGN_LEFT_MID, 16, 0);

    funtoria_create_wifi_indicator(header, primary);
  } else {
    lv_obj_t *back_icon = lv_label_create(header);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_icon, primary, 0);
    lv_obj_align(back_icon, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *header_title = lv_label_create(header);
    lv_label_set_text(header_title, "ARCADE SYSTEM");
    lv_obj_set_style_text_color(header_title, primary, 0);
    lv_obj_set_style_text_opa(header_title, LV_OPA_90, 0);
    lv_obj_align(header_title, LV_ALIGN_CENTER, 0, 0);
  }

  lv_obj_t *content = lv_obj_create(ctx->root);
  lv_obj_remove_style_all(content);
  lv_obj_set_size(content, 220, 174);
  lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 34);

  lv_obj_t *outer = lv_obj_create(content);
  lv_obj_set_size(outer, 96, 96);
  lv_obj_align(outer, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_radius(outer, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(outer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(outer, primary, 0);
  lv_obj_set_style_border_opa(outer, LV_OPA_20, 0);
  lv_obj_set_style_border_width(outer, 1, 0);

  lv_obj_t *mid = lv_obj_create(content);
  lv_obj_set_size(mid, 80, 80);
  lv_obj_align(mid, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_radius(mid, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(mid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(mid, primary, 0);
  lv_obj_set_style_border_opa(mid, LV_OPA_40, 0);
  lv_obj_set_style_border_width(mid, 1, 0);

  lv_obj_t *icon_wrap = lv_obj_create(content);
  lv_obj_set_size(icon_wrap, 64, 64);
  lv_obj_align(icon_wrap, LV_ALIGN_TOP_MID, 0, 16);
  funtoria_apply_contrast_circle_style(icon_wrap, bg_dark, primary, 24, 40);
  lv_obj_set_style_shadow_color(icon_wrap, primary, 0);
  lv_obj_set_style_shadow_width(icon_wrap, 14, 0);
  lv_obj_set_style_shadow_opa(icon_wrap, LV_OPA_40, 0);

  lv_obj_t *icon_lbl = lv_label_create(icon_wrap);
  lv_label_set_text(icon_lbl, funtoria_safe_text(icon_symbol));
  lv_obj_set_style_text_color(icon_lbl, primary, 0);
  lv_obj_set_style_text_font(icon_lbl, &lv_font_montserrat_20, 0);
  lv_obj_center(icon_lbl);

  lv_obj_t *title = lv_label_create(content);
  lv_label_set_text(title, funtoria_safe_text(title_text));
  lv_obj_set_style_text_color(title, primary, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 92);

  lv_obj_t *detail = lv_label_create(content);
  lv_label_set_text(detail, funtoria_safe_text(detail_text));
  lv_obj_set_width(detail, 198);
  lv_obj_set_style_text_color(detail, lv_color_hex(0x94A3B8), 0);
  lv_obj_set_style_text_opa(detail, LV_OPA_90, 0);
  lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
  lv_obj_align(detail, LV_ALIGN_TOP_MID, 0, 124);

  lv_obj_t *err_badge = lv_obj_create(ctx->root);
  lv_obj_set_size(err_badge, 172, 24);
  lv_obj_align(err_badge, LV_ALIGN_TOP_MID, 0, show_buttons ? 216 : 266);
  lv_obj_set_style_radius(err_badge, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(err_badge, primary, 0);
  lv_obj_set_style_bg_opa(err_badge, LV_OPA_10, 0);
  lv_obj_set_style_border_color(err_badge, primary, 0);
  lv_obj_set_style_border_opa(err_badge, LV_OPA_20, 0);
  lv_obj_set_style_border_width(err_badge, 1, 0);
  lv_obj_set_style_pad_all(err_badge, 0, 0);

  lv_obj_t *err_lbl = lv_label_create(err_badge);
  lv_label_set_text(err_lbl, funtoria_safe_text(badge_text));
  lv_obj_set_style_text_color(err_lbl, primary, 0);
  lv_obj_set_style_text_opa(err_lbl, LV_OPA_80, 0);
  lv_obj_center(err_lbl);

  if (show_buttons) {
    lv_obj_t *retry_btn = lv_btn_create(ctx->root);
    lv_obj_set_size(retry_btn, 208, 32);
    lv_obj_align(retry_btn, LV_ALIGN_BOTTOM_MID, 0, -46);
    lv_obj_set_style_radius(retry_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(retry_btn, primary, 0);
    lv_obj_set_style_bg_opa(retry_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(retry_btn, 0, 0);
    lv_obj_set_style_shadow_color(retry_btn, primary, 0);
    lv_obj_set_style_shadow_width(retry_btn, 16, 0);
    lv_obj_set_style_shadow_opa(retry_btn, LV_OPA_30, 0);

    lv_obj_t *retry_lbl = lv_label_create(retry_btn);
    lv_label_set_text(retry_lbl, funtoria_safe_text(primary_btn_text));
    lv_obj_set_style_text_color(retry_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(retry_lbl);

    lv_obj_t *menu_btn = lv_btn_create(ctx->root);
    lv_obj_set_size(menu_btn, 208, 26);
    lv_obj_align(menu_btn, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_radius(menu_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(menu_btn, bg_soft, 0);
    lv_obj_set_style_bg_opa(menu_btn, LV_OPA_30, 0);
    lv_obj_set_style_border_color(menu_btn, primary, 0);
    lv_obj_set_style_border_opa(menu_btn, LV_OPA_30, 0);
    lv_obj_set_style_border_width(menu_btn, 1, 0);

    lv_obj_t *menu_lbl = lv_label_create(menu_btn);
    lv_label_set_text(menu_lbl, funtoria_safe_text(secondary_btn_text));
    lv_obj_set_style_text_color(menu_lbl, primary, 0);
    lv_obj_set_style_text_opa(menu_lbl, LV_OPA_70, 0);
    lv_obj_center(menu_lbl);
  }
}

#ifdef __cplusplus
}
#endif

#endif // FUNTORIA_SCREEN_STYLE_H
