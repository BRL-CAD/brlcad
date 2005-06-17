#ifndef _RENDER_NORMAL_H
#define _RENDER_NORMAL_H

#include "render_internal.h"

void render_normal_init(render_t *render);
void render_normal_free(render_t *render);
void render_normal_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
