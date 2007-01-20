/*                    N U R B _ B O U N D . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2007 United States Government as represented by
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
/** @file nurb_bound.c
 *     Find the bounding box for the a NURB surface.
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

/* Since a B-Spline surface follows the convex hull property
 * the bounding box can be found by taking the min and max of
 * all points in the control  If the surface mesh contains
 * homogeneous points (i.e. [XYZW]) then divide out the W first.
 */

#include "common.h"



#include <stdio.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"

#define NEAR_ZERO(val,epsilon)	( ((val) > -epsilon) && ((val) < epsilon) )

#ifndef INFINITY		/* if INFINITY is not defined define it */
#define INFINITY	(1.0e20)
#endif

/* rt_nurb_sbound()
 * 	Calculates the bounding Right Parallel Piped (RPP) of the
 *	NURB surface, and returns the minimum and maximum points
 * 	of the surface.
 */

int
rt_nurb_s_bound(struct face_g_snurb *srf, fastf_t *bmin, fastf_t *bmax)
{
	register fastf_t *p_ptr;	/* Mesh pointr */
	register int	coords;		/* Elements per vector */
	int	i;
	int	rat;


	bmin[0] = bmin[1] = bmin[2] = INFINITY;
	bmax[0] = bmax[1] = bmax[2] = -INFINITY;

	if ( srf == (struct face_g_snurb *)0 )  {
		bu_log("nurb_s_bound:  NULL surface\n");
		return(-1);		/* BAD */
	}

	p_ptr = srf->ctl_points;
	coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);
	rat =    RT_NURB_IS_PT_RATIONAL(srf->pt_type);

	for ( i = ( srf->s_size[RT_NURB_SPLIT_ROW] *
	    srf->s_size[RT_NURB_SPLIT_COL] ); i > 0; i--) {
		if ( !rat ) {
			VMINMAX( bmin, bmax, p_ptr );
		} else if ( rat  ) {
			point_t tmp_pt;
			if ( NEAR_ZERO( p_ptr[H], SMALL ) )  {
				HPRINT( "mesh point", p_ptr );
				bu_log("nurb_s_bound:  H too small\n");
			} else {
				HDIVIDE( tmp_pt, p_ptr );
				VMINMAX( bmin, bmax, tmp_pt );
			}
		}
		p_ptr += coords;
	}
	return(0);	/* OK */
}


int
rt_nurb_c_bound(struct edge_g_cnurb *crv, fastf_t *bmin, fastf_t *bmax)
{
	register fastf_t *p_ptr;	/* Mesh pointr */
	register int	coords;		/* Elements per vector */
	int	i;
	int	rat;


	bmin[0] = bmin[1] = bmin[2] = INFINITY;
	bmax[0] = bmax[1] = bmax[2] = -INFINITY;

	if ( crv == (struct edge_g_cnurb *)0 )  {
		bu_log("nurb_c_bound:  NULL surface\n");
		return(-1);		/* BAD */
	}

	p_ptr = crv->ctl_points;
	coords = RT_NURB_EXTRACT_COORDS(crv->pt_type);
	rat =    RT_NURB_IS_PT_RATIONAL(crv->pt_type);

	for ( i = crv->c_size; i > 0; i--) {
		if ( !rat ) {
			VMINMAX( bmin, bmax, p_ptr );
		} else if ( rat  ) {
			point_t tmp_pt;
			if ( NEAR_ZERO( p_ptr[H], SMALL ) )  {
				HPRINT( "mesh point", p_ptr );
				bu_log("nurb_c_bound:  H too small\n");
			} else {
				HDIVIDE( tmp_pt, p_ptr );
				VMINMAX( bmin, bmax, tmp_pt );
			}
		}
		p_ptr += coords;
	}
	return(0);	/* OK */
}


/* rt_nurb_s_check( srf )
 * 	Checks the NURB surface control points to make
 *	sure no one point is near INIFITY, which probably means
 * 	that the surface mesh is bad.
 */

int
rt_nurb_s_check(register struct face_g_snurb *srf)
{
	register fastf_t *mp;	/* Mesh pointr */
	register int	i;

	mp = srf->ctl_points;
	i = srf->s_size[RT_NURB_SPLIT_ROW] *
	    srf->s_size[RT_NURB_SPLIT_COL] *
	    srf->pt_type;
	for ( ; i > 0; i--, mp++)  {
		/* Sanity checking */
		if ( !NEAR_ZERO( *mp, INFINITY ) )  {
			bu_log("nurb_s_check:  bad mesh found\n");
			return(-1);	/* BAD */
		}
	}
	return(0);			/* OK */
}



/* rt_nurb_c_check( srf )
 * 	Checks the NURB curve control points to make
 *	sure no one point is near INIFITY, which probably means
 * 	that the surface mesh is bad.
 */

int
rt_nurb_c_check(register struct edge_g_cnurb *crv)
{
	register fastf_t *mp;	/* Mesh pointr */
	register int	i;

	mp = crv->ctl_points;
	i = crv->c_size *
	    crv->pt_type;
	for ( ; i > 0; i--, mp++)  {
		/* Sanity checking */
		if ( !NEAR_ZERO( *mp, INFINITY ) )  {
			bu_log("nurb_c_check:  bad mesh found\n");
			return(-1);	/* BAD */
		}
	}
	return(0);			/* OK */
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
