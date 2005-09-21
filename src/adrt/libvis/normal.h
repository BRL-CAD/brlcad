#ifndef _VIS_NORMAL_H
#define _VIS_NORMAL_H

#include "vis_intern.h"

void vis_normal_init(vis_t *vis);
void vis_normal_free(vis_t *vis);
void vis_normal_work(vis_t *vis, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
