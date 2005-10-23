/*                     D E F I N E . H
 *
 * @file define.h
 *
 * BRL-CAD
 *
 * Copyright (C) 2002-2005 United States Government as represented by
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
 *
 *                     D E F I N E. H
 *
 *  Comments -
 *      Triangle Intersection Engine Defines
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
/** @addtogroup libtie */ /** @{ */

#ifndef	_TIE_DEFINE_H
#define	_TIE_DEFINE_H

#include <math.h>
#include "brlcad_config.h"

#define TIE_SINGLE_PREC		1		/* Use Single Precision Math */
#define	TIE_TAB1		"\1\0\0\2\2\1"	/* Triangle Index Table */
#define	TIE_KDTREE_NODE_MAX	4		/* Maximum number of triangles that can reside in a given node until it should be split */
#define	TIE_KDTREE_DEPTH_K1	1.6		/* K1 Depth Constant Coefficient */
#define	TIE_KDTREE_DEPTH_K2	1		/* K2 Contant */

#define MAX_SLICES 100
#define MIN_SLICES 35
#define MIN_DENSITY 0.01
#define MIN_SPAN    0.15
#define SCALE_COEF  1.80


/* Type to use for floating precision */
#if TIE_SINGLE_PREC
#define tfloat float
#else
#define tfloat double
#endif


/**
 *
 */
#define	math_min3(_a, _b, _c, _d) { \
	_a = _b < _c ? _b < _d ? _b : _d : _c < _d ? _c : _d; }

/**
 *
 */
#define math_max3(_a, _b, _c, _d) { \
	_a = _b > _c ? _b > _d ? _b : _d : _c > _d ? _c : _d; }

/**
 *
 */
#define	math_vec_set(_a, _b, _c, _d) { \
	_a.v[0] = _b; \
	_a.v[1] = _c; \
	_a.v[2] = _d; }

/**
 *
 */
#define math_vec_min(_a, _b) { \
	_a.v[0] = _a.v[0] < _b.v[0] ? _a.v[0] : _b.v[0]; \
	_a.v[1] = _a.v[1] < _b.v[1] ? _a.v[1] : _b.v[1]; \
	_a.v[2] = _a.v[2] < _b.v[2] ? _a.v[2] : _b.v[2]; }

/**
 *
 */
#define math_vec_max(_a, _b) { \
	_a.v[0] = _a.v[0] > _b.v[0] ? _a.v[0] : _b.v[0]; \
	_a.v[1] = _a.v[1] > _b.v[1] ? _a.v[1] : _b.v[1]; \
	_a.v[2] = _a.v[2] > _b.v[2] ? _a.v[2] : _b.v[2]; }

/**
 *
 */
#define math_vec_cross(_a, _b, _c) {\
	_a.v[0] = _b.v[1]*_c.v[2] - _b.v[2]*_c.v[1]; \
	_a.v[1] = _b.v[2]*_c.v[0] - _b.v[0]*_c.v[2]; \
	_a.v[2] = _b.v[0]*_c.v[1] - _b.v[1]*_c.v[0]; }

/**
 *
 */
#define	math_vec_unitize(_a) { \
	tfloat _b = 1/sqrt(_a.v[0]*_a.v[0] + _a.v[1]*_a.v[1] + _a.v[2]*_a.v[2]); \
	_a.v[0] *= _b; _a.v[1] *= _b; _a.v[2] *= _b; }

/**
 *
 */
#define math_vec_sq(_a, _b) { \
	_a.v[0] = _b.v[0] * _b.v[0]; \
	_a.v[1] = _b.v[1] * _b.v[1]; \
	_a.v[2] = _b.v[2] * _b.v[2]; }

/**
 *
 */
#define math_vec_dot(_a, _b, _c) { \
	_a = _b.v[0]*_c.v[0] + _b.v[1]*_c.v[1] + _b.v[2]*_c.v[2]; }

/**
 *
 */
#define	math_vec_add(_a, _b, _c) { \
	_a.v[0] = _b.v[0] + _c.v[0]; \
	_a.v[1] = _b.v[1] + _c.v[1]; \
	_a.v[2] = _b.v[2] + _c.v[2]; }

/**
 *
 */
#define math_vec_sub(_a, _b, _c) { \
	_a.v[0] = _b.v[0] - _c.v[0]; \
	_a.v[1] = _b.v[1] - _c.v[1]; \
	_a.v[2] = _b.v[2] - _c.v[2]; }

/**
 *
 */
#define	math_vec_mul(_a, _b, _c) { \
	_a.v[0] = _b.v[0] * _c.v[0]; \
	_a.v[1] = _b.v[1] * _c.v[1]; \
	_a.v[2] = _b.v[2] * _c.v[2]; }

/**
 *
 */
#define math_vec_mul_scalar(_a, _b, _c) { \
	_a.v[0] = _b.v[0] * _c; \
	_a.v[1] = _b.v[1] * _c; \
	_a.v[2] = _b.v[2] * _c; }

/**
 *
 */
#define	math_vec_div(_a, _b, _c) { \
	_a.v[0] = _b.v[0] / _c.v[0]; \
	_a.v[1] = _b.v[1] / _c.v[1]; \
	_a.v[2] = _b.v[2] / _c.v[2]; }

/**
 *
 */
#define math_bbox(_a, _b, _c, _d, _e) { \
	math_min3(_a.v[0], _c.v[0], _d.v[0], _e.v[0]); \
	math_min3(_a.v[1], _c.v[1], _d.v[1], _e.v[1]); \
	math_min3(_a.v[2], _c.v[2], _d.v[2], _e.v[2]); \
	math_max3(_b.v[0], _c.v[0], _d.v[0], _e.v[0]); \
	math_max3(_b.v[1], _c.v[1], _d.v[1], _e.v[1]); \
	math_max3(_b.v[2], _c.v[2], _d.v[2], _e.v[2]); }

/* ======================== X-tests ======================== */
/**
 *
 */
#define AXISTEST_X01(a, b, fa, fb) \
	p.v[0] = a*v0.v[1] - b*v0.v[2]; \
	p.v[2] = a*v2.v[1] - b*v2.v[2]; \
        if (p.v[0] < p.v[2]) { min = p.v[0]; max = p.v[2]; } else { min = p.v[2]; max = p.v[0]; } \
	rad = fa * half_size -> v[1] + fb * half_size -> v[2]; \
	if (min > rad || max < -rad) return 0; \

/**
 *
 */
#define AXISTEST_X2(a, b, fa, fb) \
	p.v[0] = a*v0.v[1] - b*v0.v[2]; \
	p.v[1] = a*v1.v[1] - b*v1.v[2]; \
        if (p.v[0] < p.v[1]) { min = p.v[0]; max = p.v[1]; } else { min = p.v[1]; max = p.v[0]; } \
	rad = fa * half_size -> v[1] + fb * half_size -> v[2]; \
	if (min > rad || max < -rad) return 0;

/* ======================== Y-tests ======================== */
/**
 *
 */
#define AXISTEST_Y02(a, b, fa, fb) \
	p.v[0] = -a*v0.v[0] + b*v0.v[2]; \
	p.v[2] = -a*v2.v[0] + b*v2.v[2]; \
        if (p.v[0] < p.v[2]) { min = p.v[0]; max = p.v[2]; } else { min = p.v[2]; max = p.v[0]; } \
	rad = fa * half_size -> v[0] + fb * half_size -> v[2]; \
	if (min > rad || max < -rad) return 0;

/**
 *
 */
#define AXISTEST_Y1(a, b, fa, fb) \
	p.v[0] = -a*v0.v[0] + b*v0.v[2]; \
	p.v[1] = -a*v1.v[0] + b*v1.v[2]; \
        if (p.v[0] < p.v[1]) { min = p.v[0]; max = p.v[1]; } else { min = p.v[1]; max = p.v[0]; } \
	rad = fa * half_size -> v[0] + fb * half_size -> v[2]; \
	if (min > rad || max < -rad) return 0;

/* ======================== Z-tests ======================== */
/**
 *
 */
#define AXISTEST_Z12(a, b, fa, fb) \
	p.v[1] = a*v1.v[0] - b*v1.v[1]; \
	p.v[2] = a*v2.v[0] - b*v2.v[1]; \
        if (p.v[2] < p.v[1]) { min = p.v[2]; max = p.v[1]; } else { min = p.v[1]; max = p.v[2]; } \
	rad = fa * half_size -> v[0] + fb * half_size -> v[1]; \
	if (min > rad || max < -rad) return 0;

/**
 *
 */
#define AXISTEST_Z0(a, b, fa, fb) \
	p.v[0] = a*v0.v[0] - b*v0.v[1]; \
	p.v[1] = a*v1.v[0] - b*v1.v[1]; \
        if (p.v[0] < p.v[1]) { min = p.v[0]; max = p.v[1]; } else { min = p.v[1]; max = p.v[0]; } \
	rad = fa * half_size -> v[0] + fb * half_size -> v[1]; \
	if (min > rad || max < -rad) return 0;

#endif

/** @} */
