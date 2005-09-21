#ifndef _VIS_FLAT_H
#define _VIS_FLAT_H

#include "vis_intern.h"

void vis_flat_init(vis_t *vis);
void vis_flat_free(vis_t *vis);
void vis_flat_work(vis_t *vis, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
