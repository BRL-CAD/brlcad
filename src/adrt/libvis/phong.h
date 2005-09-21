#ifndef _VIS_PHONG_H
#define _VIS_PHONG_H

#include "vis_intern.h"

void vis_phong_init(vis_t *vis);
void vis_phong_free(vis_t *vis);
void vis_phong_work(vis_t *vis, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
