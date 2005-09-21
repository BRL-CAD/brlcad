#ifndef _VIS_DEPTH_H
#define _VIS_DEPTH_H

#include "vis_intern.h"

void vis_depth_init(vis_t *vis);
void vis_depth_free(vis_t *vis);
void vis_depth_work(vis_t *vis, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
