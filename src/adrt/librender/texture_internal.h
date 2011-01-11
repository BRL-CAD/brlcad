/*                     T E X T U R E _ I N T E R N A L . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2010 United States Government as represented by
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
/** @file texture_internal.h
 *
 *  Comments -
 *      Texture Library - Internal texture include
 *
 */

#ifndef _TEXTURE_INTERNAL_H
#define _TEXTURE_INTERNAL_H


#include "tie.h"

#define __TEXTURE_WORK_PROTOTYPE__ texture_t *texture, void *mesh, struct tie_ray_s *ray, struct tie_id_s *id, TIE_3 *pixel

struct texture_s;
struct mesh_s;
typedef void texture_init_t(struct texture_s *texture);
typedef void texture_free_t(struct texture_s *texture);
typedef void texture_work_t(struct texture_s *texture, void *mesh, struct tie_ray_s *ray, struct tie_id_s *id, TIE_3 *pixel);


typedef struct texture_s {
    texture_free_t *free;
    texture_work_t *work;
    void *data;
} texture_t;

/* _a is transformed vertex, _b is input vertex, _c is 4x4 transformation matrix */
#define MATH_VEC_TRANSFORM(_a, _b, _c) { \
	tfloat  w; \
	_a.v[0] = (_b.v[0] * _c[0]) + (_b.v[1] * _c[4]) + (_b.v[2] * _c[8]) + _c[12]; \
	_a.v[1] = (_b.v[0] * _c[1]) + (_b.v[1] * _c[5]) + (_b.v[2] * _c[9]) + _c[13]; \
	_a.v[2] = (_b.v[0] * _c[2]) + (_b.v[1] * _c[6]) + (_b.v[2] * _c[10]) + _c[14]; \
	w = (_b.v[0] * _c[3]) + (_b.v[1] * _c[7]) + (_b.v[2] * _c[11]) + _c[15]; \
	w = w ? 1/w : 1.0; \
	_a.v[0] *= w; _a.v[1] *= w; _a.v[2] *= w; }

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
