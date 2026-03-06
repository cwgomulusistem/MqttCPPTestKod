#ifndef LV_CONF_H
#define LV_CONF_H

/* Match embedded panel format for visual parity. */
#define LV_COLOR_DEPTH 16

/* Simulator runs in a single thread loop. */
#define LV_USE_OS LV_OS_NONE

/* Use LVGL's builtin allocator on desktop. */
#define LV_MEM_CUSTOM 0

/* Fonts used by Funtoria screens. */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* SDL desktop driver. */
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_PARTIAL
#define LV_SDL_BUF_COUNT 1
#define LV_SDL_ACCELERATED 1
#define LV_SDL_FULLSCREEN 0
#define LV_SDL_DIRECT_EXIT 1
#define LV_SDL_MOUSEWHEEL_MODE LV_SDL_MOUSEWHEEL_MODE_ENCODER

#endif /* LV_CONF_H */

