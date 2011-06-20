/*                   R E N D E R _ U T I L . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @file librender/render_util.h
 *
 */

#ifndef _RENDER_UTIL_H
#define _RENDER_UTIL_H

#include "render_internal.h"

BU_EXPORT extern void render_util_spall_vec(vect_t dir, fastf_t angle, int vec_num, vect_t *vec_list);
BU_EXPORT extern void render_util_shotline_list(struct tie_s *tie, struct tie_ray_s *ray, void **data, int *dlen);
BU_EXPORT extern void render_util_spall_list(struct tie_s *tie, struct tie_ray_s *ray, tfloat angle, void **data, int *dlen);

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
