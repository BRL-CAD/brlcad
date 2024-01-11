/*                           U L P . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2023 United States Government as represented by
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
 */

#include "common.h"

#include <math.h>
#include <limits.h>
#include <float.h>

#if defined(__STDC_IEC_559__)
#  define HAVE_IEEE754 1
#endif

#if defined(HAVE_ISNAN) && !defined(HAVE_DECL_ISNAN) && !defined(isnan)
extern int isnan(double x);
#endif

#if defined(HAVE_ISINF) && !defined(HAVE_DECL_ISINF) && !defined(isinf)
extern int isinf(double x);
#endif


double
bn_epsilon(void)
{
#if defined(DBL_EPSILON)
    return DBL_EPSILON;
#elif defined(HAVE_IEEE754)
    union {
        double d;
        long long ll;
    } val;
    val.d = 1.0;
    val.ll += 1;
    return val.d - 1.0;
#else
    /* static for computed epsilon so it's only calculated once. */
    static double tol = 0.0;

    if (tol == 0.0) {
        /* volatile to avoid long registers and compiler optimizing away the loop */
        volatile double temp_tol = 1.0;
        while (1.0 + (temp_tol * 0.5) > 1.0) {
            temp_tol *= 0.5;
        }
        tol = temp_tol;
    }

    return tol;
#endif
}


float
bn_epsilonf(void)
{
#if defined(FLT_EPSILON)
    return FLT_EPSILON;
#elif defined(HAVE_IEEE754)
    union {
        float f;
        long long ll;
    } val;
    val.f = 1.0;
    val.ll += 1;
    return val.f - 1.0;
#else
    /* static for computed epsilon so it's only calculated once. */
    static float tol = 0.0;

    if (tol == 0.0) {
        /* volatile to avoid long registers and compiler optimizing away the loop */
        volatile float temp_tol = 1.0f;
        while (1.0f + (temp_tol * 0.5f) > 1.0f) {
            temp_tol *= 0.5f;
        }
        tol = temp_tol;
    }

    return tol;
#endif
}


double
bn_dbl_min(void)
{
#if defined(DBL_MIN)
    return DBL_MIN;
#else
    union {
        double d;
        unsigned long long ull;
    } minVal;

    /* set exponent to min non-subnormal value (i.e., 1) */
    minVal.ull = 1ULL << 52; /* 52 zeros for the fraction*/

    return minVal.d;
#endif
}


double
bn_dbl_max(void)
{
#if defined(DBL_MAX)
    return DBL_MAX;
#elif defined(INFINITY)
    static const double val = INFINITY;
    long long next = *(long long*)&val - 1;
    return *(double *)&next;
#else
    static double max_val = 0.0;

    if (max_val == 0.0) {
        double val = 1.0;
        double prev_val;

	/* double until it no longer doubles */
        do {
            prev_val = val;
            val *= 2.0;
        } while (!isinf(val) && val > prev_val);

        max_val = prev_val;
    }

    return max_val;
#endif
}


float
bn_flt_min(void)
{
#if defined(FLT_MIN)
    return FLT_MIN;
#else
    union {
        float f;
        long long ll;
    } minVal;

    // set exponent to min non-subnormal value (i.e., 1)
    minVal.ll = 1LL << 23; // 23 zeros for the fraction

    return minVal.f;
#endif
}


float
bn_flt_max(void)
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


float
bn_flt_min_sqrt(void)
{
    return sqrt(bn_flt_min());
}


float
bn_flt_max_sqrt(void)
{
    return sqrt(bn_flt_max());
}


double
bn_dbl_min_sqrt(void)
{
    return sqrt(bn_dbl_min());
}


double
bn_dbl_max_sqrt(void)
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
