/*	N U R B  _ T E S S . C
 *
 *  Function -
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

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
rt_nurb_par_edge(srf, epsilon)
CONST struct face_g_snurb * srf;
fastf_t epsilon;
{
	struct face_g_snurb * us, *vs, * uus, * vvs, *uvs;
	fastf_t d1,d2,d3;
	int i,j;
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

