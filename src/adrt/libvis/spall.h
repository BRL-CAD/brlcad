#ifndef _VIS_SPALL_H
#define _VIS_SPALL_H

#include "vis_intern.h"

typedef struct vis_spall_s {
  TIE_3 ray_pos;
  TIE_3 ray_dir;
  tfloat plane[4];
  tfloat angle;
  tie_t tie;
} vis_spall_t;

void vis_spall_init(vis_t *vis, TIE_3 ray_pos, TIE_3 ray_dir, tfloat angle);
void vis_spall_free(vis_t *vis);
void vis_spall_work(vis_t *vis, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
