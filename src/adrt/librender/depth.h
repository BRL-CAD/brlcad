#ifndef _RENDER_DEPTH_H
#define _RENDER_DEPTH_H

#include "render_internal.h"

void render_depth_init(render_t *render);
void render_depth_free(render_t *render);
void render_depth_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
