/* 
 *	N U R B _ B A S I S . C
 *
 * nurb_basis.c - Evaluate the B-Spline Basis Functions.
 * 
 * Author:  Paul R. Stay
 * Source
 * 	SECAD/VLD Computing Consortium, Bldg 394
 * 	The US Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 * 
 * Date: Mon June 4, 1990
 * 
 * Copyright Notice - 
 * 	This software is Copyright (C) 1990-2004 by the United States Army.
 * 	All rights reserved.
 * 
 */
#ifndef lint
static const char rcs_ident[] = "$Header$";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"

/* This uses the traditional De Boor-Cox algorithm, 
 *
 *   D[k,i] (u) =
 *
 *	   U[i+n-k] - mu 		        mu - U[i-1]
 *	   ________________ D[k-1, i-1] (mu)+_________________  D[k-1,i] (mu)
 *	   U[i+n-k] - U[i-1]		     U[i+n-k] - U[i-1]
 *
 * For U[i-1]] <= mu < U[i] where U is the knot vector, k is the order,
 * and i is the interval. If the denominator is zero than the term is
 * zero.
 *
 * Arguments -
 *	knts - knot vector of type (struct knot_vector *)
 *	interval - where the parameter mu is defined above.
 *	order - Order of the b-spline.
 *	mu -  value which the basis functions are to be evaluated.
 *
 * Returns -
 *	fastf_t  - a floating point value of the basis evaluation.
 *
 * Reference -
 *	Farin G., "Curves and Surfaces for Computer Aided Geometric Design",
 *	Academic Press, New York 1988.
 */

fastf_t
rt_nurb_basis_eval( knts, interval, order, mu)
register struct knot_vector * knts;
int	interval;
int	order;
fastf_t mu;
{

	register fastf_t den;
	register fastf_t k1;
	register fastf_t k2;
	register fastf_t k3;
	register fastf_t *kk = knts->knots + interval;
	fastf_t b1, b2;

	k1 = *(kk);
	k2 = *(kk + 1);

	if (order <= 1) {
		if ( ( k1 <= mu) && (mu < k2))
			return 1.0;
		else
			return 0.0;
	}

	k3 = *(kk + order);

	den = ( *(kk + order - 1) - k1);

	if ( den == 0.0)
		b1 = 0.0;
	else
		b1 = ((mu - k1) * 
		    rt_nurb_basis_eval( knts, interval, order - 1, mu)) / den;

	den = ( k3 - k2);

	if (den == 0.0)
		b2 = 0.0;
	else
		b2 = ((k3 - mu) * 
		    rt_nurb_basis_eval( knts, interval + 1, order - 1, mu)) / den;

	return (b1 + b2);
}
