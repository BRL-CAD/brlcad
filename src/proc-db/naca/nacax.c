/*                      N A C A X . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file nacax.c
 *
 * BRL-CAD's translation of the calculation of coordinates for NACA airfoils is
 * based on the public domain program naca456 written by Ralph Carmichael of
 * Public Domain Aeronautical Software (PDAS):
 *
 * http://www.pdas.com/naca456.html
 *
 * naca456 is in turn based off of earlier work by several authors at NASA,
 * documented in reports NASA TM X-3284, NASA TM X-3069 and NASA TM 4741. The
 * program naca456 is documented in the paper:
 *
 * Carmichael, Ralph L.: Algorithm for Calculating Coordinates of Cambered NACA
 * Airfoils At Specified Chord Locations. AIAA Paper 2001-5235, November 2001.
 *
 * Disclaimer, per the PDAS distribution:
 *
 * Although many of the works contained herein were developed by national
 * laboratories or their contractors, neither the U.S. Government nor Public
 * Domain Aeronautical Software make any warranty as to the accuracy or
 * appropriateness of the procedure to any particular application.
 *
 * The programs and descriptions have been collected and reproduced with great
 * care and attention to accuracy, but no guarantee or warranty is implied. All
 * programs are offered AS IS and Public Domain Aeronautical Software does not
 * give any express or implied warranty of any kind and any implied warranties are
 * disclaimed. Public Domain Aeronautical Software will not be liable for any
 * direct, indirect, special, incidental, or consequential damages arising out of
 * any use of this software. In no case should the results of any computational
 * scheme be used as a substitute for sound engineering practice and judgment.
 **
 */

#include "common.h"

#include <stdlib.h>

#include "bu/defines.h"
#include "vmath.h"
#include "bn.h"

#include "naca.h"

/* Forward declarations */

void MeanLine6(fastf_t a, fastf_t cl, struct fortran_array *x, struct fortran_array *ym, struct fortran_array *ymp);

void ParametrizeAirfoil(struct fortran_array *xupper, struct fortran_array *yupper, struct fortran_array *xlower, struct fortran_array *ylower,
			struct fortran_array *s, struct fortran_array *x, struct fortran_array *y);

static fastf_t Polynomial(struct fortran_array *c, fastf_t x);

void SetSixDigitPoints(int family, fastf_t tc, struct fortran_array *xt, struct fortran_array *yt);


/**
 * If x(1:n) and y(1:n) define an airfoil surface from leading edge to
 * trailing edge and if the final point has x < 1.0, add an additional
 * point that extrapolates to x = 1.0.  x and y must be dimensioned at
 * least n + 1 or no action is taken.
 */
static void
AddTrailingEdgePointIfNeeded(int *n, struct fortran_array *x, struct fortran_array *y)
{
    fastf_t slope;

    if (SIZE(x) <= *n) { return; }
    if (SIZE(y) <= *n) { return; }
    if (INDEX(x, *n) >= 1.0 ) { return; }

    slope = (INDEX(y, *n) - INDEX(y, *n - 1)) / (INDEX(x, *n) - INDEX(x, *n - 1));
    INDEX(x, *n + 1) = 1.0;
    INDEX(y, *n + 1) = INDEX(y, *n) + slope * (1.0 - INDEX(x, *n));
    *n = *n + 1;
}

/**
 * The a1 variable used for the modified 4-digit thickness
 */
static fastf_t
CalA1(fastf_t a0, fastf_t a2, fastf_t a3, fastf_t xmt)
{
    fastf_t v1, v2, v3;

    v1 = 0.5 * a0 / sqrt(xmt);
    v2 = 2.0 * a2 * xmt;
    v3 = 3.0 * a3 * xmt * xmt;
    return -v1 - v2 - v3;
}

/**
 * The a2 variable used for the modified 4-digit thickness
 */
static fastf_t
CalA2(fastf_t a0, fastf_t a3, fastf_t xmt)
{
    fastf_t v1, v2, v3;

    v1 = 0.1 / (xmt * xmt);
    v2 = 0.5 * a0 / sqrt(xmt * xmt * xmt);
    v3 = 2.0 * a3 * xmt;
    return -v1 + v2 - v3;
}

/**
 * The a2 variable used for the modified 4-digit thickness
 */
static fastf_t
CalA3(fastf_t a0, fastf_t d1, fastf_t xmt)
{
    fastf_t omxmt;
    fastf_t v1, v2, v3;

    omxmt = 1.0 - xmt;
    v1 = 0.1 / (xmt * xmt * xmt);
    v2 = (d1 * omxmt - 0.294) / (xmt * omxmt * omxmt);
    v3 = (3.0 / 8.0) * a0 / pow(xmt, 2.5);
    return v1 + v2 - v3;
}

/**
 * Calculate d1, the trailing edge half angle, used for the
 * modified 4-digit thickness distribution.
 *
 * METHOD - Curve fit of fourth order polynomial to the data:
 *   m     d1
 *  0.2  0.200
 *  0.3  0.234
 *  0.4  0.315
 *  0.5  0.465
 *  0.6  0.700
 *
 * @param[in] m x-location of max thickness, fraction of chord
 */
static fastf_t
CalculateD1(fastf_t m)
{
    /* A(1) = constant, A(2) = linear, etc. */
    const fastf_t A_c[] = { 3.48e-5, 2.3076628, -10.127712, 19.961478, -10.420597 };
    struct fortran_array *A;
    C2F(A, A_c, 5);
    return Polynomial(A, m);
}

void
CombineThicknessAndCamber(struct fortran_array *x, struct fortran_array *thick, struct fortran_array *y, struct fortran_array *yp,
			  struct fortran_array *xupper, struct fortran_array *yupper, struct fortran_array *xlower, struct fortran_array *ylower)
{
    int n;
    int i;
    struct fortran_array *s, *c;

    n = SIZE(x);
    ALLOCATE(s, n);
    ALLOCATE(c, n);
    for (i = 1; i <= n; i++) {
	INDEX(s, i) = sin(atan(INDEX(yp, i)));
	INDEX(c, i) = cos(atan(INDEX(yp, i)));

	INDEX(xupper, i) = INDEX(x, i) - INDEX(thick, i) * INDEX(s, i);
	INDEX(yupper, i) = INDEX(y, i) + INDEX(thick, i) * INDEX(c, i);

	INDEX(xlower, i) = INDEX(x, i) + INDEX(thick, i) * INDEX(s, i);
	INDEX(ylower, i) = INDEX(y, i) - INDEX(thick, i) * INDEX(c, i);
    }
}

void
GetRk1(fastf_t x, fastf_t *r, fastf_t *k1)
{
    const fastf_t M_C[] = {0.05, 0.1, 0.15, 0.2, 0.25};
    const fastf_t RTAB_C[] = {0.0580, 0.126, 0.2025,  0.29, 0.391};
    const fastf_t KTAB_C[] = {361.4, 51.64, 15.957, 6.643, 3.23};
    struct fortran_array *M, *RTAB, *KTAB;
    C2F(M, M_C, 5);
    C2F(RTAB, RTAB_C, 5);
    C2F(KTAB, KTAB_C, 5);

    *r = TableLookup(M, RTAB, 1, x);
    *k1 = TableLookup(M, KTAB, 1, x);
}

void
GetRk1k2(fastf_t x, fastf_t *r, fastf_t *k1, fastf_t *k2)
{
    const fastf_t M_C[] = {0.1, 0.15, 0.2, 0.25};
    const fastf_t RTAB_C[] = {0.13, 0.217, 0.318,  0.441};
    const fastf_t KTAB_C[] = {51.99, 15.793, 6.52, 3.191};
    struct fortran_array *M, *RTAB, *KTAB;
    C2F(M, M_C, 4);
    C2F(RTAB, RTAB_C, 4);
    C2F(KTAB, KTAB_C, 4);

    *r = TableLookup(M, RTAB, 1, x);
    *k1 = TableLookup(M, KTAB, 1, x);
    *k2 = (3.0 * pow((*r - x), 2) - pow(*r, 3));
}

void
InterpolateCombinedAirfoil(struct fortran_array *x, struct fortran_array *yt, struct fortran_array *ymean, struct fortran_array *ymeanp,
			   struct fortran_array *yu, struct fortran_array *yl,
			   struct fortran_array *yup, struct fortran_array *ylp)
{
    fastf_t dxds, dyds;
    int errCode;
    int k;
    int n, nn;
    int nupper, nlower;
    fastf_t sbar = 0.0;
    const fastf_t TOL = 1e-6;
    struct fortran_array *xupper, *yupper, *xlower, *ylower;
    struct fortran_array *xupper_short, *yupper_short, *xlower_short, *ylower_short;
    struct fortran_array *xLocal, *yLocal, *sLocal, *xpLocal, *ypLocal;
    struct fortran_array *xLocal_start, *yLocal_start, *sLocal_start, *xpLocal_start, *ypLocal_start;
    struct fortran_array *xLocal_end, *yLocal_end, *sLocal_end, *xpLocal_end, *ypLocal_end;

    n = SIZE(x);

    ALLOCATE(xupper, n + 1);
    ALLOCATE(yupper, n + 1);
    ALLOCATE(xlower, n + 1);
    ALLOCATE(ylower, n + 1);
    CombineThicknessAndCamber(x, yt, ymean, ymeanp, xupper, yupper, xlower, ylower);
    nupper = n;
    AddTrailingEdgePointIfNeeded(&nupper, xupper, yupper); /* might add 1 point */
    nlower = n;
    AddTrailingEdgePointIfNeeded(&nlower, xupper, yupper); /* might add 1 point */

    nn = nupper + nlower - 1;
    ALLOCATE(xLocal, nn);
    ALLOCATE(yLocal, nn);
    ALLOCATE(sLocal, nn);
    ALLOCATE(xpLocal, nn);
    ALLOCATE(ypLocal, nn);
    SLICE(xupper_short, xupper, 1, nupper);
    SLICE(yupper_short, yupper, 1, nupper);
    SLICE(xlower_short, xlower, 1, nlower);
    SLICE(ylower_short, ylower, 1, nlower);
    ParametrizeAirfoil(xupper_short, yupper_short, xlower_short, ylower_short, sLocal, xLocal, yLocal);
    DEALLOCATE(xupper);
    DEALLOCATE(yupper);
    DEALLOCATE(xlower);
    DEALLOCATE(ylower);
    DEALLOCATE(xupper_short);
    DEALLOCATE(yupper_short);
    DEALLOCATE(xlower_short);
    DEALLOCATE(ylower_short);

    /* Now fit splines to xLocal vs. sLocal and yLocal vs. sLocal.
     * This is a total fit around upper and lower surfaces.
     */
    FMMspline(sLocal, xLocal, xpLocal);
    FMMspline(sLocal, yLocal, ypLocal);

    SLICE(xLocal_start, xLocal, 1, nupper);
    SLICE(yLocal_start, yLocal, 1, nupper);
    SLICE(sLocal_start, sLocal, 1, nupper);
    SLICE(xpLocal_start, xpLocal, 1, nupper);
    SLICE(ypLocal_start, ypLocal, 1, nupper);
    SLICE(xLocal_end, xLocal, nupper, nn);
    SLICE(yLocal_end, yLocal, nupper, nn);
    SLICE(sLocal_end, sLocal, nupper, nn);
    SLICE(xpLocal_end, xpLocal, nupper, nn);
    SLICE(ypLocal_end, ypLocal, nupper, nn);

    for (k = 1; k <= n; k++) {
	SplineZero(sLocal_start, xLocal_start, xpLocal_start, INDEX(x, k), TOL, &sbar, &errCode);
	if (errCode != 0) {
	    fprintf(stderr, "errCode not zero from SplineZero\n");
	}
	PClookup(sLocal_start, xLocal_start, xpLocal_start, sbar, NULL, &dxds, NULL, NULL);
	PClookup(sLocal_start, yLocal_start, ypLocal_start, sbar, &INDEX(yu, k), &dyds, NULL, NULL);
	if (yup) {
	    if (ZERO(dxds)) {
		INDEX(yup, k) = 0.0;
	    } else {
		INDEX(yup, k) = dyds / dxds;
	    }
	}

	SplineZero(sLocal_end, xLocal_end, xpLocal_end, INDEX(x, k), TOL, &sbar, &errCode);
	if (errCode != 0) {
	    fprintf(stderr, "errCode not zero\n");
	}
	PClookup(sLocal_end, xLocal_end, xpLocal_end, sbar, NULL, &dxds, NULL, NULL);
	PClookup(sLocal_end, yLocal_end, ypLocal_end, sbar, &INDEX(yl, k), &dyds, NULL, NULL);
	if (ylp) {
	    if (ZERO(dxds)) {
		INDEX(ylp, k) = 0.0;
	    } else {
		INDEX(ylp, k) = dyds / dxds;
	    }
	}
    }

    DEALLOCATE(ypLocal_end);
    DEALLOCATE(xpLocal_end);
    DEALLOCATE(sLocal_end);
    DEALLOCATE(yLocal_end);
    DEALLOCATE(xLocal_end);
    DEALLOCATE(ypLocal_start);
    DEALLOCATE(xpLocal_start);
    DEALLOCATE(sLocal_start);
    DEALLOCATE(yLocal_start);
    DEALLOCATE(xLocal_start);
    DEALLOCATE(ypLocal);
    DEALLOCATE(xpLocal);
    DEALLOCATE(sLocal);
    DEALLOCATE(yLocal);
    DEALLOCATE(xLocal);
}

void
InterpolateUpperAndLower(struct fortran_array *xupper, struct fortran_array *yupper,
			 struct fortran_array *xlower, struct fortran_array *ylower,
			 struct fortran_array *x,
			 struct fortran_array *yu, struct fortran_array *yl,
			 struct fortran_array *yup, struct fortran_array *ylp)
{
    fastf_t dxds, dyds;
    int errCode;
    int k;
    int nn;
    int nupper, nlower;
    fastf_t sbar = 0.0;
    const fastf_t TOL = 1e-6;
    struct fortran_array *xupperCopy, *yupperCopy, *xlowerCopy, *ylowerCopy;
    struct fortran_array *xupperCopy_short, *yupperCopy_short, *xlowerCopy_short, *ylowerCopy_short;
    struct fortran_array *xLocal, *yLocal, *sLocal, *xpLocal, *ypLocal;
    struct fortran_array *xLocal_start, *yLocal_start, *sLocal_start, *xpLocal_start, *ypLocal_start;
    struct fortran_array *xLocal_end, *yLocal_end, *sLocal_end, *xpLocal_end, *ypLocal_end;
    nupper = SIZE(xupper);
    nlower = SIZE(xlower);
    ALLOCATE(xupperCopy, nupper + 1);
    ALLOCATE(yupperCopy, nupper + 1);
    ALLOCATE(xlowerCopy, nlower + 1);
    ALLOCATE(ylowerCopy, nlower + 1);
    COPY(xupperCopy, xupper, 1, nupper);
    COPY(yupperCopy, yupper, 1, nupper);
    COPY(xlowerCopy, xlower, 1, nlower);
    COPY(ylowerCopy, ylower, 1, nlower);
    AddTrailingEdgePointIfNeeded(&nupper, xupperCopy, yupperCopy);
    AddTrailingEdgePointIfNeeded(&nlower, xlowerCopy, ylowerCopy);

    nn = nupper + nlower - 1;

    ALLOCATE(xLocal, nn);
    ALLOCATE(yLocal, nn);
    ALLOCATE(sLocal, nn);
    ALLOCATE(xpLocal, nn);
    ALLOCATE(ypLocal, nn);
    ALLOCATE(xupperCopy_short, nupper);
    ALLOCATE(yupperCopy_short, nupper);
    ALLOCATE(xlowerCopy_short, nupper);
    ALLOCATE(ylowerCopy_short, nupper);
    SLICE(xupperCopy_short, xupperCopy, 1, nupper);
    SLICE(yupperCopy_short, yupperCopy, 1, nupper);
    SLICE(xlowerCopy_short, xlowerCopy, 1, nlower);
    SLICE(ylowerCopy_short, ylowerCopy, 1, nlower);
    ParametrizeAirfoil(xupperCopy_short, yupperCopy_short, xlowerCopy_short, ylowerCopy_short, sLocal, xLocal, yLocal);
    DEALLOCATE(ylowerCopy_short);
    DEALLOCATE(xlowerCopy_short);
    DEALLOCATE(yupperCopy_short);
    DEALLOCATE(xupperCopy_short);
    DEALLOCATE(ylowerCopy);
    DEALLOCATE(xlowerCopy);
    DEALLOCATE(yupperCopy);
    DEALLOCATE(xupperCopy);

    FMMspline(sLocal, xLocal, xpLocal);
    FMMspline(sLocal, yLocal, ypLocal);

    SLICE(xLocal_start, xLocal, 1, nupper);
    SLICE(yLocal_start, yLocal, 1, nupper);
    SLICE(sLocal_start, sLocal, 1, nupper);
    SLICE(xpLocal_start, xpLocal, 1, nupper);
    SLICE(ypLocal_start, ypLocal, 1, nupper);
    SLICE(xLocal_end, xLocal, nupper, nn);
    SLICE(yLocal_end, yLocal, nupper, nn);
    SLICE(sLocal_end, sLocal, nupper, nn);
    SLICE(xpLocal_end, xpLocal, nupper, nn);
    SLICE(ypLocal_end, ypLocal, nupper, nn);

    for (k = 1; k <= SIZE(x); k++) {
	SplineZero(sLocal_start, xLocal_start, xpLocal_start, INDEX(x, k), TOL, &sbar, &errCode);
	if (errCode != 0) {
	    fprintf(stderr, "errCode not zero\n");
	}
	PClookup(sLocal_start, xLocal_start, xpLocal_start, sbar, NULL, &dxds, NULL, NULL);
	PClookup(sLocal_start, yLocal_start, ypLocal_start, sbar, &INDEX(yu, k), &dyds, NULL, NULL);
	if (yup) {
	    if (ZERO(dxds)) {
		INDEX(yup, k) = 0.0;
	    } else {
		INDEX(yup, k) = dyds / dxds;
	    }
	}

	SplineZero(sLocal_end, xLocal_end, xpLocal_end, INDEX(x, k), TOL, &sbar, &errCode);
	if (errCode != 0) {
	    fprintf(stderr, "errCode not zero\n");
	}
	PClookup(sLocal_end, xLocal_end, xpLocal_end, sbar, NULL, &dxds, NULL, NULL);
	PClookup(sLocal_end, yLocal_end, ypLocal_end, sbar, &INDEX(yl, k), &dyds, NULL, NULL);
	if (ylp) {
	    if (ZERO(dxds)) {
		INDEX(ylp, k) = 0.0;
	    } else {
		INDEX(ylp, k) = dyds / dxds;
	    }
	}
    }

    DEALLOCATE(ypLocal_end);
    DEALLOCATE(xpLocal_end);
    DEALLOCATE(sLocal_end);
    DEALLOCATE(yLocal_end);
    DEALLOCATE(xLocal_end);
    DEALLOCATE(ypLocal_start);
    DEALLOCATE(xpLocal_start);
    DEALLOCATE(sLocal_start);
    DEALLOCATE(yLocal_start);
    DEALLOCATE(xLocal_start);
    DEALLOCATE(ypLocal);
    DEALLOCATE(xpLocal);
    DEALLOCATE(sLocal);
    DEALLOCATE(yLocal);
    DEALLOCATE(xLocal);
}

fastf_t
LeadingEdgeRadius4(fastf_t toc)
{
    const fastf_t A = 1.1019; /* Eq. 6.3, p.114 in Abbot & von Doenhoff */
    return A * pow(toc, 2);
}

fastf_t
LeadingEdgeRadius4M(fastf_t toc, fastf_t leIndex)
{
    const fastf_t A = 1.1019 / 36.0; /* See p.117 in Abbot & von Doenhoff */
    return A * pow(toc * leIndex, 2);
}

fastf_t
LeadingEdgeRadius6(fastf_t family, fastf_t toc)
{
    struct fortran_array *xt, *yt;
    struct fortran_array *minus_yt;
    struct fortran_array *sLocal, *xLocal, *yLocal, *xpLocal, *ypLocal;
    fastf_t xpp, yp;
    ALLOCATE(xt, 201);
    ALLOCATE(yt, 201);
    ALLOCATE(minus_yt, 201);
    ALLOCATE(sLocal, 401);
    ALLOCATE(xLocal, 401);
    ALLOCATE(yLocal, 401);
    ALLOCATE(xpLocal, 401);
    ALLOCATE(ypLocal, 401);
    SetSixDigitPoints(family, toc, xt, yt);
    VSCALEN(F2C(minus_yt), F2C(yt), -1.0, 201);
    ParametrizeAirfoil(xt, yt, xt, minus_yt, sLocal, xLocal, yLocal);
    FMMspline(sLocal, xLocal, xpLocal);
    FMMspline(sLocal, yLocal, ypLocal);
    PClookup(sLocal, xLocal, xpLocal, INDEX(sLocal, 201), NULL, NULL, &xpp, NULL);
    PClookup(sLocal, yLocal, ypLocal, INDEX(sLocal, 201), NULL, &yp, NULL, NULL);

    ALLOCATE(ypLocal, 401);
    DEALLOCATE(xpLocal);
    DEALLOCATE(yLocal);
    DEALLOCATE(xLocal);
    DEALLOCATE(sLocal);
    DEALLOCATE(minus_yt);
    DEALLOCATE(yt);
    DEALLOCATE(xt);

    return yp * yp / xpp;
}

void
LoadX(int denCode, int nx, struct fortran_array *x)
{
    const fastf_t COARSE[26] = {0.0,0.005,0.0075,0.0125,
				0.025,0.05,0.075,0.1,0.15,0.2,0.25,0.3,0.35,0.4,0.45,0.5,0.55,
				0.6,0.65,0.7,0.75,0.8,0.85,0.9,0.95,1.0};
    const fastf_t MEDIUM[64] = {0.0, 0.0002, 0.0005, 0.001,
				0.0015,0.002,0.005,0.01,0.015,0.02,0.03,0.04,0.05,0.06,0.08,0.10,0.12,
				0.14,0.16,0.18,0.20,0.22,0.24,0.26,0.28,0.30,0.32,0.34,0.36,0.38,0.40,
				0.42,0.44,0.46,0.48,0.50,0.52,0.54,0.56,0.58,0.60,0.62,0.64,0.66,0.68,
				0.70,0.72,0.74,0.76,0.78,0.80,0.82,0.84,0.86,0.88,0.90,0.92,0.94,0.96,
				0.97,0.98,0.99,0.995,1.0};
    const fastf_t FINE[98] = {0.0, 0.00005, 0.0001, 0.0002,
			      0.0003, 0.0004, 0.0005, 0.0006, 0.0008, 0.0010, 0.0012, 0.0014, 0.0016,
			      0.0018,0.002,0.0025,0.003,0.0035,0.004,0.0045,0.005,0.006,0.007,0.008,
			      0.009, 0.01,0.011,0.012,0.013,0.014,0.015,0.016,0.018,0.02,0.025,0.03,
			      0.035,0.04,0.045,0.05,0.055,0.06,0.07,0.08,0.09,0.10,0.12,
			      0.14,0.16,0.18,0.20,0.22,0.24,0.26,0.28,0.30,0.32,0.34,0.36,0.38,0.40,
			      0.42,0.44,0.46,0.48,0.50,0.52,0.54,0.56,0.58,0.60,0.62,0.64,0.66,0.68,
			      0.70,0.72,0.74,0.76,0.78,0.80,0.82,0.84,0.86,0.88,0.90,0.92,0.94,0.95,
			      0.96,0.97,0.975,0.98,0.985,0.99,0.995,0.999,1.0};
    switch (denCode) {
    case 2:
	nx = (int) (sizeof(MEDIUM) / sizeof(fastf_t));
	VMOVEN(F2C(x), MEDIUM, nx);
	break;
    case 3:
	nx = (int) (sizeof(FINE) / sizeof(fastf_t));
	VMOVEN(F2C(x), FINE, nx);
	break;
    default: /* denCode = 1 and anything else (probably an accident) */
	nx = (int) (sizeof(COARSE) / sizeof(fastf_t));
	VMOVEN(F2C(x), COARSE, nx);
	break;
    }
}

void
MeanLine2(fastf_t cmax,
	  fastf_t xmaxc,
	  struct fortran_array *x,
	  struct fortran_array *ym, struct fortran_array *ymp)
{
    int k, n;
    fastf_t slope, slope1, slope2;
    fastf_t theta;

    slope1 = 2.0 * cmax / xmaxc;
    slope2 = -2.0 * cmax / (1.0 - xmaxc);
    n = SIZE(x);
    for (k = 1; k <= n; k++) {
	if (INDEX(x, k) < xmaxc) {
	    theta = INDEX(x, k) / xmaxc;
	    slope = slope1;
	} else {
	    theta = (1.0 - INDEX(x, k)) / (1.0 - xmaxc);
	    slope = slope2;
	}
	INDEX(ym, k) = theta * (2.0 - theta);
	INDEX(ymp, k) = slope * (1.0 - theta);
    }

    VSCALEN(F2C(ym), F2C(ym), cmax, n);
}

void
MeanLine3(fastf_t cl,
	  fastf_t xmaxc,
	  struct fortran_array *x,
	  struct fortran_array *ym, struct fortran_array *ymp)
{
    int k, n;
    fastf_t k1; /* related to y at x = r */
    fastf_t r; /* fraction of chord where 2nd deriv goes to zero */
    fastf_t xx;

    n = SIZE(x);
    GetRk1(xmaxc, &r, &k1);

    for (k = 1; k <= n; k++) {
	xx = INDEX(x, k);
	if (xx < r) {
	    INDEX(ym, k) = xx * (xx * (xx - 3.0 * r) + r * r * (3.0 - r));
	    INDEX(ymp, k) = 3.0 * xx * (xx - r -r) + r * r * (3.0 - r);
	} else {
	    INDEX(ym, k) = r * r * r * (1.0 - xx);
	    INDEX(ymp, k) = -r * r *r;
	}

	INDEX(ym, k) = (k1 * cl / 1.8) * INDEX(ym, k);
	INDEX(ymp, k) = (k1 * cl / 1.8) * INDEX(ymp, k);
    }
}

void
MeanLine3Reflex(fastf_t cl,
		fastf_t xmaxc,
		struct fortran_array *x,
		struct fortran_array *ym, struct fortran_array *ymp)
{
    int k, n;
    fastf_t k1, k21;
    fastf_t mr3; /* (1.0 - r) ** 3 */
    fastf_t r;
    fastf_t r3;
    fastf_t xx;

    n = SIZE(x);
    GetRk1k2(xmaxc, &r, &k1, &k21);

    r3 = pow(r, 3);
    mr3 = pow(1.0 - r, 3);

    for (k = 1; k <= n; k++) {
	xx = INDEX(x, k);
	if (xx < r) {
	    INDEX(ym, k) = pow(xx - r, 3) - k21 * mr3 * xx - xx * r3 + r3;
	    INDEX(ymp, k) = 3.0 * pow(xx - r, 2) - k21 * mr3 - r3;
	} else {
	    INDEX(ym, k) = k21 * pow(xx - r, 3) - k21 * mr3 * xx - xx * r3 + r3;
	    INDEX(ymp, k) = 3.0 * k21 * pow(xx - r, 2) - k21 * mr3 - r3;
	}
	INDEX(ym, k) = (k1 * cl / 1.8) * INDEX(ym, k);
	INDEX(ymp, k) = (k1 * cl / 1.8) * INDEX(ymp, k);
    }
}

void
MeanLine6(fastf_t a, fastf_t cl, struct fortran_array *x, struct fortran_array *ym, struct fortran_array *ymp)
{
    fastf_t amx;
    fastf_t g, h;
    int k, n;
    fastf_t oma;
    fastf_t omx;
    fastf_t term1, term2, term1p, term2p;
    fastf_t xx;

    const fastf_t EPS = 1e-7;

    n = SIZE(x);
    VSETALLN(F2C(ym), 0, n);
    VSETALLN(F2C(ymp), 0, n);

    oma = 1.0 - a;
    if (fabs(oma) < EPS) {
	for (k = 1; k <= n; k++) {
	    xx = INDEX(x, k);
	    omx = 1.0 - xx;
	    if ((xx < EPS) || (omx < EPS)) {
		INDEX(ym, k) = 0.0;
		INDEX(ymp, k) = 0.0;
		continue;
	    }
	    INDEX(ym, k) = omx * log(omx) + xx * log(xx);
	    INDEX(ymp, k) = log(omx) - log(xx);
	}
	VSCALEN(F2C(ym), F2C(ym), -1 * cl * 0.25 * M_PI, n);
	VSCALEN(F2C(ymp), F2C(ymp), cl * 0.25 * M_PI, n);
	return;
    }

    for (k = 1; k <= n; k++) {
	xx = INDEX(x, k);
	omx = 1.0 - xx;
	if ((xx < EPS) || (fabs(omx) < EPS)) {
	    INDEX(ym, k) = 0.0;
	    INDEX(ymp, k) = 0.0;
	    continue;
	}

	if (fabs(a) < EPS) {
	    g = -0.25;
	    h = -0.5;
	} else {
	    g = -(a * a * (0.5 * log(a) - 0.25) + 0.25) / oma;
	    h = g + (0.5 * oma * oma * log(oma) - 0.25 * oma * oma) / oma;
	}

	amx = a - xx;
	if (fabs(amx) < EPS) {
	    term1 = 0.0;
	    term1p = 0.0;
	} else {
	    term1 = amx * amx * (2.0 * log(fabs(amx)) - 1.0);
	    term1p = -amx * log(fabs(amx));
	}

	term2 = omx * omx * (1.0 - 2.0 * log(omx));
	term2p = omx * log(fabs(amx));

	INDEX(ym, k) = 0.25 * (term1 + term2) / oma - xx * log(xx) + g - h * xx;
	INDEX(ymp, k) = (term1p + term2p) / oma - 1.0 - log(xx) - h;
    }

    VSCALEN(F2C(ym), F2C(ym), cl / (M_2PI * (a + 1.0)), n);
    VSCALEN(F2C(ymp), F2C(ymp), cl / (M_2PI * (a + 1.0)), n);
}

void
MeanLine6M(fastf_t cl, struct fortran_array *x, struct fortran_array *ym, struct fortran_array *ymp)
{
    const fastf_t A = 0.8; /* chordwise extent of uniform loading 0 <= a <= 1 */
    const fastf_t EPS = 1e-7;
    fastf_t amx;
    fastf_t g, h;
    int k, n;
    fastf_t oma;
    fastf_t omx;
    fastf_t term1, term2, term1p, term2p;
    fastf_t teSlope;
    fastf_t xx;

    n = SIZE(x);
    VSETALLN(F2C(ym), 0.0, n);
    VSETALLN(F2C(ymp), 0.0, n);

    oma = 1.0 - A;
    teSlope = -0.24521 * cl;

    for (k = 1; k <= n; k++) {
	xx = INDEX(x, k);

	if (xx < EPS) {
	    INDEX(ym, k) = 0.0;
	    INDEX(ymp, k) = 0.0;
	    continue;
	}

	omx = 1.0 - xx;
	if (omx < EPS) {
	    INDEX(ym, k) = 0.0;
	    INDEX(ymp, k) = teSlope;
	    continue;
	}

	g = -(A * A * (0.5 * log(A) - 0.25) + 0.25) / oma;
	h = g + (0.5 * oma * oma * log(oma) - 0.25 * oma * oma) / oma;

	amx = A - xx;
	if (fabs(amx) < EPS) {
	    term1 = 0.0;
	    term1p = 0.0;
	} else {
	    term1 = amx * amx * (2.0 * log(fabs(amx)) - 1.0);
	    term1p = -amx * log(fabs(amx));
	}

	term2 = omx * omx * (1.0 - 2.0 * log(omx));
	term2p = omx * log(fabs(amx));

	INDEX(ym, k) = 0.25 * (term1 + term2) / oma - xx * log(xx) + g - h * xx;
	INDEX(ymp, k) = (term1p + term2p) / oma - 1.0 - log(xx) - h;
    }

    VSCALEN(F2C(ym), F2C(ym), cl / (M_2PI * (A + 1.0)), n);
    VSCALEN(F2C(ymp), F2C(ymp), cl / (M_2PI * (A + 1.0)), n);

    /* This is the special treatment */
    for (k = 1; k <= n; k++) {
	if (INDEX(x, k) > 0.86) {
	    INDEX(ym, k) = teSlope * (INDEX(x, k) - 1.0);
	    INDEX(ymp, k) = teSlope;
	}
    }
}

void
ParametrizeAirfoil(struct fortran_array *xupper, struct fortran_array *yupper, struct fortran_array *xlower, struct fortran_array *ylower,
		   struct fortran_array *s, struct fortran_array *x, struct fortran_array *y)
{
    int k, nn;
    int nupper, nlower; /* don't have to be the same */

    nupper = SIZE(xupper);
    nlower = SIZE(xlower);

    if ((SIZE(yupper) < nupper) || (SIZE(ylower) < nlower)) {
	fprintf(stderr, "DISASTER #1\n");
	exit(1);
    }

    nn = nupper + nlower - 1;
    if ((SIZE(s) < nn) || (SIZE(x) < nn) || (SIZE(y) < nn)) {
	fprintf(stderr, "DISASTER #2\n");
	exit(2);
    }

    for (k = 1; k <= nn; k++) {
	if (k > nupper) {
	    INDEX(x, k) = INDEX(xlower, k - nupper + 1);
	    INDEX(y, k) = INDEX(ylower, k - nupper + 1);
	} else {
	    INDEX(x, k) = INDEX(xupper, nupper - k + 1);
	    INDEX(y, k) = INDEX(yupper, nupper - k + 1);
	}
    }

    INDEX(s, 1) = 0.0;
    for (k = 2; k <= nn; k++) {
	INDEX(s, k) = INDEX(s, k - 1) + sqrt(pow(INDEX(x, k) - INDEX(x, k - 1), 2) + pow(INDEX(y, k) - INDEX(y, k - 1), 2));
    }
}

/**
 * Evaluate a polynomial with real coefficients at x f = c(1) + c(2) *
 * x + c(3) * x **2 + c(4) * x ** 3 + c(5) * x ** 5 +
 * ... Straightforward application of Horner's rule.
 */
static fastf_t
Polynomial(struct fortran_array *c, fastf_t x)
{
    fastf_t ff;
    int j, n;
    n = SIZE(c);
    ff = INDEX(c, n);
    for (j = n - 1; j <= 1; j--) {
	ff = ff * x + INDEX(c, j);
    }

    return ff;
}

fastf_t
ScaleFactor(int family, fastf_t tc)
{
    struct fortran_array *coeff;
    /* The original fortran uses a column for each of the 6- and
     * 6a-series families.  This uses a row instead to make copying it
     * into coeff easier. */
    const fastf_t COEFF_c[8][5] = {
	{0.0, 8.1827699, 1.3776209,  -0.092851684, 7.5942563},
	{0.0, 4.6535511, 1.038063,   -1.5041794,   4.7882784},
	{0.0, 6.5718716, 0.49376292,  0.7319794,   1.9491474},
	{0.0, 6.7581414, 0.19253769,  0.81282621,  0.85202897},
	{0.0, 6.627289,  0.098965859, 0.96759774,  0.90537584},
	{0.0, 8.1845925, 1.0492569,   1.31150930,  4.4515579},
	{0.0, 8.2125018, 0.76855961,  1.4922345,   3.6130133},
	{0.0, 8.2514822, 0.46569361,  1.50113018,  2.0908904}
    };

    if ((family < 1) || (family > 8) || (tc <= 0.0)) {
	return 0;
    }

    C2F(coeff, COEFF_c[family - 1], 5);

    return Polynomial(coeff, tc);
}

void
SetSixDigitPoints(int family, fastf_t tc, struct fortran_array *xt, struct fortran_array *yt)
{
    const fastf_t A = 1.0;
    const int NP = 201;
    fastf_t phi[201], eps[201], psi[201];
    const fastf_t *orig_eps = NULL;
    const fastf_t *orig_psi = NULL;
    bn_complex_t tmp;
    bn_complex_t z[201], zprime[201], zeta[201], zfinal[201];
    int i;
    double sf;

    for (i = 0; i < NP; i++) {
	phi[i] = i * (M_PI / (NP - 1));
    }

    switch(family) {
    case 1:
	orig_eps = EPS1;
	orig_psi = PSI1;
	break;
    case 2:
	orig_eps = EPS2;
	orig_psi = PSI2;
	break;
    case 3:
	orig_eps = EPS3;
	orig_psi = PSI3;
	break;
    case 4:
	orig_eps = EPS4;
	orig_psi = PSI4;
	break;
    case 5:
	orig_eps = EPS5;
	orig_psi = PSI5;
	break;
    case 6:
	orig_eps = EPS6;
	orig_psi = PSI6;
	break;
    case 7:
	orig_eps = EPS7;
	orig_psi = PSI7;
	break;
    case 8:
	orig_eps = EPS8;
	orig_psi = PSI8;
    }

    sf = ScaleFactor(family, tc);
    VSCALEN(eps, orig_eps, sf, NP);
    VSCALEN(psi, orig_psi, sf, NP);

    /* Now use this scaled set of epsilon and psi functions to perform
     * the conformal mapping of the circle z into the scaled airfoil
     * zfinal.  Return the real and imaginary parts as xt and yt.
     */
    for (i = 0; i < NP; i++) {
	bn_cx_cons(&z[i], psi[0] * cos(phi[i]), exp(psi[0]) * sin(phi[i]));
	bn_cx_scal(&z[i], A);
	bn_cx_cons(&zprime[i], exp(psi[i] - psi[0]) * cos(-eps[i]), exp(psi[i] - psi[i]) * sin(-eps[i]));
	bn_cx_mul(&zprime[i], &z[i]);
	bn_cx_cons(&zeta[i], A * A, 0);
	bn_cx_div(&zeta[i], &zprime[i]);
	bn_cx_add(&zeta[i], &zprime[i]);
	zfinal[i] = zeta[0];
	bn_cx_sub(&zfinal[i], &zeta[i]);
	tmp = zeta[NP - 1];
	bn_cx_sub(&tmp, &zeta[0]);
	bn_cx_scal(&zfinal[i], 1.0 / sqrt(pow(bn_cx_real(&tmp), 2) + pow(bn_cx_imag(&tmp), 2)));
	INDEX(xt, i + 1) = bn_cx_real(&zfinal[i]);
	INDEX(yt, i + 1) = -bn_cx_imag(&zfinal[i]);
    }
}


void
Thickness4(fastf_t toc, struct fortran_array *x, struct fortran_array *y, struct fortran_array *yp, struct fortran_array *ypp)
{
    const fastf_t A0 = 0.2969;
    const fastf_t A1 = -0.1260;
    const fastf_t A2 = -0.3516;
    const fastf_t A22 = A2 + A2;
    const fastf_t A3 = 0.2843;
    const fastf_t A33 = A3 + A3 + A3;
    const fastf_t A4 = -0.1014;
    const fastf_t A42 = A4 + A4, A44 = A42 + A42;
    int k, n;
    fastf_t srx = 0.0, xx = 0.0;

    n = SIZE(x);
    if (yp) { INDEX(yp, 1) = 1e22; }
    if (ypp) { INDEX(ypp, 1) = -1e22; }
    for (k = 1; k <= n; k++) { /* based on t/c = 0.2 */
	xx += INDEX(x, k);
	if (ZERO(xx)) {
	    INDEX(y, k) = 0.0;
	    if (yp) { INDEX(yp, k) = 1e22; }
	    if (ypp) { INDEX(ypp, 1) = 1e22; }
	} else {
	    srx = sqrt(xx);
	    INDEX(y, k) = A0 * srx + xx * (A1 + xx * (A2 + xx * (A3 + xx * A4)));
	    if (yp) { INDEX(yp, k) = 0.5 * A0 / srx + A1 + xx * (A22 + xx * (A33 + xx * A44)); }
	    if (ypp) { INDEX(ypp, k) = -0.25 * A0 / (srx * srx * srx) + A22 + 6.0 * xx * (A3 + xx + A42); }
	}
    }
    VSCALEN(F2C(y), F2C(y), 5.0 * toc, n); /* convert to correct t/c */
    if (yp) { VSCALEN(F2C(yp), F2C(yp), 5.0 * toc, n); }
    if (ypp) { VSCALEN(F2C(ypp), F2C(ypp), 5.0 * toc, n); }
}

void
Thickness4M(fastf_t toc, fastf_t leIndex, fastf_t xmaxt,
	    struct fortran_array *x, struct fortran_array *y, struct fortran_array *yp, struct fortran_array *ypp)
{
    fastf_t A0, A1, A2, A3;
    fastf_t A22, A33;
    fastf_t D0, D1, D2, D3;
    fastf_t D22, D33;
    int k, n;
    fastf_t omxmt, omxmsq;
    fastf_t rle;
    fastf_t srx;
    fastf_t xx;

    n = SIZE(x);

    D1 = CalculateD1(xmaxt);
    rle = LeadingEdgeRadius4M(toc, leIndex);

    A0 = (0.2 / toc) * sqrt(rle + rle);
    A3 = CalA3(A0, D1, xmaxt);
    A2 = CalA2(A0, A3, xmaxt);
    A1 = CalA1(A0, A2, A3, xmaxt);
    A22 = A2 + A2;
    A33 = A3 + A3;

    omxmt = 1.0 - xmaxt; /* calculate the "d" constants" */
    omxmsq = omxmt * omxmt;

    D3 = ((3.0 * D1) - (0.588 / omxmt)) / (3.0 * omxmsq);
    D2 = (-1.5 * omxmt * D3) - ((0.5 * D1) / omxmt);
    D0 = 0.002;
    D22 = D2 + D2;
    D33 = D3 + D3 + D3;

    for (k = 1; k <= n; k++) { /* based on t/c = 0.2 */
	xx = INDEX(x, k);
	if (ZERO(xx)) {
	    INDEX(y, k) = 0.0;
	    if (yp) { INDEX(yp, k) = 1e20; }
	    if (ypp) { INDEX(ypp, k) = -1e20; }
	    continue;
	}
	if (INDEX(x, k) < xmaxt) {
	    srx = sqrt(xx);
	    INDEX(y, k) = A0 * srx + xx * (A1 + xx * (A2 + xx * A3));
	    if (yp) { INDEX(yp, k) = 0.5 * A0 / srx + A1 + xx * (A22 + xx * A33); }
	    if (ypp) { INDEX(ypp, k) = -0.25 * A0 / (srx * srx * srx) + A22 + A33 * xx; }
	} else {
	    xx = 1.0 - INDEX(x, k);
	    INDEX(y, k) = D0 + xx * (D1 + xx * (D2 + xx * D3));
	    if (yp) { INDEX(yp, k) = D1 + xx * (D22 + xx * D33); }
	    if (ypp) { INDEX(ypp, k) = D22 + 2.0 * xx * D33; }
	}
    }

    VSCALEN(F2C(y), F2C(y), 5.0 * toc, n); /* convert to correct t/c */
    if (yp) { VSCALEN(F2C(yp), F2C(yp), 5.0 * toc, n); }
    if (ypp) { VSCALEN(F2C(ypp), F2C(ypp), 5.0 * toc, n); }
}

void
Thickness4sharpTE(fastf_t toc, struct fortran_array *x, struct fortran_array *y, struct fortran_array *yp, struct fortran_array *ypp)
{
    const fastf_t A0 = 0.2969;
    const fastf_t A1 = -0.1260;
    const fastf_t A2 = -0.3516;
    const fastf_t A22 = A2 + A2;
    const fastf_t A3 = 0.2843;
    const fastf_t A33 = A3 + A3 + A3;
    const fastf_t A4 = -0.1036;
    const fastf_t A42 = A4 + A4, A44 = A42 + A42;
    int k, n;
    fastf_t srx = 0.0, xx = 0.0;

    n = SIZE(x);
    if (yp) { INDEX(yp, 1) = 1e22; }
    if (ypp) { INDEX(ypp, 1) = 1e22; }
    for (k = 1; k <= n; k++) { /* based on t/c = 0.2 */
	xx += INDEX(x, k);
	if (ZERO(xx)) {
	    INDEX(y, k) = 0.0;
	    if (yp) { INDEX(yp, k) = 1e22; }
	    if (ypp) { INDEX(ypp, 1) = 1e22; }
	} else {
	    srx = sqrt(xx);
	    INDEX(y, k) = A0 * srx + xx * (A1 + xx * (A2 + xx * (A3 + xx * A4)));
	    if (yp) { INDEX(yp, k) = 0.5 * A0 / srx + A1 + xx * (A22 + xx * (A33 + xx * A44)); }
	    if (ypp) { INDEX(ypp, k) = -0.25 * A0 / (srx * srx * srx) + A22 + 6.0 * xx * (A3 + xx + A42); }
	}
    }
    VSCALEN(F2C(y), F2C(y), 5.0 * toc, n); /* convert to correct t/c */
    if (yp) { VSCALEN(F2C(yp), F2C(yp), 5.0 * toc, n); }
    if (ypp) { VSCALEN(F2C(ypp), F2C(ypp), 5.0 * toc, n); }
}

void
Thickness6(int family, fastf_t toc, struct fortran_array *x,
	   struct fortran_array *y, struct fortran_array *yp, struct fortran_array *ypp)
{
    const fastf_t TOL = 1e-6;
    int errCode;
    int k, n;
    fastf_t sx = 0.0;
    struct fortran_array *xt, *yt;
    struct fortran_array *minus_yt;
    struct fortran_array *xLocal, *yLocal, *sLocal, *xpLocal, *ypLocal;
    struct fortran_array *sLower, *xLower, *yLower, *xpLower, *ypLower;
    struct fortran_array *dxds, *d2xds2, *dyds, *d2yds2;

    n = SIZE(x);

    ALLOCATE(xt, 201);
    ALLOCATE(yt, 201);
    ALLOCATE(minus_yt, 201);
    ALLOCATE(xLocal, 401);
    ALLOCATE(yLocal, 401);
    ALLOCATE(sLocal, 401);
    ALLOCATE(xpLocal, 401);
    ALLOCATE(ypLocal, 401);
    ALLOCATE(dxds, n);
    ALLOCATE(d2xds2, n);
    ALLOCATE(dyds, n);
    ALLOCATE(d2yds2, n);

    if (yp) { VSETALLN(F2C(yp), 0.0, n); }
    if (ypp) { VSETALLN(F2C(ypp), 0.0, n); }

    SetSixDigitPoints(family, toc, xt, yt);
    VSCALEN(F2C(minus_yt), F2C(yt), -1, 201);
    ParametrizeAirfoil(xt, yt, xt, minus_yt, sLocal, xLocal, yLocal);

    FMMspline(sLocal, xLocal, xpLocal);
    FMMspline(sLocal, yLocal, ypLocal);

    SLICE(sLower, sLocal, 201, 401);
    SLICE(xLower, xLocal, 201, 401);
    SLICE(yLower, yLocal, 201, 401);
    SLICE(xpLower, xpLocal, 201, 401);
    SLICE(ypLower, ypLocal, 201, 401);

    for (k = 1; k <= n; k++) {
	SplineZero(sLower, xLower, xpLower, INDEX(x, k), TOL, &sx, &errCode);
	if (errCode != 0) { fprintf(stderr, "Non-zero errCode in Thickness6\n"); }
	PClookup(sLower, xLower, xpLower, sx, NULL, &INDEX(dxds, k), &INDEX(d2xds2, k), NULL);
	PClookup(sLower, yLower, ypLower, sx, &INDEX(y, k), &INDEX(dyds, k), &INDEX(d2yds2, k), NULL);
    }

    /* Is this what the corresponding lines in nacax.f90 do? */
    if (yp) {
	for (k = 1; k <= n; k++) {
	    if (ZERO(INDEX(dxds, k))) {
		INDEX(yp, k) = 0.0;
	    } else {
		INDEX(yp, k) = INDEX(dyds, k) / INDEX(dxds, k);
	    }
	}
    }

    if (ypp) {
	for (k = 1; k <= n; k++) {
	    if (ZERO(INDEX(d2xds2, k))) {
		INDEX(ypp, k) = 0.0;
	    } else {
		INDEX(ypp, k) = INDEX(d2yds2, k) / INDEX(d2xds2, k);
	    }
	}
    }
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
