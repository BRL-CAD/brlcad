#ifndef _VIS_GRID_H
#define _VIS_GRID_H

#include "vis_intern.h"

void vis_grid_init(vis_t *vis);
void vis_grid_free(vis_t *vis);
void vis_grid_work(vis_t *vis, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
