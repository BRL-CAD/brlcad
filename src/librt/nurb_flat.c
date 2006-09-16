/*                     N U R B _ F L A T . C
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
/** @file nurb_flat.c
 *  Tests the NURB surface to see if its flat depending
 *  on the epsilon passed.
 *
 * Author -
 *     Paul R. Stay
 *
 * Source -
 *     SECAD/VLD Computing Consortium, Bldg 394
 *     The U.S. Army Ballistic Research Laboratory
 *     Aberdeen Proving Ground, Maryland 21005
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

int
rt_nurb_s_flat(struct face_g_snurb *srf, fastf_t epsilon)

                		/* Epsilon value for flatness testing */
{
	register fastf_t 	max_row_dist;
	register fastf_t 	max_col_dist;
	register fastf_t 	max_dist;
	int	dir;
	fastf_t        * mesh_ptr = srf->ctl_points;
	int	coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);
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

	otherdir = (dir == RT_NURB_SPLIT_ROW) ? RT_NURB_SPLIT_COL : RT_NURB_SPLIT_ROW;

	max_row_dist = max_col_dist = -INFINITY;

	crv = (fastf_t * ) bu_malloc( sizeof(fastf_t) *
	    RT_NURB_EXTRACT_COORDS(srf->pt_type) * srf->s_size[1],
	    "rt_nurb_s_flat: crv");

	/* Test Row and RT_NURB_SPLIT_COL curves for flatness,
	 * If a curve is not flat than get distance to line */

	/* Test Row Curves */

	for (i = 0; i < (srf->s_size[0]); i++) {
		fastf_t rdist;
		for (j = 0;
		    j < (srf->s_size[1] *
			RT_NURB_EXTRACT_COORDS(srf->pt_type));
		    j++)
			crv[j] = *mesh_ptr++;

		rdist = rt_nurb_crv_flat(crv, srf->s_size[1],
		    srf->pt_type);
		max_row_dist = MAX(max_row_dist, rdist);
	}

	bu_free( (char *)crv, "rt_nurb_s_flat: crv" );

	crv = (fastf_t * ) bu_malloc(sizeof(fastf_t) *
	    RT_NURB_EXTRACT_COORDS(srf->pt_type) *
	    srf->s_size[0], 	"rt_nurb_s_flat: crv");

	for (i = 0; i < (coords * srf->s_size[1]); i += coords) {
		fastf_t rdist;

		for (j = 0; j < (srf->s_size[0]); j++) {
			mesh_elt =
			    (j * (srf->s_size[1] * coords)) + i;

			for (k = 0; k < coords; k++)
				crv[j * coords + k] =
				    srf->ctl_points[mesh_elt + k];
		}

		rdist = rt_nurb_crv_flat(crv,
		    srf->s_size[0], srf->pt_type);

		max_col_dist = MAX( max_col_dist, rdist);
	}

	bu_free((char *)crv, "rt_nurb_s_flat: crv");

	max_dist = MAX( max_row_dist, max_col_dist);

	if ( max_dist > epsilon) {
		if ( max_row_dist > max_col_dist )
			return RT_NURB_SPLIT_ROW;
		else
			return RT_NURB_SPLIT_COL;
	}

	/* Test the corners to see if they lie in a plane. */

	/*
	 * Extract the four corners and put a plane through three of them and
	 * see how far the fourth is to the plane.
	 */

	mesh_ptr = srf->ctl_points;

	if ( !RT_NURB_IS_PT_RATIONAL(srf->pt_type) ) {

		VMOVE(p1, mesh_ptr);
		VMOVE(p2,
		    (mesh_ptr + (srf->s_size[1] - 1) * coords));
		VMOVE(p3,
		    (mesh_ptr +
		    ((srf->s_size[1] *
		    (srf->s_size[0] - 1)) +
		    (srf->s_size[1] - 1)) * coords));

		VMOVE(p4,
		    (mesh_ptr +
		    (srf->s_size[1] *
		    (srf->s_size[0] - 1)) * coords));
	} else
	 {
		hvect_t h1, h2, h3, h4;
	 	int	offset;

		HMOVE(h1, mesh_ptr);
		HDIVIDE( p1, h1 );

	 	offset = (srf->s_size[1] - 1) * coords;
		HMOVE(h2, mesh_ptr + offset);
		HDIVIDE( p2, h2 );

	 	offset =
		    ((srf->s_size[1] *
		    (srf->s_size[0] - 1)) +
		    (srf->s_size[1] - 1)) * coords;
		HMOVE(h3, mesh_ptr + offset);
		HDIVIDE( p3, h3 );

	 	offset =
		    (srf->s_size[1] *
		    (srf->s_size[0] - 1)) * coords;
		HMOVE(h4, mesh_ptr + offset);
		HDIVIDE( p4, h4 );
	}

	VSUB2(v1, p2, p1);
	VSUB2(v2, p3, p1);

	VCROSS(nrm, v1, v2);

	nrmln = MAGNITUDE(nrm);
	if( nrmln < 0.0001 )			/* XXX Why this constant? */
		return RT_NURB_SPLIT_FLAT;

	VSUB2(v3, p4, p1);

	dist = fabs(VDOT( v3, nrm)) / nrmln;

	if (dist > epsilon)
		return otherdir;

	return RT_NURB_SPLIT_FLAT;		/* Must be flat */

}


fastf_t
rt_nurb_crv_flat(fastf_t *crv, int size, int pt_type)
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

	coords = RT_NURB_EXTRACT_COORDS( pt_type);
	rational = RT_NURB_IS_PT_RATIONAL( pt_type);
	max_dist = -INFINITY;

	if ( !rational) {
		VMOVE(p1, crv);
	} else	 {
		HMOVE( h1, crv);
		HDIVIDE( p1, h1);
	}

	length = 0.0;

	/*
	 * loop through all of the points until a line is found which may not
	 * be the end pts of the curve if the endpoints are the same.
	 */
	for (i = size - 1; (i > 0) && length < SQRT_SMALL_FASTF; i--) {
		if ( !rational) {
			VMOVE(p2, (crv + (i * coords)));
		} else {
			HMOVE( h2, (crv + ( i * coords)));
			HDIVIDE( p2, h2 );
		}

		VSUB2(ln, p1, p2);
		length = MAGNITUDE(ln);
	}


	if( length >= SQRT_SMALL_FASTF )  {
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
