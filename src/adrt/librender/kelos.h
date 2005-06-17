#ifndef _RENDER_KELOS_H
#define _RENDER_KELOS_H

#include "render_internal.h"

void render_kelos_init(render_t *render);
void render_kelos_free(render_t *render);
void render_kelos_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
