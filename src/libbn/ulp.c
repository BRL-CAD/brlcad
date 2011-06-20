/*                           U L P . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2011 United States Government as represented by
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
/** @file libbn/ulp.c
 *
 * Routines useful for performing comparisons and dynamically
 * calculating floating point limits including the Unit in the Last
 * Place (ULP).
 *
 * In this context, ULP is the distance to the next normalized
 * floating point value larger that a given input value.
 *
 * TODO: handle NaN, +-Inf, underflow, overflow, non-IEEE, float.h
 *
 * This file is completely in flux, incomplete, limited, and subject
 * to drastic changes.  Do NOT use it for anything.
 *
 * It also assumes an IEEE 754 compliant floating point
 * representation.
 */

#include "common.h"

#include <float.h>


double
bn_epsilon()
{
#if defined(DBL_EPSILON)
    return DBL_EPSILON;
#elif defined(HAVE_IEEE754)
    static const double val = 1.0;
    register long long next = *(long long*)&val + 1;
    return val - *(double *)&next;
#else
    /* must be volatile to avoid long registers */
    volatile double tol = 1.0;
    while (1.0 + (tol * 0.5) != 1.0) {
	tol *= 0.5;
    }
#endif
}


float
bn_epsilonf()
{
#if defined(FLT_EPSILON)
    return FLT_EPSILON;
#elif defined(HAVE_IEEE754)
    static const float val = 1.0;
    register long next = *(long*)&val + 1;
    return val - *(float *)&next;
#else
    /* must be volatile to avoid long registers */
    volatile float tol = 1.0f;
    while (1.0f + (tol * 0.5f) != 1.0f) {
	tol *= 0.5f;
    }
#endif
}


double
bn_dbl_min()
{
    register long long val = (1LL<<52);
    return *(double *)&val;
}


double
bn_dbl_max()
{
    static const double val = INFINITY;
    register long long next = *(long long*)&val - 1;
    return *(double *)&next;
}


double
bn_flt_min()
{
    register long val = (1LL<<23);
    return *(float *)&val;
}


double
bn_flt_max()
{
    static const float val = INFINITY;
    register long next = *(long*)&val - 1;
    return *(float *)&next;
}


double
bn_ulp(double val)
{
    double next;
    register long long up, dn, idx;

    if (isnan(val) || !isfinite(val))
	return val;

    if (val >=0) {
	up = *(long long*)&val + 1;
	return *(double *)&up - val;
    }
    dn = *(long long*)&val - 1;
    return *(double *)&dn - val;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
