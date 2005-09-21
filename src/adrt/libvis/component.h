#ifndef _VIS_COMPONENT_H
#define _VIS_COMPONENT_H

#include "vis_intern.h"

void vis_component_init(vis_t *vis);
void vis_component_free(vis_t *vis);
void vis_component_work(vis_t *vis, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
