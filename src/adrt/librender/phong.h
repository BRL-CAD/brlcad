#ifndef _RENDER_PHONG_H
#define _RENDER_PHONG_H

#include "render_internal.h"

void render_phong_init(render_t *render);
void render_phong_free(render_t *render);
void render_phong_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
