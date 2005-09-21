#ifndef _RENDER_FLAT_H
#define _RENDER_FLAT_H

#include "render_internal.h"

void render_flat_init(render_t *render);
void render_flat_free(render_t *render);
void render_flat_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
