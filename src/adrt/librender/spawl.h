#ifndef _RENDER_SPAWL_H
#define _RENDER_SPAWL_H

#include "render_internal.h"

typedef struct render_spawl_s {
  TIE_3 ray_pos;
  TIE_3 ray_dir;
  tfloat plane[4];
  tfloat angle;
  tie_t tie;
} render_spawl_t;

void render_spawl_init(render_t *render, TIE_3 ray_pos, TIE_3 ray_dir, tfloat angle);
void render_spawl_free(render_t *render);
void render_spawl_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
