/* 
 *       N U R B _ B O U N D . C
 *
 * Function -
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
 * Copyright Notice -
 *     This software is Copyright (C) 1990 by the United States Army.
 *     All rights reserved.
 */

/* Since a B-Spline surface follows the convex hull property 
 * the bounding box can be found by taking the min and max of
 * all points in the control mesh. If the surface mesh contains
 * homogeneous points (i.e. [XYZW]) then divide out the W first.
 */

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "./nurb.h"

#define NEAR_ZERO(val,epsilon)	( ((val) > -epsilon) && ((val) < epsilon) )
#define INFINITY	(1.0e20)

/* rt_nurb_sbound()
 * 	Calculates the bounding Right Parallel Piped (RPP) of the
 *	NURB surface, and returns the minimum and maximum points
 * 	of the surface.
 */

int
rt_nurb_s_bound( srf, bmin, bmax )
struct snurb *srf;
point_t bmin, bmax;
{
	register fastf_t *p_ptr;	/* Mesh pointr */
	register int	coords;		/* Elements per vector */
	int	i;
	int	rat;


	bmin[0] = bmin[1] = bmin[2] = INFINITY;
	bmax[0] = bmax[1] = bmax[2] = -INFINITY;

	if ( srf == (struct snurb *)0 )  {
		rt_log("nurb_s_bound:  NULL surface\n");
		return(-1);		/* BAD */
	}

	p_ptr = srf->mesh->ctl_points;
	coords = EXTRACT_COORDS(srf->mesh->pt_type);
	rat =    EXTRACT_RAT(srf->mesh->pt_type);

	for ( i = ( srf->mesh->s_size[ROW] * 
	    srf->mesh->s_size[COL] ); i > 0; i--) {
		if ( !rat ) {
			VMINMAX( bmin, bmax, p_ptr );
		} else if ( rat  ) {
			point_t tmp_pt;
			if ( NEAR_ZERO( p_ptr[H], SMALL ) )  {
				HPRINT( "mesh point", p_ptr );
				rt_log("nurb_s_bound:  H too small\n");
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
rt_nurb_c_bound( crv, bmin, bmax )
struct cnurb *crv;
point_t bmin, bmax;
{
	register fastf_t *p_ptr;	/* Mesh pointr */
	register int	coords;		/* Elements per vector */
	int	i;
	int	rat;


	bmin[0] = bmin[1] = bmin[2] = INFINITY;
	bmax[0] = bmax[1] = bmax[2] = -INFINITY;

	if ( crv == (struct cnurb *)0 )  {
		rt_log("nurb_c_bound:  NULL surface\n");
		return(-1);		/* BAD */
	}

	p_ptr = crv->mesh->ctl_points;
	coords = EXTRACT_COORDS(crv->mesh->pt_type);
	rat =    EXTRACT_RAT(crv->mesh->pt_type);

	for ( i = crv->mesh->c_size; i > 0; i--) {
		if ( !rat ) {
			VMINMAX( bmin, bmax, p_ptr );
		} else if ( rat  ) {
			point_t tmp_pt;
			if ( NEAR_ZERO( p_ptr[H], SMALL ) )  {
				HPRINT( "mesh point", p_ptr );
				rt_log("nurb_c_bound:  H too small\n");
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
rt_nurb_s_check( srf )
register struct snurb *srf;
{
	register fastf_t *mp;	/* Mesh pointr */
	register int	i;

	mp = srf->mesh->ctl_points;
	i = srf->mesh->s_size[ROW] * 
	    srf->mesh->s_size[COL] * 
	    srf->mesh->pt_type;
	for ( ; i > 0; i--, mp++)  {
		/* Sanity checking */
		if ( !NEAR_ZERO( *mp, INFINITY ) )  {
			rt_log("nurb_s_check:  bad mesh found\n");
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
rt_nurb_c_check( crv )
register struct cnurb *crv;
{
	register fastf_t *mp;	/* Mesh pointr */
	register int	i;

	mp = crv->mesh->ctl_points;
	i = crv->mesh->c_size * 
	    crv->mesh->pt_type;
	for ( ; i > 0; i--, mp++)  {
		/* Sanity checking */
		if ( !NEAR_ZERO( *mp, INFINITY ) )  {
			rt_log("nurb_c_check:  bad mesh found\n");
			return(-1);	/* BAD */
		}
	}
	return(0);			/* OK */
}


