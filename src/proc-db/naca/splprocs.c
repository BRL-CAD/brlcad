/*                      S P L P R O C S . C
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
/** @file splprocs.c
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
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/defines.h"
#include "vmath.h"
#include "bn.h"

#include "naca.h"

static fastf_t Zeroin(fastf_t ax, fastf_t bx, fastf_t (*f)(fastf_t x), fastf_t tol);

/* Globals for EvaluateCubic (g prefix for global) */
fastf_t ga, gfa, gfpa;
fastf_t gb, gfb, gfpb;

/**
 * Evaluate a cubic polynomial defined by the function and the first
 * derivative at two points.
 *
 * @param[in] u The point to evaluate the function at.
 */
static fastf_t
EvaluateCubic(fastf_t u)
{
    fastf_t d = (gfb - gfa) / (gb - ga);
    fastf_t t = (u - ga) / (gb - ga);
    fastf_t p = 1.0 - t;
    return p * gfa + t * gfb - p * t * (gb - ga) * (p * (d - gfpa) - t * (d - gfpb));
}

void
EvaluateCubicAndDerivs(fastf_t a, fastf_t fa, fastf_t fpa,
                       fastf_t b, fastf_t fb, fastf_t fpb,
                       fastf_t u,
                       fastf_t *f, fastf_t *fp, fastf_t *fpp, fastf_t *fppp)
{
  /* The "magic" matrix */
  const mat_t magic = {2.0, -2.0, 1.0, 1.0,
                       -3.0, 3.0, -2.0, -1.0,
                       0.0, 0.0, 1.0, 0.0,
                       1.0, 0.0, 0.0, 0.0};

  hvect_t coef, rhs;
  fastf_t h, t;

  HSET(rhs, fa, fb, fpa * (b - a), fpb * (b - a));
  bn_matXvec(coef, magic, rhs);

  /* CAUTION - these are not the coefficients of the cubic in the
   * original coordinates.  This is the cubic on [0,1] from the
   * mapping t=(x-a)/(b-a).  That is why the h terms appear in the
   * derivatives
   */
  h = 1.0/(b - a);
  t = (u - a) * h;
  if (f) { *f = coef[3] + t * (coef[2] + t * (coef[1] + t * coef[0])); }
  if (fp) { *fp = h * (coef[2] + t * (2.0 * coef[1] + t * 3.0 * coef[0])); }
  if (fpp) { *fpp = h * h * (2.0 * coef[1] + t * 6.0 * coef[0]); }
  if (fppp) { *fppp = h * h * h * 6.0 * coef[0]; }
}


void
FMMspline(struct fortran_array *x, struct fortran_array *y, struct fortran_array *yp)
{
    int i, n;
    fastf_t deriv1, deriv2;
    struct fortran_array *dx, *dy, *delta, *dd, *alpha, *beta, *sigma, *tmp;

    /* y and yp must be at least this size */
    n = SIZE(x);
    if (n < 2) {
	INDEX(yp, 1) = 0.0;
	return;
    }
    ALLOCATE(dx, n - 1);
    ALLOCATE(dy, n - 1);
    ALLOCATE(delta, n - 1);
    VSUB2N(F2C(dx), F2CI(dx, 2), F2C(x), n - 1);
    VSUB2N(F2C(dy), F2CI(dy, 2), F2C(y), n - 1);
    for (i = 1; i <= n; i++) {
	INDEX(delta, i) = INDEX(dy, i) / INDEX(dx, i);
    }
    if (n == 2) {
	INDEX(yp, 1) = INDEX(delta, 1);
	INDEX(yp, 2) = INDEX(delta, 2);
	DEALLOCATE(dx);
	DEALLOCATE(dy);
	DEALLOCATE(delta);
	return;
    }
    ALLOCATE(dd, n - 2);
    VSUB2N(F2C(dd), F2CI(delta, 2), F2C(delta), n - 2);
    if (n == 3) {
	deriv2 = INDEX(dd, 1) / (INDEX(x, 3) - INDEX(x, 1));
	deriv1 = INDEX(delta, 1) - deriv2 * INDEX(dx, 1);
	INDEX(yp, 1) = deriv1;
	INDEX(yp, 2) = deriv1 + deriv2 * INDEX(dx, 1);
	INDEX(yp, 3) = deriv1 + deriv2 * (INDEX(x, 3) - INDEX(x, 1));
	DEALLOCATE(dx);
	DEALLOCATE(dy);
	DEALLOCATE(delta);
	DEALLOCATE(dd);
    }
    /* This gets rid of the trivial cases n = 1, 2, 3.  Assume from here on n > 3 */

    ALLOCATE(alpha, n);
    ALLOCATE(beta, n);
    ALLOCATE(sigma, n);
    ALLOCATE(tmp, n);
    INDEX(alpha, 1) = -INDEX(dx, 1);
    VADD2N(F2CI(alpha, 2), F2CI(dx, 1), F2CI(dx, 2), n - 2);
    VSCALEN(F2CI(alpha, 2), F2CI(alpha, 2), 2.0, n - 2);
    /* Serial loop, fwd elimination */
    for (i = 2; i <= n - 1; i ++) {
	INDEX(alpha, i) = INDEX(alpha, i) - INDEX(dx, i - 1) * INDEX(dx, i - 1) / INDEX(alpha, i - 1);
    }
    INDEX(alpha, n) = -INDEX(dx, n - 1) - INDEX(dx, n - 1) * INDEX(dx, n - 1) / INDEX(alpha, n - 1);

    INDEX(beta, 1) = INDEX(dd, 2) / (INDEX(x, 4) - INDEX(x, 2)) - INDEX(dd, 1) / (INDEX(x, 3) - INDEX(x, 1));
    INDEX(beta, 1) = INDEX(beta, 1) * INDEX(dx, 1) * INDEX(dx, 1) / (INDEX(x, 4) - INDEX(x, 1));
    SLICE(tmp, beta, 2, n-1);
    COPY(tmp, dd, 1, n - 2);
    INDEX(beta, n) = INDEX(dd, n - 2) / (INDEX(x, n) - INDEX(x, n - 2)) - INDEX(dd, n - 3) / (INDEX(x, n - 1 ) - INDEX(x, n - 3));
    INDEX(beta, n) = -INDEX(beta, n) * INDEX(dx, n - 1) * INDEX(dx, n - 1) / (INDEX(x, n) - INDEX(x, n - 3));
    /* Serial loop, fwd elimination */
    for (i = 2; i <= n; i++) {
	INDEX(beta, i) = INDEX(beta, i) - INDEX(dx, i - 1) * INDEX(beta, i - 1) / INDEX(alpha, i - 1);
    }

    INDEX(sigma, n) = INDEX(beta, n) / INDEX(alpha, n);
    /* Reverse order serial loop, back substitution */
    for (i = n - 1; i <= 1; i--) {
	INDEX(sigma, i) = (INDEX(beta, i) - INDEX(dx, i) * INDEX(sigma, i + 1)) / INDEX(alpha, i);
    }

    for (i = 1; i <= n - 1; i++) {
	INDEX(yp, i) = INDEX(delta, i) - INDEX(dx, i) * (INDEX(sigma, i) + INDEX(sigma, i) + INDEX(sigma, i + 1));
    }
    INDEX(yp, n) = INDEX(yp, n - 1) + INDEX(dx, n - 1) * 3.0 * (INDEX(sigma, n) + INDEX(sigma, n - 1));

    DEALLOCATE(dx);
    DEALLOCATE(dy);
    DEALLOCATE(delta);
    DEALLOCATE(alpha);
    DEALLOCATE(beta);
    DEALLOCATE(sigma);
    DEALLOCATE(dd);
    return;
}

/**
 * Compute the value of the interpolating polynomial thru x- and
 * y-arrays at the x-value of u, using Lagrange's equation.
 *
 * @param[in] x Tables of coordinates
 * @param[in] y Tables of coordinates
 * @param[in] u value of x-coordinate for interpolation
 */
static double
InterpolatePolynomial(struct fortran_array *x, struct fortran_array *y, fastf_t u)
{
    int n = SIZE(x);
    struct fortran_array *du;
    fastf_t sum = 0.0;
    int i, j;
    ALLOCATE(du, n);
    VSETALLN(F2C(du), u, n);
    VSUB2N(F2C(du), F2C(du), F2C(x), n);
    for (j = 1; j <= n; j++) {
        fastf_t fact = 1.0;
        for (i = 1; i <= n; i++) {
            if (i != j) {
                fact = fact * INDEX(du, i) / (INDEX(x, j) - INDEX(x, i));
            }
        }
        sum = sum + INDEX(y, j) * fact;
    }
    return sum;
}

/**
 * Search a sorted (increasing) array to find the interval bounding a
 * given number.  If n is the size of the array a
 *  return 0 if number x is less than a(1)
 *  return n if x > a(n)
 *  return i if a(i) <= x < a(i+1)
 *  If x is exactly equal to a(n), return n
 *
 * @param[in] xtab Input array
 * @param[in] x Input number
 */
static int
Lookup(struct fortran_array *xtab, fastf_t x)
{
    int i;
    int j, k, n;
    n = SIZE(xtab);
    if (n <= 0) {
        return -1;
    }

    if (x < INDEX(xtab, 1)) {
        return 0;
    }

    if (x > INDEX(xtab, n)) {
        return n;
    }

    i = 1;
    j = SIZE(xtab);
    while (1) {
        if (j <= i + 1) { break; }
        k = (i + j)/2; /* Integer division */
        if (x < INDEX(xtab, k)) {
            j = k;
        } else {
            i = k;
        }
    }

    return i;
}

void
PClookup(struct fortran_array *x, struct fortran_array *y, struct fortran_array *yp,
	 fastf_t u,
	 fastf_t *f, fastf_t *fp, fastf_t *fpp, fastf_t *fppp)
{
    fastf_t ud, a, fa, fpa, b, fb, fpb;
    int k;

    k = Lookup(x, u);
    V_MIN(k, SIZE(x)-1);
    V_MAX(k, 1);
    a = INDEX(x, k);
    fa = INDEX(y, k);
    fpa = INDEX(yp, k);
    b = INDEX(x, k + 1);
    fb = INDEX(y, k + 1);
    fpb = INDEX(yp, k + 1);
    ud = u;
    EvaluateCubicAndDerivs(a, fa, fpa, b, fb, fpb, ud, f, fp, fpp, fppp);
}

void
SplineZero(struct fortran_array *x, struct fortran_array *f, struct fortran_array *fp,
	   fastf_t fbar, fastf_t tol,
	   fastf_t *xbar, int *errCode)
{
    int k, n;
    struct fortran_array *fLocal;

    n = SIZE(x);

    /* Look for an exact match.  Could happen... */
    for (k = 1; k <= n; k++) {
	if (fabs(INDEX(f, k) - fbar) < tol) {
	    *xbar = INDEX(x, k);
	    *errCode = 0;
	    return;
	}
    }

    ALLOCATE(fLocal, n);
    VSETALLN(F2C(fLocal), fbar, n);
    VSUB2N(F2C(fLocal), F2C(f), F2C(fLocal), n);
    /* Look for a zero of fLocal */

    for (k = 2; k <= n; k++) {
	if (INDEX(fLocal, k - 1) * INDEX(fLocal, k) < 0.0) break;
    }
    if (k == n + 1) { /* No crossing could be found */
	*errCode = 1;
	DEALLOCATE(fLocal);
	return;
    }

    *errCode = 0;
    /* Set the global variables for EvaluateCubic */
    ga = INDEX(x, k - 1);
    gfa = INDEX(fLocal, k - 1);
    gfpa = INDEX(fp, k - 1);
    gb = INDEX(x, k);
    gfb = INDEX(fLocal, k);
    gfpb = INDEX(fp, k);
    DEALLOCATE(fLocal);
    *xbar = Zeroin(ga, gb, EvaluateCubic, tol);
}

fastf_t
TableLookup(struct fortran_array *x, struct fortran_array *y, int order, fastf_t u)
{
    int j, m;
    /* Used for the parameters to InterpolatePolynomial */
    struct fortran_array *xp, *yp;

    m = order + 1;
    V_MIN(m, SIZE(x)); /* number of points used for interpolating poly */
    j = Lookup(x, u);
    j = j - (m / 2 - 1);
    V_MIN(j, 1 + SIZE(x) - m); /* j + m - 1 must not exceed SIZE(x) */
    V_MIN(j, 1); /* j must be positive */
    /* Use points j through j + m - 1 for interpolation (m points) */
    SLICE(xp, x, j, j + m - 1);
    SLICE(yp, y, j, j + m - 1);
    return InterpolatePolynomial(xp, yp, u);
}

/**
 * Compute a zero of f in the interval (ax, bx)
 *
 * ax and bx are the lift and right endpoints of the interval, tol is
 * the desired interval of uncertainty.
 *
 * @param[in] ax left endpoint of interval
 * @param[in] bx right endpoint of interval
 * @param[in] tol desired interval of uncertainty
 */
static fastf_t
Zeroin(fastf_t ax, fastf_t bx, fastf_t (*f)(fastf_t x), fastf_t tol)
{
    const int MAX_ITER = 500;
    int k;
    fastf_t a, b, c, eps, fa, fb, fc, tol1, xm, p, r, s, tmp;

    fastf_t d = 0.0;
    fastf_t e = 0.0;
    fastf_t q = 0.0;

    eps = SMALL_FASTF;
    a = ax; /* initialization */
    b = bx;
    fa = f(a);
    fb = f(b); /* should test that fa and fb have opposite signs */
    c = b;
    fc = fb;
    e = b - a;

    for (k = 1; k <= MAX_ITER; k++) { /* begin iteration */
	if (((fb > 0.0) && (fc > 0.0)) || ((fb < 0.0 && fc < 0.0))) {
	    c = a;
	    fc = fa;
	    d = b - a;
	    e = d;
	}

	if (fabs(fc) < fabs(fb)) {
	    a = b;
	    b = c;
	    c = a;
	    fa = fb;
	    fb = fc;
	    fc = fa;
	}

	tol1 = 2.0 * eps * fabs(b) + 0.5 * tol;	/* convergence test */
	xm = 0.5 * (c - b);
	if ((fabs(xm) <= tol1) || (NEAR_ZERO(fb, tol))) {
	    return b;
	}

	/* Is bisection necessary */
	if ((fabs(e) <= tol1) && (fabs(fa) > fabs(fb))) {
	    s = fb / fa; /* is quadratic interpolation possible? */
	    if (NEAR_EQUAL(a, c, tol)) {
		s = fb / fa; /* Use linear interpolation */
		p = 2.0 * xm * s;
		q = 1.0 - s;
	    } else {
		q = fa / fc; /* Use inverse quadratic interpolation */
		r = fb / fc;
		s = fb / fa;
		p = s * (2.0 * xm * q * (q - r) - (b - a) * (r - 1.0));
		q = (q - 1.0) * (r - 1.0) * (s - 1.0);
	    }

	    if (p > 0.0) { q = -q; } /* adjust signs */
	    p = fabs(p);

	    tmp = 3.0 * xm * q - fabs(tol1 * q);
	    V_MIN(tmp, fabs(e * q));
	    if (p + p < tmp) {
		e = d; /* Use interpolation */
		d = p / q;
	    } else {
		d = xm; /* Use bisection */
		e = d;
	    }
	} else {
	    d = xm; /* Use bisection */
	    e = d;
	}

	a = b;
	fa = fb;
	if (fabs(d) > tol1) {
	    b = b + d;
	} else {
	    b = b + SIGN(tol1, xm);
	}
	fb = f(b);
    }

    return b; /* but this is a bad return.  Max iterations exceeded */
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
