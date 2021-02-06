/*                          N A C A . H
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
/** @file naca.h
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

#include "vmath.h"


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
 * @param[in] a     a at first point
 * @param[in] fa    f(a) at first point
 * @param[in] fpa   f'(a) at first point
 * @param[in] b     b at second point
 * @param[in] fb    f(b) at second point
 * @param[in] fpb   f'(b) at second point
 * @param[in] u     The point to evaluate the function at.
 * @param[out] f    f(u)
 * @param[out] fp   f'(u), f''(u), f'''(u)
 * @param[out] fpp  f''(u), f'''(u)
 * @param[out] fppp f'''(u)
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
 * @param[in] x     Defines the cubic spline
 * @param[in] y     Defines the cubic spline
 * @param[in] yp    Defines the cubic spline
 * @param[in] u     Point where spline is to be evaluated
 * @param[out] f    f(u)
 * @param[out] fp   f'(u), f''(u), f'''(u)
 * @param[out] fpp  f''(u), f'''(u)
 * @param[out] fppp f'''(u)
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
 * behind evaluation point to match order of interpolation desired.
 *
 * @param[in] x Data tables
 * @param[in] y Data tables
 * @param[in] order Order of interpolation (1 = linear, 2 = quadratic ...)
 * @param[in] u x-coord where function is to be evaluated
 */
extern fastf_t TableLookup(struct fortran_array *x, struct fortran_array *y, int order, fastf_t u);



/**
 * Add the computed thickness and camber to get a cambered airfoil.
 *
 * NOTE - The dimension of xupper, ... must not be less than that of x.
 *
 * Note on param thick: note this the half-thickness
 */
void CombineThicknessAndCamber(struct fortran_array *x, struct fortran_array *thick, struct fortran_array *y, struct fortran_array *yp,
			       struct fortran_array *xupper, struct fortran_array *yupper, struct fortran_array *xlower, struct fortran_array *ylower);

/**
 * Compute the r and k1 factors used in the calculation of ordinates
 * of a three-digit mean line.
 *
 * REF: Table, upper left of p.8 of NASA Technical Memorandum 4741
 *
 * @param[in] x x-coor of max. camber ( in [0,0.25] )
 * @param[out] r factor used by MeanLine3
 * @param[out] k1 factor used by MeanLine3
 */
void GetRk1(fastf_t x, fastf_t *r, fastf_t *k1);

/**
 * Compute the r, k1, and k2 factors used in the calculation of
 * ordinates of a three-digit-reflex mean line
 *
 * REF - Table, upper right, of p.8 of NASA Technical Memorandum 4741.
 *
 * NOTE - This is really the correct equation for k2.  It was printed
 * incorrectly on p. 9 of NASA TM X-3284 in 1976 and then changed
 * (incorrectly!!) on p.8 of NASA TM 4741 in 1995.  Eastman Jacobs had
 * it right the first time on p.522 of NACA Report 537 in 1935.
 *
 * @param[in] x x-coor of maximum camber ( in [0,0.25] )
 * @param[out] r factor used by MeanLine3R
 * @param[out] k1 factor used by MeanLine3R
 * @param[out] k2 factor used by MeanLine3R
 */
void GetRk1k2(fastf_t x, fastf_t *r, fastf_t *k1, fastf_t *k2);

/**
 * Compute the upper and lower coordinates of an airfoil defined by the
 * thickness (x,yt) and the mean line (x,ymean,ymeanp)
 */
void InterpolateCombinedAirfoil(struct fortran_array *x, struct fortran_array *yt, struct fortran_array *ymean, struct fortran_array *ymeanp,
				struct fortran_array *yu, struct fortran_array *yl,
				struct fortran_array *yup, struct fortran_array *ylp);

/**
 * Using the airfoil defined by (xupper, yupper) for upper surface and
 * (xlower, ylower) for the lower surface, interpolate at each point
 * of array x to yield yu and yup on upper surface and yl and ylp on
 * lower.
 *
 * NOTE - xupper and xlower do not need to be the same size.
 */
void InterpolateUpperAndLower(struct fortran_array *xupper, struct fortran_array *yupper,
			      struct fortran_array *xlower, struct fortran_array *ylower,
			      struct fortran_array *x,
			      struct fortran_array *yu, struct fortran_array *yl,
			      struct fortran_array *yup, struct fortran_array *ylp);

/**
 * Compute the leading edge radius of a 4-digit thickness
 * distribution
 *
 * @param[in] toc maximum value of t/c
 */
fastf_t LeadingEdgeRadius4(fastf_t toc);

/**
 * Compute the leading edge radius of a 4-digit-modified thickness
 * distribution from the thickness and the le index.
 *
 * NOTE - leIndex is coded as fastf_t, although it is usually integer.
 *
 * @param[in] toc maximum value of t/c
 * @param[in] leIndex leading edge index
 */
fastf_t LeadingEdgeRadius4M(fastf_t toc, fastf_t leIndex);

/**
 * Compute the leading edge radius of a 6- or 6A-series thickness
 * distribution.
 *
 * @param[in] family family
 * @param[in] toc maximum value of t/c
 */
fastf_t LeadingEdgeRadius6(fastf_t family, fastf_t toc);

/**
 * Calculates a table of x-values to be used for the output.  They are
 * very dense at the nose, less dense on the forward airfoil and much
 * less dense on the aft airfoil.
 */
void LoadX(int denCode, int nx, struct fortran_array *x);

/**
 * Compute displacement and slope of the NACA 2-digit mean line.
 *
 * NOTE - E.g., a NACA Mean Line 64 has cmax = 0.06 and xmax = 0.4.
 *
 * REF - Eq 6.4, p.114 of Abbot and von Doenhoff
 *
 * @param[in] cmax max. camber as fraction of chord
 * @param[in] xmaxc fraction of chord where the camber is max.
 * @param[in] x x
 * @param[out] ym ym
 * @param[out] ymp ymp
 */
void MeanLine2(fastf_t cmax,
	       fastf_t xmaxc,
	       struct fortran_array *x,
	       struct fortran_array *ym, struct fortran_array *ymp);

/**
 * Compute displacement and slope of the NACA 3-digit mean line.
 * EXAMPLE - A 210 mean line has design CL=0.3 and max camber at 5 percent
 * (first digit is 2/3 CL in tenths; second digit is twice x of max
 * camber in percent chord; third digit, zero, indicates non reflexed)
 *
 * REF - Eq. 6.6 p.115 of Abbot and von Doenhoff
 *
 * NOTE - The constant 1.8
 *
 * @param[in] cl design lift coefficient
 * @param[in] xmaxc x-coor of maximum camber
 * @param[in] x x
 * @param[out] ym  ym
 * @param[out] ymp ymp
 */
void MeanLine3(fastf_t cl,
	       fastf_t xmaxc,
	       struct fortran_array *x,
	       struct fortran_array *ym, struct fortran_array *ymp);

/**
 * Compute displacement and slope of the NACA 3-digit reflex mean line
 *
 * NOTE - The constant 1.8 appears in the final calculation.  This
 * comes from the fact that the basic equation is given for cl = 0.3
 * and that there is a 6 in the denominator, so one finally multiplies
 * by (cl/0.3)/6
 *
 * REF - p.8 of NASA Technical Memorandum 474 and NACA Report 537.
 *
 * @param[in] cl design lift coefficient
 * @param[in] xmaxc x-coor of maximum camber
 * @param[in] x x
 * @param[out] ym ym
 * @param[out] ymp ymp
 */
void MeanLine3Reflex(fastf_t cl,
		     fastf_t xmaxc,
		     struct fortran_array *x,
		     struct fortran_array *ym, struct fortran_array *ymp);

/**
 * Compute the displacement and slope of a 6-series mean line.
 *
 * REF - EQ. 4-26 and 4-27, p.74 of Abbott and von Doenhoff.
 *
 * @param[in] a chordwise extent of uniform loading 0 <= a <= 1
 * @param[in] cl design CL of the mean line
 * @param[out] x fraction of chord
 * @param[out] ym y-coord of meanline
 * @param[out] ymp dy/dx of mean line
 */
void MeanLine6(fastf_t a, fastf_t cl, struct fortran_array *x, struct fortran_array *ym, struct fortran_array *ymp);

/**
 * Compute the displacement and slope of a modified six-series mean
 * line as designed for use with the 6A-series profiles
 *
 * REF - p.210 of NACA Report 903 (Larry Loftin).
 *
 * NOTE - a is not an input variable; it is always 0.8 for this mean line.
 *
 * @param[in] cl the design lift coefficient
 * @param[in] x fraction of chord
 * @param[out] ym y-coor of meanline
 * @param[out] ymp dy/dx of mean line
 */
void MeanLine6M(fastf_t cl, struct fortran_array *x, struct fortran_array *ym, struct fortran_array *ymp);

/**
 * Define the shape of an airfoil parametrically using s, the
 * inscribed arc length, s.  s = 0 at the trailing edge, then
 * increases by moving forward along the upper surface, then around
 * the nose and rearward along the lower surface.  The final value of
 * s is somewhat greater than 2.
 */
void ParametrizeAirfoil(struct fortran_array *xupper, struct fortran_array *yupper, struct fortran_array *xlower, struct fortran_array *ylower,
			struct fortran_array *s, struct fortran_array *x, struct fortran_array *y);

/**
 * Compute the scale factor used to multiply the epsilon and psi
 * functions to get the proper thickness/chord for the mapping of a
 * six-series airfoil (or 6A).
 *
 * REF - Discussed in AIAA-2001-5235
 *
 * @param[in] family  1 = 63, 2 = 64, 3 = 65, 4 = 66, 5 = 67,
 *                    6 = 63A, 7 = 64A, 8 = 65A
 * @param[in] tc      desired t/c (fraction, not percent)
 */
fastf_t ScaleFactor(int family, fastf_t tc);

/**
 * Set the data points that define a 6-series or 6A-series thickness
 * distribution.  User has no control of the spacing of xt - it
 * depends on phi, eps, psi
 *
 * @param[in] family 1 = 63, 2 = 64, 3 = 65, 4 = 66, 5 = 67,
 *		     6 = 63A, 7 = 64A, 8 = 65A
 * @param[in] tc max value of t / c
 * @param[out] xt xt
 * @param[out] yt yt
 */
void SetSixDigitPoints(int family, fastf_t tc, struct fortran_array *xt, struct fortran_array *yt);

/**
 * Compute the values of y, dy/dx, d2y/dx2 at each point of x.
 */
void Thickness4(fastf_t toc, struct fortran_array *x, struct fortran_array *y, struct fortran_array *yp, struct fortran_array *ypp);

/**
 * Compute the thickness distribution (and derivatives) of a NACA
 * 4-digit-modified section.
 *
 * NOTE - First digit after dash is l.e. index; 2nd is loc of max thickness
 *
 * @param[in] toc toc
 * @param[in] leIndex leading edge index
 * @param[in] xmaxt   x-coor of maximum thickness (fraction of chord)
 * @param[in] x x
 * @param[out] y y
 * @param[out] yp yp
 * @param[out] ypp ypp
 */
void Thickness4M(fastf_t toc, fastf_t leIndex, fastf_t xmaxt,
		 struct fortran_array *x, struct fortran_array *y, struct fortran_array *yp, struct fortran_array *ypp);

/**
 * Compute the values of y, dy/dx, d2y/d2x at each point of x.  The
 * equation used gives an airfoil that is very similar to the
 * four-digit airfoil profile, but has a sharp trailing edge.  This
 * airfoil does not have any NACA blessing, so it is strictly
 * unofficial.  Lots of folks want to compute a four-digit section,
 * but are using a theory that assumes a zero trailing edge.
 */
void Thickness4sharpTE(fastf_t toc, struct fortran_array *x, struct fortran_array *y, struct fortran_array *yp, struct fortran_array *ypp);

/**
 * Compute the values of y, dy/dx, d2y/dx2 at each poitn of x by
 * interpolation in the tables computed by xxxx.
 */
void Thickness6(int family, fastf_t toc, struct fortran_array *x,
		struct fortran_array *y, struct fortran_array *yp, struct fortran_array *ypp);



extern const fastf_t EPS1[];
extern const fastf_t EPS2[];
extern const fastf_t EPS3[];
extern const fastf_t EPS4[];
extern const fastf_t EPS5[];
extern const fastf_t EPS6[];
extern const fastf_t EPS7[];
extern const fastf_t EPS8[];
extern const fastf_t PSI1[];
extern const fastf_t PSI2[];
extern const fastf_t PSI3[];
extern const fastf_t PSI4[];
extern const fastf_t PSI5[];
extern const fastf_t PSI6[];
extern const fastf_t PSI7[];
extern const fastf_t PSI8[];

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
