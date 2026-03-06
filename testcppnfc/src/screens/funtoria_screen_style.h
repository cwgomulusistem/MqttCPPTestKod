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

static inline bool funtoria_finder_module_on(uint8_t x, uint8_t y, uint8_t ox,
                                             uint8_t oy) {
  if (x < ox || x > (ox + 6U) || y < oy || y > (oy + 6U)) {
    return false;
  }

  const uint8_t lx = x - ox;
  const uint8_t ly = y - oy;
  const bool outer_ring = (lx == 0U || lx == 6U || ly == 0U || ly == 6U);
  const bool inner_dot = (lx >= 2U && lx <= 4U && ly >= 2U && ly <= 4U);
  return outer_ring || inner_dot;
}

static inline bool funtoria_qr_module_on(uint8_t x, uint8_t y) {
  if (funtoria_finder_module_on(x, y, 0U, 0U) ||
      funtoria_finder_module_on(x, y, 14U, 0U) ||
      funtoria_finder_module_on(x, y, 0U, 14U)) {
    return true;
  }

  if ((x <= 7U && y <= 7U) || (x >= 13U && y <= 7U) ||
      (x <= 7U && y >= 13U)) {
    return false;
  }

  if ((x == 6U && y >= 8U && y <= 12U) || (y == 6U && x >= 8U && x <= 12U)) {
    return ((x + y) & 1U) == 0U;
  }

  const uint16_t seed = (uint16_t)(x * 31U + y * 17U + x * y * 7U);
  return (seed % 11U == 0U) || (((x + y) % 7U == 0U) && (seed % 3U == 0U));
}

static inline lv_obj_t *funtoria_create_fake_qr_scaled(lv_obj_t *parent,
                                                        uint8_t pixel_size,
                                                        uint8_t gap) {
  const uint8_t modules = 21U;
  const uint8_t cell = (uint8_t)(pixel_size + gap);
  const lv_coord_t grid_size = (lv_coord_t)(modules * cell - gap);

  lv_obj_t *grid = lv_obj_create(parent);
  lv_obj_remove_style_all(grid);
  lv_obj_set_size(grid, grid_size, grid_size);

  for (uint8_t y = 0U; y < modules; y++) {
    for (uint8_t x = 0U; x < modules; x++) {
      if (!funtoria_qr_module_on(x, y)) {
        continue;
      }

      lv_obj_t *px = lv_obj_create(grid);
      lv_obj_remove_style_all(px);
      lv_obj_set_size(px, pixel_size, pixel_size);
      lv_obj_set_pos(px, (lv_coord_t)(x * cell), (lv_coord_t)(y * cell));
      lv_obj_set_style_radius(px, 0, 0);
      lv_obj_set_style_bg_color(px, lv_color_hex(0x111111), 0);
      lv_obj_set_style_bg_opa(px, LV_OPA_COVER, 0);
    }
  }

  return grid;
}

static inline lv_obj_t *funtoria_create_fake_qr(lv_obj_t *parent) {
  return funtoria_create_fake_qr_scaled(parent, 4U, 1U);
}

static inline void funtoria_create_nav_item(lv_obj_t *parent, int16_t x_off,
                                            const char *icon, const char *text,
                                            bool active) {
  lv_obj_t *item = lv_obj_create(parent);
  lv_obj_remove_style_all(item);
  lv_obj_set_size(item, 68, 42);
  lv_obj_align(item, LV_ALIGN_CENTER, x_off, 0);

  lv_obj_t *icon_lbl = lv_label_create(item);
  lv_label_set_text(icon_lbl, funtoria_safe_text(icon));
  lv_obj_set_style_text_color(icon_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_opa(icon_lbl, active ? LV_OPA_100 : LV_OPA_60, 0);
  lv_obj_align(icon_lbl, LV_ALIGN_TOP_MID, 0, 2);

  lv_obj_t *txt_lbl = lv_label_create(item);
  lv_label_set_text(txt_lbl, funtoria_safe_text(text));
  lv_obj_set_style_text_color(txt_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_opa(txt_lbl, active ? LV_OPA_100 : LV_OPA_60, 0);
  lv_obj_align(txt_lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
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
  lv_obj_set_size(header, 220, 28);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 6);

  if (use_funtoria_header) {
    lv_obj_t *logo = lv_label_create(header);
    lv_label_set_text(logo, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(logo, primary, 0);
    lv_obj_align(logo, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *header_title = lv_label_create(header);
    lv_label_set_text(header_title, "Funtoria OS");
    lv_obj_set_style_text_color(header_title, primary, 0);
    lv_obj_set_style_text_opa(header_title, LV_OPA_90, 0);
    lv_obj_set_style_text_font(header_title, &lv_font_montserrat_16, 0);
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
    lv_obj_set_style_text_font(header_title, &lv_font_montserrat_16, 0);
    lv_obj_align(header_title, LV_ALIGN_CENTER, 0, 0);
  }

  lv_obj_t *content = lv_obj_create(ctx->root);
  lv_obj_remove_style_all(content);
  lv_obj_set_size(content, 224, 200);
  lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 30);

  lv_obj_t *outer = lv_obj_create(content);
  lv_obj_set_size(outer, 116, 116);
  lv_obj_align(outer, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_radius(outer, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(outer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(outer, primary, 0);
  lv_obj_set_style_border_opa(outer, LV_OPA_20, 0);
  lv_obj_set_style_border_width(outer, 1, 0);

  lv_obj_t *mid = lv_obj_create(content);
  lv_obj_set_size(mid, 98, 98);
  lv_obj_align(mid, LV_ALIGN_TOP_MID, 0, 9);
  lv_obj_set_style_radius(mid, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(mid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(mid, primary, 0);
  lv_obj_set_style_border_opa(mid, LV_OPA_40, 0);
  lv_obj_set_style_border_width(mid, 1, 0);

  lv_obj_t *icon_wrap = lv_obj_create(content);
  lv_obj_set_size(icon_wrap, 78, 78);
  lv_obj_align(icon_wrap, LV_ALIGN_TOP_MID, 0, 19);
  funtoria_apply_contrast_circle_style(icon_wrap, bg_dark, primary, 24, 40);
  lv_obj_set_style_shadow_color(icon_wrap, primary, 0);
  lv_obj_set_style_shadow_width(icon_wrap, 18, 0);
  lv_obj_set_style_shadow_opa(icon_wrap, LV_OPA_40, 0);

  lv_obj_t *icon_lbl = lv_label_create(icon_wrap);
  lv_label_set_text(icon_lbl, funtoria_safe_text(icon_symbol));
  lv_obj_set_style_text_color(icon_lbl, primary, 0);
  lv_obj_set_style_text_font(icon_lbl, &lv_font_montserrat_32, 0);
  lv_obj_center(icon_lbl);

  lv_obj_t *title = lv_label_create(content);
  lv_label_set_text(title, funtoria_safe_text(title_text));
  lv_obj_set_style_text_color(title, primary, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 124);

  lv_obj_t *detail = lv_label_create(content);
  lv_label_set_text(detail, funtoria_safe_text(detail_text));
  lv_obj_set_width(detail, 210);
  lv_obj_set_style_text_color(detail, lv_color_hex(0x94A3B8), 0);
  lv_obj_set_style_text_opa(detail, LV_OPA_90, 0);
  lv_obj_set_style_text_font(detail, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
  lv_obj_align(detail, LV_ALIGN_TOP_MID, 0, 160);

  lv_obj_t *err_badge = lv_obj_create(ctx->root);
  lv_obj_set_size(err_badge, 172, 24);
  lv_obj_align(err_badge, LV_ALIGN_TOP_MID, 0, show_buttons ? 206 : 268);
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

static inline void
funtoria_screen_apply_waiting_qr(const FuntoriaScreenContext *ctx) {
  if (!ctx || !ctx->root) {
    return;
  }

  funtoria_set_base_labels_hidden(ctx, true);
  funtoria_clear_dynamic_children(ctx);

  lv_obj_set_scrollbar_mode(ctx->root, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_radius(ctx->root, 0, 0);
  lv_obj_set_style_border_width(ctx->root, 0, 0);
  lv_obj_set_style_pad_all(ctx->root, 0, 0);
  lv_obj_set_style_bg_color(ctx->root, lv_color_hex(0x8B5CF6), 0);
  lv_obj_set_style_bg_grad_color(ctx->root, lv_color_hex(0x4C1D95), 0);
  lv_obj_set_style_bg_grad_dir(ctx->root, LV_GRAD_DIR_VER, 0);

  lv_obj_t *orb_cyan = lv_obj_create(ctx->root);
  lv_obj_remove_style_all(orb_cyan);
  lv_obj_set_size(orb_cyan, 120, 120);
  lv_obj_align(orb_cyan, LV_ALIGN_BOTTOM_LEFT, -55, 45);
  lv_obj_set_style_radius(orb_cyan, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(orb_cyan, lv_color_hex(0x06B6D4), 0);
  lv_obj_set_style_bg_opa(orb_cyan, LV_OPA_20, 0);

  lv_obj_t *orb_pink = lv_obj_create(ctx->root);
  lv_obj_remove_style_all(orb_pink);
  lv_obj_set_size(orb_pink, 120, 120);
  lv_obj_align(orb_pink, LV_ALIGN_TOP_RIGHT, 55, -45);
  lv_obj_set_style_radius(orb_pink, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(orb_pink, lv_color_hex(0xEC4899), 0);
  lv_obj_set_style_bg_opa(orb_pink, LV_OPA_20, 0);

  lv_obj_t *header = lv_obj_create(ctx->root);
  lv_obj_remove_style_all(header);
  lv_obj_set_size(header, 220, 22);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t *left_icon = lv_label_create(header);
  lv_label_set_text(left_icon, LV_SYMBOL_PLAY);
  lv_obj_set_style_text_color(left_icon, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(left_icon, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *left_title = lv_label_create(header);
  lv_label_set_text(left_title, "ARCADE OS");
  lv_obj_set_style_text_color(left_title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_opa(left_title, LV_OPA_90, 0);
  lv_obj_align(left_title, LV_ALIGN_LEFT_MID, 18, 0);

  lv_obj_t *wifi_icon = lv_label_create(header);
  lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(wifi_icon, LV_ALIGN_RIGHT_MID, -18, 0);

  lv_obj_t *bat_icon = lv_label_create(header);
  lv_label_set_text(bat_icon, LV_SYMBOL_BATTERY_FULL);
  lv_obj_set_style_text_color(bat_icon, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(bat_icon, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t *title = lv_label_create(ctx->root);
  lv_label_set_text(title, "OYNAMAK ICIN TARA");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 42);

  lv_obj_t *subtitle = lv_label_create(ctx->root);
  lv_label_set_text(subtitle, "Scan to Play");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_opa(subtitle, LV_OPA_70, 0);
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 64);

  lv_obj_t *qr_card = lv_obj_create(ctx->root);
  lv_obj_set_size(qr_card, 164, 164);
  lv_obj_align(qr_card, LV_ALIGN_TOP_MID, 0, 82);
  lv_obj_set_style_bg_color(qr_card, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(qr_card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(qr_card, 14, 0);
  lv_obj_set_style_border_width(qr_card, 0, 0);
  lv_obj_set_style_pad_all(qr_card, 10, 0);
  lv_obj_set_style_shadow_color(qr_card, lv_color_hex(0x06B6D4), 0);
  lv_obj_set_style_shadow_width(qr_card, 24, 0);
  lv_obj_set_style_shadow_opa(qr_card, LV_OPA_30, 0);

  lv_obj_t *qr_zone = lv_obj_create(qr_card);
  lv_obj_set_size(qr_zone, 142, 142);
  lv_obj_center(qr_zone);
  lv_obj_set_style_bg_color(qr_zone, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(qr_zone, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(qr_zone, 10, 0);
  lv_obj_set_style_border_color(qr_zone, lv_color_hex(0xDDD6FE), 0);
  lv_obj_set_style_border_width(qr_zone, 2, 0);
  lv_obj_set_style_pad_all(qr_zone, 0, 0);

  lv_obj_t *qr = funtoria_create_fake_qr(qr_zone);
  lv_obj_center(qr);

  lv_obj_t *balance = lv_obj_create(ctx->root);
  lv_obj_set_size(balance, 140, 26);
  lv_obj_align(balance, LV_ALIGN_TOP_MID, 0, 252);
  lv_obj_set_style_radius(balance, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(balance, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(balance, LV_OPA_30, 0);
  lv_obj_set_style_border_color(balance, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_opa(balance, LV_OPA_40, 0);
  lv_obj_set_style_border_width(balance, 1, 0);
  lv_obj_set_style_pad_all(balance, 0, 0);

  lv_obj_t *balance_lbl = lv_label_create(balance);
  lv_label_set_text(balance_lbl, LV_SYMBOL_CHARGE " BAKIYE: TL45.00");
  lv_obj_set_style_text_color(balance_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_center(balance_lbl);

  lv_obj_t *footer = lv_obj_create(ctx->root);
  lv_obj_set_size(footer, 220, 46);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_radius(footer, 12, 0);
  lv_obj_set_style_bg_color(footer, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(footer, LV_OPA_20, 0);
  lv_obj_set_style_border_color(footer, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_opa(footer, LV_OPA_40, 0);
  lv_obj_set_style_border_width(footer, 1, 0);
  lv_obj_set_style_pad_all(footer, 0, 0);

  funtoria_create_nav_item(footer, -72, LV_SYMBOL_WARNING, "BILGI", false);
  funtoria_create_nav_item(footer, 0, LV_SYMBOL_IMAGE, "TARA", true);
  funtoria_create_nav_item(footer, 72, LV_SYMBOL_HOME, "PROFIL", false);
}

#ifdef __cplusplus
}
#endif

#endif // FUNTORIA_SCREEN_STYLE_H
