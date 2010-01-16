/*                    T I E _ D E F I N E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2010 United States Government as represented by
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
/** @file tie_define.h
 *
 * Brief description
 *
 */

#ifndef	_TIE_DEFINE_H
#define	_TIE_DEFINE_H

#include "common.h"

#include <math.h>
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#endif

#include "vmath.h"

#define TIE_SINGLE_PRECISION 0
#define TIE_DOUBLE_PRECISION 1

/*
 * define which precision to use, 0 is 'float' and 1 is 'double'.
 * Default to double precision to protect those not familiar.
 * Horrible macros wrap functions and values to build a library
 * capable of doing either without recompilation.
 */
#ifndef TIE_PRECISION
# define TIE_PRECISION TIE_SINGLE_PRECISION
#endif

#define	TIE_TAB1		"\1\0\0\2\2\1"	/* Triangle Index Table */
#define	TIE_KDTREE_NODE_MAX	4		/* Maximum number of triangles that can reside in a given node until it should be split */
#define	TIE_KDTREE_DEPTH_K1	1.4		/* K1 Depth Constant Coefficient */
#define	TIE_KDTREE_DEPTH_K2	1		/* K2 Contant */
#define TIE_CHECK_DEGENERATE	1

#define TIE_KDTREE_FAST		0x0
#define TIE_KDTREE_OPTIMAL	0x1

#define MAX_SLICES	100
#define MIN_SLICES	35
#define MIN_DENSITY	0.01
#define MIN_SPAN	0.15
#define SCALE_COEF	1.80

/* Type to use for floating precision */
#if TIE_PRECISION == TIE_SINGLE_PRECISION
# define tfloat float
# define TIE_FUNC(x, args...) x##0 ( args )
# define TIE_VAL(x) x##0
#elif TIE_PRECISION == TIE_DOUBLE_PRECISION
# define tfloat double
# define TIE_FUNC(x, args...) x##1 ( args )
# define TIE_VAL(x) x##1
#else
# error "Unknown precision"
#endif

/* TCOPY(type, source base, source offset, dest base, dest offset) */
#define TCOPY(_t, _fv, _fi, _tv, _ti) { \
	*(_t *)&((uint8_t *)_tv)[_ti] = *(_t *)&((uint8_t *)_fv)[_fi]; }

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
