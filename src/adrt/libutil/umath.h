/*                     U M A T H . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2009 United States Government as represented by
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
 */

#ifndef _COMMON_MATH_H
#define _COMMON_MATH_H

#include "common.h"

#include <math.h>
#include "tie.h"
#include "rand.h"

/* Vector Functions */

/* _a is reflected ray, _b is incident ray, _c is normal */
#define	MATH_VEC_REFLECT(_a, _b, _c) { \
	tfloat _d; \
	MATH_VEC_DOT(_d, _b, _c); \
	MATH_VEC_MUL_SCALAR(_a, _c, 2.0*_d); \
	MATH_VEC_SUB(_a, _b, _a); \
	MATH_VEC_UNITIZE(_a); }

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
