#ifndef FUNTORIA_SCREEN_CONTEXT_H
#define FUNTORIA_SCREEN_CONTEXT_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  lv_obj_t *root;
  lv_obj_t *title;
  lv_obj_t *body;
  lv_obj_t *footer;
} FuntoriaScreenContext;

#ifdef __cplusplus
}
#endif

#endif // FUNTORIA_SCREEN_CONTEXT_H
