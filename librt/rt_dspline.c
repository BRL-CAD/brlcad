/*	R T _ S P L I N E . C
 *
 *  Simple data (double) spline package.
 *
 *  rt_dspline_matrix(m, type, tension, bias)	create basis matrix
 *  rt_dspline4(m, a, b, c, d, alpha)		interpolate 1 value
 *  rt_dspline4v(m, a, b, c, d, depth alpha)	interpolate vectors
 *  rt_dspline(r, m, knots, n, depth, alpha)	interpolate n knots over 0..1
 *
 *  Example:
 *	mat_t	m;
 *	double	d;
 *	vect_t	v;
 *	vect_t  kn = { 	{0., 0., 0.},
 *		 	{0., 1., 0.},
 * 			{.5, 1., 0.},
 *		 	{1., 1., 0.},
 *		 	{1., 0., 0.} };
 *
 *	rt_dspline_matrix(m, "Beta", 0.5, 1.0);
 *
 *	d = rt_dspline4(m, .0, .0, 1.0, 1.0, 0.25);
 *
 *	for (p = 0.0 ; p <= 1.0 ; p += 0.0625 ) {
 *		rt_dspline(v, m, kn, 5, 3, p);
 *		bu_log("%g (%g %g %g)\n", p, V3ARGS(v));
 *	}
 *
 *  Author -
 *	Lee A. Butler
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "bu.h"
#include "raytrace.h"

static void
GetBeta(m, bias, tension)
mat_t m;
const double bias, tension;
{
	register int i;
	double d, b2, b3;
	register double tmp;

	b2 = bias * bias;
	b3 = bias * b2;

	tmp = 2.0 * b3;
	m[12] = tmp;
	m[ 0] = -tmp;

	tmp = tension+b2+bias;
	m[ 1] = 2.0 * (tmp + b3);
	m[ 2] = -2.0 * (tmp + 1.0);

	tmp = tension + 2.0 * b2;
	m[ 5] = -3.0 * (tmp + 2.0 * b3);
	m[ 6] =  3.0 * tmp;

	tmp = 6.0 * b3;
	m[ 4] = tmp;
	m[ 8] = -tmp;

	m[ 9] = 6.0 * (b3 - bias);
	m[10] = 6.0 * bias;

	tmp = tension + 4.0 * (b2 + bias);
	m[13] = tmp;
	d = 1.0 / ( tmp + 2.0 * b3 + 2.0);

	m[ 3] = m[14] = 2.0;
	m[ 7] = m[11] = m[15] = 0.0;

	for (i=0 ; i < 16; i++) m[i] *= d;
}

static void
GetCardinal(m, tension)
mat_t m;
const double tension;
{
	m[ 1] = 2.0 - tension;
	m[ 2] = tension - 2.0;
	m[ 4] = 2.0 * tension;
	m[ 5] = tension - 3.0;
	m[ 6] = 3.0 - 2.0 * tension;
	m[13] = 1.0;
	m[ 3] = m[10] = tension;
	m[ 0] = m[7] = m[ 8] = -tension;
	m[ 9] = m[11] = m[12] = m[14] = m[15] = 0.0;
}

/*	R T _ D S P L I N E _ M A T R I X 
 *
 *	Initialize a spline matrix for a particular spline type.
 *
 */
void
rt_dspline_matrix(m, type, tension, bias)
      mat_t	m;
const char	*type;		/* "Cardinal", "Catmull", "Beta" */
const double	tension;	/* Cardinal tension of .5 is Catmull spline */
const double	bias;		/* only for B spline */
{
	if (!strncmp(type, "Cardinal", 8))	GetCardinal(m, tension);
	else if (!strncmp(type, "Catmull", 7))	GetCardinal(m, 0.5);
	else if (!strncmp(type, "Beta", 4)) 	GetBeta(m, bias, tension);
	else {
		bu_log( "Error: %s:%d spline type \"%s\" Unknown\n", 
			__FILE__, __LINE__, type);
		abort();
	}
}

/*	R T _ D S P L I N E 4
 *
 *	m		spline matrix (see rt_dspline4_matrix)
 *	a, b, c, d	knot values
 *	alpha:	0..1	interpolation point
 *
 *	Evaluate a 1-dimensional spline at a point "alpha" on knot values
 *	a, b, c, d.
 */
double
rt_dspline4(m, a, b, c, d, alpha)
mat_t	m;		/* spline matrix */
double a, b, c, d;	/* control pts */
double alpha;		/* point to interpolate at */
{
	double p0, p1, p2, p3;

	p0 = m[ 0]*a + m[ 1]*b + m[ 2]*c + m[ 3]*d;
	p1 = m[ 4]*a + m[ 5]*b + m[ 6]*c + m[ 7]*d;
	p2 = m[ 8]*a + m[ 9]*b + m[10]*c + m[11]*d;
	p3 = m[12]*a + m[13]*b + m[14]*c + m[15]*d;

	return  p3 +  alpha*(p2 + alpha*(p1 + alpha*p0) );
}

/*	R T _ D S P L I N E 4 V
 *
 *	pt		vector to recieve the interpolation result
 *	m		spline matrix (see rt_dspline4_matrix)
 *	a, b, c, d	knot values
 *	alpha:	0..1	interpolation point
 *
 *  Evaluate a spline at a point "alpha" between knot pts b & c
 *  The knots and result are all vectors with "depth" values (length).
 *
 */
void
rt_dspline4v(pt, m, a, b, c, d, depth, alpha)
double *pt;	/* result */
const mat_t	m;	/* spline matrix obtained with spline_matrix() */
const double *a;	/* knots */
const double *b;
const double *c;
const double *d;
const int depth;	/* number of values per knot */
const double alpha;	/* 0 <= alpha <= 1 */
{
	int i;
	double p0, p1, p2, p3;

	for (i=0 ; i < depth ; i++) {
		p0 = m[ 0]*a[i] + m[ 1]*b[i] + m[ 2]*c[i] + m[ 3]*d[i];
		p1 = m[ 4]*a[i] + m[ 5]*b[i] + m[ 6]*c[i] + m[ 7]*d[i];
		p2 = m[ 8]*a[i] + m[ 9]*b[i] + m[10]*c[i] + m[11]*d[i];
		p3 = m[12]*a[i] + m[13]*b[i] + m[14]*c[i] + m[15]*d[i];

		pt[i] = p3 +  alpha*(p2 + alpha*(p1 + alpha*p0) );
	}
}


/*	R T _ D S P L I N E _ N
 *
 *	Interpolate n knot vectors over the range 0..1
 *
 *	"knots" is an array of "n" knot vectors.  Each vector consists of
 *	"depth" values.  They define an "n" dimensional surface which is
 *	evaluated at the single point "alpha".  The evaluated point is
 *	returned in "r"
 *
 *	Example use:
 *		double result[MAX_DEPTH], knots[MAX_DEPTH*MAX_KNOTS];
 *		mat_t	m;
 *		int	knot_count, depth;
 *
 *		knots = bu_malloc(sizeof(double) * knot_length * knot_count);
 *		result = bu_malloc(sizeof(double) * knot_length);
 *
 *		rt_dspline4_matrix(m, "Catmull", (double *)NULL, 0.0);
 *
 *		for (i=0 ; i < knot_count ; i++)
 *			get a knot(knots, i, knot_length);
 *
 *		rt_dspline_n(result, m, knots, knot_count, knot_length, alpha);
 *
 */
void
rt_dspline_n(r, m, knots, nknots, depth, alpha)
      double	*r;	/* result */
const mat_t	m;	/* spline matrix */
const double	*knots;	/* knot values */
const int	nknots;	/* number of knots */
const int	depth;	/* number of values per knot */
const double	alpha;	/* point on surface (0..1) to evaluate */
{
	double *a, *b, *c, *d, x;
	int nspans = nknots - 3;
	int span;

	/* validate args */
	if (nspans < 1 || depth < 1 || alpha < 0.0 || alpha > 1.0 ||
	    !r || !knots)
		abort();


	/* compute which knots (span) we're going to interpolate */


	x = alpha * nspans;
	span = (int)x;
	if (span >= nspans) span = nspans - 1;
	x -= span;

	/* compute point (alpha 0..1) within this span */

	a = (double *)&knots[span*depth];
	b = a+depth;
	c = b+depth;
	d = c+depth;

	rt_dspline4v(r, m, a, b, c, d, depth, x);

}
