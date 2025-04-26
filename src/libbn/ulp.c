/*                           U L P . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2025 United States Government as represented by
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

/*interface header */
#include "bn/ulp.h"

#include <math.h>     /* for isnan(), isinf() */
#include <limits.h>
#include <float.h>
#include <stdint.h>   /* for int32_t, uint64_t */

// Temporary Wfloat-equal disablement
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#if defined(__STDC_IEC_559__)
#  define HAVE_IEEE754 1
#endif

#if defined(HAVE_ISNAN) && !defined(HAVE_DECL_ISNAN) && !defined(isnan)
extern int isnan(double x);
#endif

#if defined(HAVE_ISINF) && !defined(HAVE_DECL_ISINF) && !defined(isinf)
extern int isinf(double x);
#endif


/* tag‑type unions for bit‑punning */
union dbl_bits {
    double    d;
    int64_t   i;
    uint64_t  u;
};
union flt_bits {
    float     f;
    int32_t   i;
    uint32_t  u;
};


double
bn_dbl_epsilon(void)
{
#if defined(DBL_EPSILON)
    return DBL_EPSILON;
#elif defined(HAVE_IEEE754)
    union dbl_bits val;
    val.d = 1.0;
    val.i += 1;
    return val.d - 1.0;
#else
    /* static for computed epsilon so it's only calculated once. */
    static double tol = 0.0;

    if (!tol) {
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
bn_flt_epsilon(void)
{
#if defined(FLT_EPSILON)
    return FLT_EPSILON;
#elif defined(HAVE_IEEE754)
    union flt_bits val;
    val.f = 1.0;
    val.d += 1;
    return val.f - 1.0;
#else
    /* static for computed epsilon so it's only calculated once. */
    static float tol = 0.0;

    if (!tol) {
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
    union dbl_bits minVal;

    /* set exponent to min non-subnormal value (i.e., 1) */
    minVal.u = 1 << 52; /* 52 zeros for the fraction*/

    return minVal.d;
#endif
}


double
bn_dbl_max(void)
{
#if defined(DBL_MAX)
    return DBL_MAX;
#elif defined(INFINITY)
    union dbl_bits val;

    val.d = INFINITY;
    val.u -= 1;
    return val.d;
#else
    static double max_val = 0.0;

    if (!max_val) {
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
    union flt_bits minVal;

    // set exponent to min non-subnormal value (i.e., 1)
    minVal.d = 1 << 23; // 23 zeros for the fraction

    return minVal.f;
#endif
}


float
bn_flt_max(void)
{
#if defined(FLT_MAX)
    return FLT_MAX;
#elif defined(INFINITY)
    union flt_bits bits;
    bits.f = INFINITY;
    bits.i--;
    return bits.f;
#else
    return 1.0/bn_flt_min();
#endif
}


float
bn_flt_min_sqrt(void)
{
    return (float)sqrt(bn_flt_min());
}


float
bn_flt_max_sqrt(void)
{
    return (float)sqrt(bn_flt_max());
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
bn_nextafter_up(double val)
{
    union dbl_bits bits;

    if (isnan(val) || isinf(val))
	return val;

    /* unify both +0 and -0 into +0’s next subnormal */
    if (fabs(val) < DBL_EPSILON) {
        bits.u = 1ULL; /* raw 0x0000…001 */
        return bits.d;
    }

    bits.d = val;
    bits.i++; /* always moves toward +∞ */
    return bits.d;
}


double
bn_nextafter_dn(double val)
{
    union dbl_bits bits;

    if (isnan(val) || isinf(val))
	return val;

    /* unify both +0 and -0 into -0’s next subnormal */
    if (fabs(val) < DBL_EPSILON) {
        /* raw, largest negative subnormal */
        bits.i = (int64_t)0x8000000000000001ULL;
        return bits.d;
    }

    bits.d = val;
    bits.i--; /* always moves toward −∞ */
    return bits.d;
}


float
bn_nextafterf_up(float val)
{
    union flt_bits bits;

    if (isnan(val) || isinf(val))
	return val;

    if (fabs(val) < FLT_EPSILON) {
        bits.u = 1U; /* raw 0x00000001 */
        return bits.f;
    }

    bits.f = val;
    bits.i++; /* always moves toward +∞ */
    return bits.f;
}


float
bn_nextafterf_dn(float val)
{
    union flt_bits bits;

    if (isnan(val) || isinf(val))
	return val;

    /* unify both +0 and -0 into -0’s next subnormal */
    if (fabs(val) < FLT_EPSILON) {
        /* raw, largest negative subnormal */
        bits.u = 0x80000001U;
        return bits.f;
    }

    bits.f = val;
    bits.i--; /* always moves toward −∞ */
    return bits.f;
}


double
bn_ulp(double val)
{
    union dbl_bits bits;

    if (isnan(val) || isinf(val))
	return val;

    bits.d = val;
    bits.u++; /* next "bigger" val */
    if (fabs(val) < DBL_EPSILON || val > 0.0) {
	return bits.d - val;
    }
    return -bits.d + val;
}


float
bn_ulpf(float val)
{
    union flt_bits bits;

    if (isnan(val) || isinf(val))
	return val;

    bits.f = val;
    bits.i++; /* next "bigger" val */
    if (fabs(val) < FLT_EPSILON || val > 0.0f) {
	return bits.f - val;
    }
    return -bits.f + val;
}

// Temporary Wfloat-equal disablement
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
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
