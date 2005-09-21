#ifndef _VIS_UTIL_H
#define _VIS_UTIL_H

#include "vis_intern.h"

void vis_util_spall_vec(TIE_3 dir, tfloat angle, int vec_num, TIE_3 *vec_list);
void vis_util_shotline_list(tie_t *tie, tie_ray_t *ray, void **data, int *dlen);
void vis_util_spall_list(tie_t *tie, tie_ray_t *ray, tfloat angle, void **data, int *dlen);

#endif
