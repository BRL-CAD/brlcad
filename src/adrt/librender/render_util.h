#ifndef _RENDER_UTIL_H
#define _RENDER_UTIL_H

#include "render_internal.h"

void render_util_spall_vec(TIE_3 dir, tfloat angle, int vec_num, TIE_3 *vec_list);
void render_util_shotline_list(tie_t *tie, tie_ray_t *ray, void **data, int *dlen);
void render_util_spall_list(tie_t *tie, tie_ray_t *ray, tfloat angle, void **data, int *dlen);

#endif
