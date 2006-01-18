/*                     U M A T H . H
 * BRL-CAD
 *
 * Copyright (c) 2002-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file umath.h
 *                     U M A T H . H
 *
 *  Utilities Library - Extended Math Header
 *
 *  Author -
 *      Justin L. Shumaker
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * $Id$
 */

#ifndef _COMMON_MATH_H
#define _COMMON_MATH_H


#include <math.h>
#include "tie.h"
#include "brlcad_config.h"
#include "rand.h"


#define	math_pi		3.14159265f
#define math_2_pi	6.28318530f
#define	math_rad2deg	57.2957795f
#define	math_deg2rad	0.0174532925f
#define	math_1div180	0.0055555556f

#define math_min2(_a, _b, _c) { \
	_a = _b < _c ? _b : _c; }

#define math_max2(_a, _b, _c) { \
	_a = _b > _c ? _b : _c; }

#define	math_min3(_a, _b, _c, _d) { \
	_a = _b < _c ? _b < _d ? _b : _d : _c < _d ? _c : _d; }

#define math_max3(_a, _b, _c, _d) { \
	_a = _b > _c ? _b > _d ? _b : _d : _c > _d ? _c : _d; }

#define math_min_max3(_a, _b, _c, _d, _e) { \
	math_min3(_a, _c, _d, _e); \
        math_max3(_b, _c, _d, _e); }

#define math_vec_min(_a, _b) { \
	_a.v[0] = _a.v[0] < _b.v[0] ? _a.v[0] : _b.v[0]; \
	_a.v[1] = _a.v[1] < _b.v[1] ? _a.v[1] : _b.v[1]; \
	_a.v[2] = _a.v[2] < _b.v[2] ? _a.v[2] : _b.v[2]; }

#define math_vec_max(_a, _b) { \
	_a.v[0] = _a.v[0] > _b.v[0] ? _a.v[0] : _b.v[0]; \
	_a.v[1] = _a.v[1] > _b.v[1] ? _a.v[1] : _b.v[1]; \
	_a.v[2] = _a.v[2] > _b.v[2] ? _a.v[2] : _b.v[2]; }

#define math_vec_mag(_a, _b) { \
	_a = sqrt(_b.v[0]*_b.v[0] + _b.v[1]*_b.v[1] + _b.v[2]*_b.v[2]); }

#define math_swap(_a,_b) { \
	tfloat	_c; \
	_c = _b; \
	_b = _a; \
	_a = _c; }


/* Vector Functions */

#define math_vec_mul_scalar(_a, _b, _c) { \
	_a.v[0] = _b.v[0] * _c; \
	_a.v[1] = _b.v[1] * _c; \
	_a.v[2] = _b.v[2] * _c; }

/* _a is transformed vertex, _b is input vertex, _c is 4x4 transformation matrix */
#define math_vec_transform(_a, _b, _c) { \
	tfloat	w; \
	_a.v[0] = (_b.v[0] * _c[0]) + (_b.v[1] * _c[4]) + (_b.v[2] * _c[8]) + _c[12]; \
	_a.v[1] = (_b.v[0] * _c[1]) + (_b.v[1] * _c[5]) + (_b.v[2] * _c[9]) + _c[13]; \
	_a.v[2] = (_b.v[0] * _c[2]) + (_b.v[1] * _c[6]) + (_b.v[2] * _c[10]) + _c[14]; \
	w = (_b.v[0] * _c[3]) + (_b.v[1] * _c[7]) + (_b.v[2] * _c[11]) + _c[15]; \
        w = w ? 1/w : 1.0; \
        _a.v[0] *= w; _a.v[1] *= w; _a.v[2] *= w; }

/* _a is transformed vertex, _b is input vertex, _c is 4x4 transformation matrix */
#define math_vec_transform_rotate(_a, _b, _c) { \
	_a.v[0] = (_b.v[0] * _c[0]) + (_b.v[1] * _c[4]) + (_b.v[2] * _c[8]); \
	_a.v[1] = (_b.v[0] * _c[1]) + (_b.v[1] * _c[5]) + (_b.v[2] * _c[9]); \
	_a.v[2] = (_b.v[0] * _c[2]) + (_b.v[1] * _c[6]) + (_b.v[2] * _c[10]); }

/* _a is reflected ray, _b is incident ray, _c is normal */
#define	math_vec_reflect(_a, _b, _c) { \
	tfloat _d; \
	math_vec_dot(_d, _b, _c); \
	math_vec_mul_scalar(_a, _c, 2.0*_d); \
	math_vec_sub(_a, _b, _a); \
	math_vec_unitize(_a); }


extern	void	math_mat_ident(tfloat *M, int S);						/* Identity Matrix */
extern	void	math_mat_mult(tfloat *A, int Ar, int Ac, tfloat *B, int Br, int Bc, tfloat *C);	/* Multiply 2 Matrices */
extern	void	math_mat_invert(tfloat *D, tfloat *M, int S);					/* Invert */

#endif
