#ifndef _RENDER_GRID_H
#define _RENDER_GRID_H

#include "render_internal.h"

void render_grid_init(render_t *render);
void render_grid_free(render_t *render);
void render_grid_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
