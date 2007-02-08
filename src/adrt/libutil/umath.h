/*                     U M A T H . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2007 United States Government as represented by
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
/** @file umath.h
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


#define	MATH_PI		3.14159265f
#define MATH_2_PI	6.28318530f
#define	MATH_RAD2DEG	57.2957795f
#define	MATH_DEG2RAD	0.0174532925f
#define	MATH_1DIV180	0.0055555556f

#define MATH_MIN2(_a, _b, _c) { \
	_a = _b < _c ? _b : _c; }

#define MATH_MAX2(_a, _b, _c) { \
	_a = _b > _c ? _b : _c; }

#define	MATH_MIN3(_a, _b, _c, _d) { \
	_a = _b < _c ? _b < _d ? _b : _d : _c < _d ? _c : _d; }

#define MATH_MAX3(_a, _b, _c, _d) { \
	_a = _b > _c ? _b > _d ? _b : _d : _c > _d ? _c : _d; }

#define MATH_MIN_MAX3(_a, _b, _c, _d, _e) { \
	MATH_MIN3(_a, _c, _d, _e); \
	MATH_MAX3(_b, _c, _d, _e); }

#define MATH_VEC_MIN(_a, _b) { \
	_a.v[0] = _a.v[0] < _b.v[0] ? _a.v[0] : _b.v[0]; \
	_a.v[1] = _a.v[1] < _b.v[1] ? _a.v[1] : _b.v[1]; \
	_a.v[2] = _a.v[2] < _b.v[2] ? _a.v[2] : _b.v[2]; }

#define MATH_VEC_MAX(_a, _b) { \
	_a.v[0] = _a.v[0] > _b.v[0] ? _a.v[0] : _b.v[0]; \
	_a.v[1] = _a.v[1] > _b.v[1] ? _a.v[1] : _b.v[1]; \
	_a.v[2] = _a.v[2] > _b.v[2] ? _a.v[2] : _b.v[2]; }

#define MATH_VEC_MAG(_a, _b) { \
	_a = sqrt(_b.v[0]*_b.v[0] + _b.v[1]*_b.v[1] + _b.v[2]*_b.v[2]); }

#define MATH_SWAP(_a,_b) { \
	TFLOAT	_c; \
	_c = _b; \
	_b = _a; \
	_a = _c; }


/* Vector Functions */

#define MATH_VEC_MUL_SCALAR(_a, _b, _c) { \
	_a.v[0] = _b.v[0] * _c; \
	_a.v[1] = _b.v[1] * _c; \
	_a.v[2] = _b.v[2] * _c; }

/* _a is transformed vertex, _b is input vertex, _c is 4x4 transformation matrix */
#define MATH_VEC_TRANSFORM(_a, _b, _c) { \
	TFLOAT	w; \
	_a.v[0] = (_b.v[0] * _c[0]) + (_b.v[1] * _c[4]) + (_b.v[2] * _c[8]) + _c[12]; \
	_a.v[1] = (_b.v[0] * _c[1]) + (_b.v[1] * _c[5]) + (_b.v[2] * _c[9]) + _c[13]; \
	_a.v[2] = (_b.v[0] * _c[2]) + (_b.v[1] * _c[6]) + (_b.v[2] * _c[10]) + _c[14]; \
	w = (_b.v[0] * _c[3]) + (_b.v[1] * _c[7]) + (_b.v[2] * _c[11]) + _c[15]; \
	w = w ? 1/w : 1.0; \
	_a.v[0] *= w; _a.v[1] *= w; _a.v[2] *= w; }

/* _a is transformed vertex, _b is input vertex, _c is 4x4 transformation matrix */
#define MATH_VEC_TRANSFORM_ROTATE(_a, _b, _c) { \
	_a.v[0] = (_b.v[0] * _c[0]) + (_b.v[1] * _c[4]) + (_b.v[2] * _c[8]); \
	_a.v[1] = (_b.v[0] * _c[1]) + (_b.v[1] * _c[5]) + (_b.v[2] * _c[9]); \
	_a.v[2] = (_b.v[0] * _c[2]) + (_b.v[1] * _c[6]) + (_b.v[2] * _c[10]); }

/* _a is reflected ray, _b is incident ray, _c is normal */
#define	MATH_VEC_REFLECT(_a, _b, _c) { \
	TFLOAT _d; \
	MATH_VEC_DOT(_d, _b, _c); \
	MATH_VEC_MUL_SCALAR(_a, _c, 2.0*_d); \
	MATH_VEC_SUB(_a, _b, _a); \
	MATH_VEC_UNITIZE(_a); }


extern	void	math_mat_ident(TFLOAT *M, int S);						/* Identity Matrix */
extern	void	math_mat_mult(TFLOAT *A, int Ar, int Ac, TFLOAT *B, int Br, int Bc, TFLOAT *C);	/* Multiply 2 Matrices */
extern	void	math_mat_invert(TFLOAT *D, TFLOAT *M, int S);					/* Invert */

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
