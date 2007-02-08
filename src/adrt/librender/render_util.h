/*                   R E N D E R _ U T I L . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file render_util.h
 *
 *  Author -
 *      Justin L. Shumaker
 *
 */

#ifndef _RENDER_UTIL_H
#define _RENDER_UTIL_H

#include "render_internal.h"

void render_util_spall_vec(TIE_3 dir, TFLOAT angle, int vec_num, TIE_3 *vec_list);
void render_util_shotline_list(tie_t *tie, tie_ray_t *ray, void **data, int *dlen);
void render_util_spall_list(tie_t *tie, tie_ray_t *ray, TFLOAT angle, void **data, int *dlen);

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
