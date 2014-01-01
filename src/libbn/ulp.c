/*                           U L P . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2014 United States Government as represented by
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
 * NOTE: This is a work-in-progress and not yet published API to be
 * used anywhere.  DO NOT USE.
 *
 * Routines useful for performing comparisons and dynamically
 * calculating floating point limits including the Unit in the Last
 * Place (ULP).
 *
 * In this context, ULP is the distance to the next normalized
 * floating point value larger than a given input value.
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

#include <math.h>
#include <limits.h>
#ifdef HAVE_FLOAT_H
#  include <float.h>
#endif

/* #define HAVE_IEEE754 1 */

double
bn_epsilon()
{
#if defined(DBL_EPSILON)
    return DBL_EPSILON;
#elif defined(HAVE_IEEE754)
    static const double val = 1.0;
    long long next = *(long long*)&val + 1;
    return val - *(double *)&next;
#else
    /* must be volatile to avoid long registers */
    volatile double tol = 1.0;
    while (1.0 + (tol * 0.5) > 1.0) {
	tol *= 0.5;
    }
    return tol;
#endif
}


float
bn_epsilonf()
{
#if defined(FLT_EPSILON)
    return FLT_EPSILON;
#elif defined(HAVE_IEEE754)
    static const float val = 1.0;
    long next = *(long*)&val + 1;
    return val - *(float *)&next;
#else
    /* must be volatile to avoid long registers */
    volatile float tol = 1.0f;
    while (1.0f + (tol * 0.5f) > 1.0f) {
	tol *= 0.5f;
    }
    return tol;
#endif
}


double
bn_dbl_min()
{
    long long val = (1LL<<52);
    return *(double *)&val;
}


double
bn_dbl_max()
{
#if defined(DBL_MAX)
	return DBL_MAX;
#elif defined(INFINITY)
    static const double val = INFINITY;
    long long next = *(long long*)&val - 1;
    return *(double *)&next;
#else
	return 1.0/bn_dbl_min();
#endif
}


double
bn_flt_min()
{
    long val = (1LL<<23);
    return *(float *)&val;
}


double
bn_flt_max()
{
#if defined(FLT_MAX)
	return FLT_MAX;
#elif defined(INFINITY)
    static const float val = INFINITY;
    long next = *(long*)&val - 1;
    return *(float *)&next;
#else
	return 1.0/bn_flt_min();
#endif
}


double
bn_flt_min_sqrt()
{
    return sqrt(bn_flt_min());
}


double
bn_flt_max_sqrt()
{
    return sqrt(bn_flt_max());
}


double
bn_dbl_min_sqrt()
{
    return sqrt(bn_dbl_min());
}


double
bn_dbl_max_sqrt()
{
    return sqrt(bn_dbl_max());
}


double
bn_ulp(double val)
{
    long long up, dn;

    if (isnan(val) || isinf(val))
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
