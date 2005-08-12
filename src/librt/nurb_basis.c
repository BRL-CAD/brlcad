/*                    N U R B _ B A S I S . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \defgroup nurb nurb_ 
 * \ingroup librt
 */

/*@{*/
/** @file nurb_basis.c
 * Evaluate the B-Spline Basis Functions.
 *
 */
/*@}*/

/* 
 *	N U R B _ B A S I S . C
 *
 * nurb_basis.c - Evaluate the B-Spline Basis Functions.
 *
 * 
 * Author:  Paul R. Stay
 * Source
 * 	SECAD/VLD Computing Consortium, Bldg 394
 * 	The US Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 * 
 * Date: Mon June 4, 1990
 */
#ifndef lint
static const char rcs_ident[] = "$Header$";
#endif

#include "common.h"



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
rt_nurb_basis_eval(register struct knot_vector *knts, int interval, int order, fastf_t mu)
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
