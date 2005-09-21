#ifndef _RENDER_COMPONENT_H
#define _RENDER_COMPONENT_H

#include "render_internal.h"

void render_component_init(render_t *render);
void render_component_free(render_t *render);
void render_component_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

#endif
