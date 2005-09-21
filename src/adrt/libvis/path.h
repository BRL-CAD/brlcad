#ifndef _VIS_PATH_H
#define _VIS_PATH_H

#include "vis_intern.h"


typedef struct vis_path_s {
  int samples;
  tfloat inv_samples;
} vis_path_t;


void vis_path_init(vis_t *vis, int samples);
void vis_path_free(vis_t *vis);
void vis_path_work(vis_t *vis, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);


#endif
