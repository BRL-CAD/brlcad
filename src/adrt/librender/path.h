#ifndef _RENDER_PATH_H
#define _RENDER_PATH_H

#include "render_internal.h"


typedef struct render_path_s {
  int samples;
  tfloat inv_samples;
} render_path_t;


void render_path_init(render_t *render, int samples);
void render_path_free(render_t *render);
void render_path_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);


#endif
