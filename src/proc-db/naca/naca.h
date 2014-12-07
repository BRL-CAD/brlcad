/*                          N A C A . H
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file naca.h
 *
 * Brief description
 *
 */

#include "common.h"
#include "bu/defines.h"

/* The array in this structure should be 1-indexed */
struct fortran_array {
    int n;
    fastf_t *nums;
};
#define SIZE(array) array->n
#define INDEX(array, n) array->nums[n]
#define ALLOCATE(array, _n) do {					\
	array = (struct fortran_array *)malloc(sizeof(struct fortran_array));	\
	array->nums = (fastf_t *)malloc(sizeof(fastf_t) * (_n + 1));	\
	array->n = _n;							\
    } while (0);
#define DEALLOCATE(array) do {			\
	free(array->nums);			\
	free(array);				\
    } while (0);
#define SLICE(new, array, start, end) do {	\
	new = (struct fortran_array *)malloc(sizeof(struct fortran_array)); \
	new->n = (end - start) + 1;					\
	new->nums = array->nums + start - 1;				\
    } while (0);
#define COPY(new, array, start, end) do {	\
	VMOVEN(F2C(new), F2CI(array, start), (end - start) + 1);	\
    } while (0);

#define F2C(array) F2CI(array, 1)
#define F2CI(array, indx) (array->nums + indx)

#define C2F(array, carray, n) do {		\
	ALLOCATE(array, n);			\
	VMOVEN(array->nums + 1, carray, n);	\
    } while (0);

#define SIGNUM(x) ((x > 0) - (x < 0))
#define SIGN(a, b) (a * SIGNUM(b))

/**
 * Evaluate a cubic polynomial and its 1st, 2nd, and 3rd derivatives
 * at a specified point.  The cubic is defined by the function and the
 * 1st derivative at two points.  The cubic isi mapped onto [0,1] for
 * some savings in computation.  If no derivatives are wanted, it is
 * probably preferable to use the evaluateCubic function.
 *
 * Any of f, fp, fpp, and fppp may be null.
 *
 * @param[in] a, fa, fpa a, f(a), f'(a) at first point
 * @param[in] b, fb, fpb b, f(b), f'(b) at second point
 * @param[in] u The point to evaluate the function at.
 * @param[out] f fp fpp fppp f(u), f'(u), f''(u), f'''(u)
 */
extern void EvaluateCubicAndDerivs(fastf_t a, fastf_t fa, fastf_t fpa,
				   fastf_t b, fastf_t fb, fastf_t fpb,
				   fastf_t u,
				   fastf_t *f, fastf_t *fp, fastf_t *fpp, fastf_t *fppp);

/**
 * Compute the cubic spline with endpoint conditions chosen by FMM
 * (from the book Forsyth, Malcolm & Moler)
 */
extern void FMMspline(struct fortran_array *x, struct fortran_array *y, struct fortran_array *yp);

/**
 * Interpolate in a cubic spline at one point
 *
 * @param[in] x, y, yp Defines the cubic spline
 * @param[in] u Point where spline is to be evaluated
 * @param[out] f, fp, fpp, fppp f(u), f'(u), f''(u), f'''(u)
 */
extern void PClookup(struct fortran_array *x, struct fortran_array *y, struct fortran_array *yp,
		     fastf_t u,
		     fastf_t *f, fastf_t *fp, fastf_t *fpp, fastf_t *fppp);

/**
 * Find a value of x corresponding to a value of fbar of the cubic
 * spline defined by arrays x, f, fp.  f is the value of the spline at
 * x and fp is the first derivative.
 *
 * The spline is searched for an interval that crosses the specified
 * value.  Then Brent's method is used for finding the zero.
 */
extern void SplineZero(struct fortran_array *x, struct fortran_array *f, struct fortran_array *fp,
		       fastf_t fbar, fastf_t tol,
		       fastf_t *xbar, int *errCode);

/**
 * Use polynomial evaluation for table lookup.  Find points ahead and
 * behind evaluation point to match order of interpolatin desired.
 *
 * @param[in] x, y Data tables
 * @param[in] order Order of interpolation (1 = linear, 2 = quadratic ...)
 * @param[in] u x-coord where function is to be evaluated
 */
extern fastf_t TableLookup(struct fortran_array *x, struct fortran_array *y, int order, fastf_t u);

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
