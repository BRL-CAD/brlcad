/* 
 *       N U R B _ F L A T . C
 *
 * Function -
 *     Tests the NURB surface to see if its flat depending
 *     on the epsilon passed.
 * 
 * Author -
 *     Paul R. Stay
 *
 * Source -
 *     SECAD/VLD Computing Consortium, Bldg 394
 *     The U.S. Army Ballistic Research Laboratory
 *     Aberdeen Proving Ground, Maryland 21005
 *
 * Copyright Notice -
 *     This software is Copyright (C) 1990 by the United States Army.
 *     All rights reserved.
 */

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "nurb.h"

int
rt_nurb_s_flat( srf, epsilon )
struct snurb *srf;
fastf_t epsilon;		/* Epsilon value for flatness testing */
{
	register fastf_t 	max_row_dist;
	register fastf_t 	max_col_dist;
	register fastf_t 	max_dist;
	int	dir;
	fastf_t        * mesh_ptr = srf->mesh->ctl_points;
	int	coords = EXTRACT_COORDS(srf->mesh->pt_type);
	int	j, i, k;
	int	mesh_elt;
	vect_t          p1, p2, p3, p4, v1, v2, v3;
	vect_t          nrm;
	fastf_t         nrmln;
	fastf_t         dist;
	fastf_t        * crv;
	fastf_t 	spl_crv_flat();
	int	otherdir;

	dir = srf->dir;

	otherdir = (dir == ROW) ? COL : ROW;

	max_row_dist = max_col_dist = -SPL_INFINIT;

	crv = (fastf_t * ) rt_malloc( sizeof(fastf_t) * 
	    EXTRACT_COORDS(srf->mesh->pt_type) * srf->mesh->s_size[1], 
	    "rt_nurb_s_flat: crv");

	/* Test Row and COL curves for flatness, 
	 * If a curve is not flat than get distance to line */

	/* Test Row Curves */

	for (i = 0; i < (srf->mesh->s_size[0]); i++) {
		fastf_t rdist;
		for (j = 0; 
		    j < (srf->mesh->s_size[1] * 
			EXTRACT_COORDS(srf->mesh->pt_type)); 
		    j++)
			crv[j] = *mesh_ptr++;

		rdist = internal_crv_flat(crv, srf->mesh->s_size[1], 
		    srf->mesh->pt_type);
		max_row_dist = MAX(max_row_dist, rdist);
	}

	rt_free( (char *)crv, "rt_nurb_s_flat: crv" );

	crv = (fastf_t * ) rt_malloc(sizeof(fastf_t) * 
	    EXTRACT_COORDS(srf->mesh->pt_type) *  
	    srf->mesh->s_size[0], 	"rt_nurb_s_flat: crv");

	for (i = 0; i < (coords * srf->mesh->s_size[1]); i += coords) {
		fastf_t rdist;

		for (j = 0; j < (srf->mesh->s_size[0]); j++) {
			mesh_elt = 
			    (j * (srf->mesh->s_size[1] * coords)) + i;

			for (k = 0; k < coords; k++)
				crv[j * coords + k] = 
				    srf->mesh->ctl_points[mesh_elt + k];
		}

		rdist = internal_crv_flat(crv, 
		    srf->mesh->s_size[0], srf->mesh->pt_type);

		max_col_dist = MAX( max_col_dist, rdist);
	}

	rt_free((char *)crv, "rt_nurb_s_flat: crv");

	max_dist = MAX( max_row_dist, max_col_dist);

	if ( max_dist > epsilon) {
		if ( max_row_dist > max_col_dist )
			return ROW;
		else
			return COL;
	}

	/* Test the corners to see if they lie in a plane. */

	/*
	 * Extract the four corners and put a plane through three of them and
	 * see how far the fourth is to the plane. 
	 */

	mesh_ptr = srf->mesh->ctl_points;

	if ( !EXTRACT_RAT(srf->mesh->pt_type) ) {

		VMOVE(p1, mesh_ptr);
		VMOVE(p2,
		    (mesh_ptr + (srf->mesh->s_size[1] - 1) * coords));
		VMOVE(p3,
		    (mesh_ptr + 
		    ((srf->mesh->s_size[1] * 
		    (srf->mesh->s_size[0] - 1)) + 
		    (srf->mesh->s_size[1] - 1)) * coords));

		VMOVE(p4,
		    (mesh_ptr + 
		    (srf->mesh->s_size[1] * 
		    (srf->mesh->s_size[0] - 1)) * coords));
	} else
	 {
		hvect_t h1, h2, h3, h4;

		HVMOVE(h1, mesh_ptr);
		HVMOVE(h2,
		    (mesh_ptr + (srf->mesh->s_size[1] - 1) * coords));
		HVMOVE(h3,
		    (mesh_ptr + 
		    ((srf->mesh->s_size[1] * 
		    (srf->mesh->s_size[0] - 1)) + 
		    (srf->mesh->s_size[1] - 1)) * coords));

		HVMOVE(h4,
		    (mesh_ptr + 
		    (srf->mesh->s_size[1] * 
		    (srf->mesh->s_size[0] - 1)) * coords));

		HDIVIDE( p1, h1 );
		HDIVIDE( p2, h2 );
		HDIVIDE( p3, h3 );
		HDIVIDE( p4, h4 );
	}

	VSUB2(v1, p2, p1);
	VSUB2(v2, p3, p1);

	VCROSS(nrm, v1, v2);

	nrmln = MAGNITUDE(nrm);

	if (APX_EQ(nrmln, 0.0))
		return FLAT;

	VSUB2(v3, p4, p1);

	dist = fabs(VDOT( v3, nrm)) / nrmln;

	if (dist > epsilon)
		return otherdir;

	return FLAT;		/* Must be flat */

}


fastf_t 
internal_crv_flat( crv, size, pt_type )
fastf_t *crv;
int	size;
int	pt_type;
{
	point_t         p1, p2;
	vect_t          ln;
	int	i;
	fastf_t         dist;
	fastf_t 	max_dist;
	fastf_t         length;
	fastf_t        * c_ptr;
	vect_t          testv, xp;
	hvect_t         h1, h2;
	int	coords;
	int	rational;

	coords = EXTRACT_COORDS( pt_type);
	rational = EXTRACT_RAT( pt_type);
	max_dist = -SPL_INFINIT;

	if ( !rational) {
		VMOVE(p1, crv);
	} else	 {
		HVMOVE( h1, crv);
		HDIVIDE( p1, h1);
	}

	length = 0.0;

	/*
	 * loop through all of the points until a line is found which may not
	 * be the end pts of the curve if the endpoints are the same. 
	 */
	for (i = size - 1; (i > 0) && APX_EQ(length, 0.0); i--) {
		if ( !rational) {
			VMOVE(p2, (crv + (i * coords)));
		} else {
			HVMOVE( h2, (crv + ( i * coords)));
			HDIVIDE( p2, h2 );
		}

		VSUB2(ln, p1, p2);
		length = MAGNITUDE(ln);
	}


	if (!APX_EQ(length, 0.0)) {
		VSCALE(ln, ln, 1.0 / length);
		c_ptr = crv + coords;

		for (i = 1; i < size; i++) {
			if ( !rational ) {
				VSUB2(testv, p1, c_ptr);
			} else		    {
				HDIVIDE(h2, c_ptr);
				VSUB2( testv, p1, h2);
			}

			VCROSS(xp, testv, ln);
			dist = MAGNITUDE(xp);
			max_dist = MAX( max_dist, dist);
			c_ptr += coords;
		}
	}
	return max_dist;
}
