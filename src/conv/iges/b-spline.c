/*                      B - S P L I N E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file iges/b-spline.c
 *
 * These functions evaluate a Rational B-Spline Curve
 *
 */

#include "common.h"

#include <stdio.h>

#include "vmath.h"
#include "raytrace.h"		/* for declaration of bu_calloc() */


static fastf_t *knots=(fastf_t *)NULL;
static int numknots=0;


/* Set the knot values */
void
Knot(int n /* number of values in knot sequence */, fastf_t values[] /* knot values */)
{
    int i;

    if (n < 2) {
	bu_log("Knot: ERROR %d knot values\n", n);
	bu_exit(1, "Knot: cannot have less than 2 knot values\n");
    }

    if (numknots)
	bu_free((char *)knots, "Knot: knots");

    knots = (fastf_t *)bu_calloc(n, sizeof(fastf_t), "Knot: knots");

    numknots = n;

    for (i=0; i<n; i++)
	knots[i] = values[i];

}


void
Freeknots(void)
{
    bu_free((char *)knots, "Freeknots: knots");
    numknots = 0;
}


/* Evaluate the Basis functions */
fastf_t
Basis(int i /* interval number (0 through k) */, int k /* degree of basis function */, fastf_t t /* parameter value */)
{
    fastf_t denom1, denom2, retval=0.0;

    if ((i+1) > (numknots-1)) {
	bu_log("Error in evaluation of a B-spline Curve\n");
	bu_log("attempt to access knots out of range: numknots=%d i=%d, k=%d\n", numknots, i, k);
	return 0.0;
    }

    if (k == 1) {
	if (t >= knots[i] && t < knots[i+1])
	    return 1.0;
	else
	    return 0.0;
    } else {
	denom1 = knots[i+k-1] - knots[i];
	denom2 = knots[i+k] - knots[i+1];

	if (!ZERO(denom1))
	    retval += (t - knots[i])*Basis(i, k-1, t)/denom1;

	if (!ZERO(denom2))
	    retval += (knots[i+k] - t)*Basis(i+1, k-1, t)/denom2;

	return retval;
    }
}


/* Evaluate a B-Spline curve */
void
B_spline(fastf_t t /* parameter value */,
	 int m /* upper limit of sum (number of control points - 1) */,
	 int k /* order */,
	 point_t P[] /* Control Points (x, y, z) */,
	 fastf_t weights[] /* Weights for Control Points */,
	 point_t pt /* Evaluated point on spline */)
{

    fastf_t tmp, numer[3], denom=0.0;
    int i, j;

    for (j=0; j<3; j++)
	numer[j] = 0.0;

    for (i=0; i<=m; i++) {
	tmp = weights[i]*Basis(i, k, t);
	denom += tmp;
	for (j=0; j<3; j++)
	    numer[j] += P[i][j]*tmp;
    }

    for (j=0; j<3; j++)
	pt[j] = numer[j]/denom;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
