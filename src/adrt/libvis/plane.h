#ifndef _VIS_PLANE_H
#define _VIS_PLANE_H

#include "vis_intern.h"

typedef struct vis_plane_s {
  TIE_3 ray_pos;
  TIE_3 ray_dir;
  tfloat plane[4];
  tie_t tie;
} vis_plane_t;

void vis_plane_init(vis_t *vis, TIE_3 ray_pos, TIE_3 ray_dir);
void vis_plane_free(vis_t *vis);
void vis_plane_work(vis_t *vis, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
