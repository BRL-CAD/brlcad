#ifndef _RENDER_SPALL_H
#define _RENDER_SPALL_H

#include "render_internal.h"

typedef struct render_spall_s {
  TIE_3 ray_pos;
  TIE_3 ray_dir;
  tfloat plane[4];
  tfloat angle;
  tie_t tie;
} render_spall_t;

void render_spall_init(render_t *render, TIE_3 ray_pos, TIE_3 ray_dir, tfloat angle);
void render_spall_free(render_t *render);
void render_spall_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
