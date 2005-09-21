#ifndef _RENDER_PLANE_H
#define _RENDER_PLANE_H

#include "render_internal.h"

typedef struct render_plane_s {
  TIE_3 ray_pos;
  TIE_3 ray_dir;
  tfloat plane[4];
  tie_t tie;
} render_plane_t;

void render_plane_init(render_t *render, TIE_3 ray_pos, TIE_3 ray_dir);
void render_plane_free(render_t *render);
void render_plane_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
