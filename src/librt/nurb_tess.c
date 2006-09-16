/*                     N U R B _ T E S S . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
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

/** @addtogroup nurb */
/*@{*/
/** @file nurb_tess.c
 *	Given Epsilon, compute the number of internal knots to
 *	add so that every triangle generated in parametric space
 *	is within epsilon of the original surface.
 *
 *  Author -
 *	Paul Randal Stay
 *
 *  Source -
 * 	SECAD/VLD Computing Consortium, Bldg 394
 *	The U.S. Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 *
 */
/*@}*/

#include "common.h"



#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"

/* Algorithm -
 *
 * See paper in Computer Aided Design (CAD) Volumne 27, Number 1, January 1995
 *	TESSELATING TRIMMMED NURBS SURFACES, Leslie A Piegl and Arnaud Richard.
 *
 * There is a slight deviation from the paper, Since libnurb (rt_nurb_s_diff)
 * differentiation correctly handles rational surfaces, no special processing for
 * rational is needed.
 *
 * The idea is to compute the longest edge size in parametric space such that
 * a the edge (or curve) in real space is within epsilon tolerance. The mapping
 * from parametric space is done as a separate step.
 *
 */

fastf_t
rt_nurb_par_edge(const struct face_g_snurb *srf, fastf_t epsilon)
{
	struct face_g_snurb * us, *vs, * uus, * vvs, *uvs;
	fastf_t d1,d2,d3;
	int i;
	fastf_t *pt;


	us = rt_nurb_s_diff(srf, RT_NURB_SPLIT_ROW);
	vs = rt_nurb_s_diff(srf, RT_NURB_SPLIT_COL);
	uus = rt_nurb_s_diff(us, RT_NURB_SPLIT_ROW);
	vvs = rt_nurb_s_diff(vs, RT_NURB_SPLIT_COL);
	uvs = rt_nurb_s_diff(vs, RT_NURB_SPLIT_ROW);

	d1 = 0.0;
	d2 = 0.0;
	d3 = 0.0;

	pt = (fastf_t *) uus->ctl_points;

	/* Find the maximum value of the 2nd derivative in U */

	for( i = 0; i < uus->s_size[0] * uus->s_size[1]; i++)
	{
		fastf_t mag;

		mag = MAGNITUDE( pt );

		if ( mag > d1) d1 = mag;

		pt += RT_NURB_EXTRACT_COORDS(uus->pt_type);

	}

	/* Find the maximum value of the partial derivative in UV */

	pt = (fastf_t *) uvs->ctl_points;

	for( i = 0; i < uvs->s_size[0] * uvs->s_size[1]; i++)
	{
		fastf_t mag;

		mag = MAGNITUDE( pt );

		if ( mag > d2) d2 = mag;

		pt += RT_NURB_EXTRACT_COORDS(uvs->pt_type);

	}


	/* Find the maximum value of the 2nd derivative in V */
	pt = (fastf_t *) vvs->ctl_points;

	for( i = 0; i < vvs->s_size[0] * vvs->s_size[1]; i++)
	{
		fastf_t mag;

		mag = MAGNITUDE( pt );

		if ( mag > d3) d3 = mag;

		pt += RT_NURB_EXTRACT_COORDS(vvs->pt_type);

	}

	/* free up storage */

        rt_nurb_free_snurb( us, (struct resource *)NULL);
        rt_nurb_free_snurb( vs, (struct resource *)NULL);
        rt_nurb_free_snurb( uus, (struct resource *)NULL);
        rt_nurb_free_snurb( vvs, (struct resource *)NULL);
        rt_nurb_free_snurb( uvs, (struct resource *)NULL);


	/* The paper uses the following to calculate the longest edge size
  	 *			  	  1/2
	 *  3.0 * (			)
	 *	  (	   2.0		)
	 *	  _________________________
	 *	  (2.0 * (d1 + 2 D2 + d3)
 	 */

	return ( 3.0 * sqrt( epsilon / (2.0*(d1 + (2.0 * d2)+ d3))));
}

/*
 *		R T _ C N U R B _ P A R _ E D G E
 *
 *	Calculate the maximum edge length (in parameter space)
 *	that will keep the curve approximation within epsilon
 *	of the true curve
 *
 *	This is a temporary guess until legitimate code can be found
 *
 *	returns:
 *		-1.0 if the curve is a straight line
 *		maximum parameter increment otherwise
 */
fastf_t
rt_cnurb_par_edge(const struct edge_g_cnurb *crv, fastf_t epsilon)
{
	struct edge_g_cnurb *d1, *d2;
	fastf_t der2[5], t, *pt;
	fastf_t num_coord_factor, final_t;
	int num_coords;
	int i,j;

	if( crv->order < 3)
		return( -1.0 );

	num_coords = RT_NURB_EXTRACT_COORDS( crv->pt_type );
	if( num_coords > 5 )
	{
		bu_log( "ERROR: rt_cnurb_par_edge() cannot handle curves with more than 5 coordinates (curve has %d)\n",
			num_coords );
		bu_bomb( "ERROR: rt_cnurb_par_edge() cannot handle curves with more than 5 coordinates\n" );
	}

	for( i=0 ; i<num_coords ; i++ )
	{
		der2[i] = 0.0;
	}

	final_t = MAX_FASTF;
	num_coord_factor = sqrt( (double)num_coords );

	d1 = rt_nurb_c_diff( crv );
	d2 = rt_nurb_c_diff( d1 );

#if 0
	pt = d1->ctl_points;
	for( i=0 ; i<d1->c_size ; i++ )
	{
		for( j=0 ; j<num_coords ; j++ )
		{
			fastf_t abs_val;

			abs_val = *pt > 0.0 ? *pt : -(*pt);
			if( abs_val > der1[j] )
				der1[j] = abs_val;
			pt++;
		}
	}
#endif
	pt = d2->ctl_points;
	for( i=0 ; i<d2->c_size ; i++ )
	{
		for( j=0 ; j<num_coords ; j++ )
		{
			fastf_t abs_val;

			abs_val = *pt > 0.0 ? *pt : -(*pt);
			if( abs_val > der2[j] )
				der2[j] = abs_val;
			pt++;
		}
	}

	rt_nurb_free_cnurb( d1 );
	rt_nurb_free_cnurb( d2 );

	for( j=0 ; j<num_coords ; j++ )
	{
		if( NEAR_ZERO( der2[j], SMALL_FASTF ) )
			continue;

		t = sqrt( 2.0 * epsilon / (num_coord_factor * der2[j] ) );
		if( t < final_t )
			final_t = t;
	}

	if( final_t == MAX_FASTF )
		return( -1.0 );
	else
		return( final_t/2.0 );
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
