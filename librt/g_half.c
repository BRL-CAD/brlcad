/*
 *  			H A L F . C
 *  
 *  Function -
 *  	Intersect a ray with a Halfspace
 *  
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCShalf[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/raytrace.h"
#include "debug.h"


/*
 *			Ray/HALF Intersection
 *
 *  A HALFSPACE is defined by an outward pointing normal vector,
 *  and the distance from the origin to the plane, which is defined
 *  by N and d.
 *
 *  With outward pointing normal vectors,
 *  note that the ray enters the half-space defined by a plane when D cdot N <
 *  0, is parallel to the plane when D cdot N = 0, and exits otherwise.
 */

struct half_specific  {
	fastf_t	half_d;			/* dist from origin along N */
	vect_t	half_N;			/* Unit-length Normal (outward) */
	vect_t	half_Xbase;		/* "X" basis direction */
	vect_t	half_Ybase;		/* "Y" basis direction */
};
#define HALF_NULL	((struct half_specific *)0)

/*
 *			M A K E _ P E R P
 *
 *  Given a vector, create another vector which is perpendicular to it.
 */
void
make_perp( new, old )
vect_t new, old;
{
	register int i;
	LOCAL vect_t another;	/* Another vector, different */

	i = X;
	if( fabs(old[Y])<fabs(old[i]) )  i=Y;
	if( fabs(old[Z])<fabs(old[i]) )  i=Z;
	VSET( another, 0, 0, 0 );
	another[i] = 1.0;
	if( old[X] == 0 && old[Y] == 0 && old[Z] == 0 )  {
		VMOVE( new, another );
	} else {
		VCROSS( new, another, old );
	}
}

/*
 *  			H L F _ P R E P
 */
hlf_prep( vec, stp, mat )
fastf_t *vec;
struct soltab *stp;
matp_t mat;
{
	register struct half_specific *halfp;
	FAST fastf_t f;

	/*
	 * Process a HALFSPACE, which is represented as a 
	 * normal vector, and a distance.
	 */
	GETSTRUCT( halfp, half_specific );
	stp->st_specific = (int *)halfp;

	VMOVE( halfp->half_N, &vec[0] );
	f = MAGSQ( halfp->half_N );
	if( f < 0.001 )  {
		rt_log("half(%s):  bad normal\n", stp->st_name );
		return(1);	/* BAD */
	}
	f -= 1.0;
	if( !NEAR_ZERO( f, 0.001 ) )  {
		rt_log("half(%s):  normal not unit length\n", stp->st_name );
		VUNITIZE( halfp->half_N );
	}
	halfp->half_d = vec[1*ELEMENTS_PER_VECT];
	VSCALE( stp->st_center, halfp->half_N, halfp->half_d );

	/* X and Y basis for uv map */
	make_perp( halfp->half_Xbase, stp->st_center );
	VCROSS( halfp->half_Ybase, halfp->half_Xbase, halfp->half_N );
	VUNITIZE( halfp->half_Xbase );
	VUNITIZE( halfp->half_Ybase );

	/* No bounding sphere or bounding RPP is possible */
	VSET( stp->st_min, -INFINITY,-INFINITY,-INFINITY);
	VSET( stp->st_max,  INFINITY, INFINITY, INFINITY);

	stp->st_aradius = INFINITY;
	stp->st_bradius = INFINITY;
	return(0);		/* OK */
}

/*
 *  			H L F _ P R I N T
 */
hlf_print( stp )
register struct soltab *stp;
{
	register struct half_specific *halfp =
		(struct half_specific *)stp->st_specific;
	register int i;

	if( halfp == HALF_NULL )  {
		rt_log("half(%s):  no data?\n", stp->st_name);
		return;
	}
	VPRINT( "Normal", halfp->half_N );
	rt_log( "d = %f\n", halfp->half_d );
	VPRINT( "Xbase", halfp->half_Xbase );
	VPRINT( "Ybase", halfp->half_Ybase );
}

/*
 *			H L F _ S H O T
 *  
 * Function -
 *	Shoot a ray at a HALFSPACE
 *
 * Algorithm -
 * 	The intersection distance is computed.
 *  
 * Returns -
 *	0	MISS
 *  	segp	HIT
 */
struct seg *
hlf_shot( stp, rp )
struct soltab *stp;
register struct xray *rp;
{
	register struct half_specific *halfp =
		(struct half_specific *)stp->st_specific;
	LOCAL struct half_specific *iplane, *oplane;
	FAST fastf_t	dn;		/* Direction dot Normal */
	FAST fastf_t	dxbdn;
	LOCAL fastf_t	in, out;	/* ray in/out distances */

	in = -INFINITY;
	out = INFINITY;
	iplane = oplane = HALF_NULL;

	dxbdn = VDOT( halfp->half_N, rp->r_pt ) - halfp->half_d;
	if( (dn = -VDOT( halfp->half_N, rp->r_dir )) < -1.0e-10 )  {
		/* exit point, when dir.N < 0.  out = min(out,s) */
		out = dxbdn/dn;
		oplane = halfp;
	} else if ( dn > 1.0e-10 )  {
		/* entry point, when dir.N > 0.  in = max(in,s) */
		in = dxbdn/dn;
		iplane = halfp;
	}  else  {
		/* ray is parallel to plane when dir.N == 0.
		 * If it is outside the solid, stop now */
		if( dxbdn > 0.0 )
			return( SEG_NULL );	/* MISS */
	}
	if( rt_g.debug & DEBUG_TESTING )
		rt_log("half: in=%f, out=%f\n", in, out);

	{
		register struct seg *segp;

		GET_SEG( segp );
		segp->seg_stp = stp;
		segp->seg_in.hit_dist = in;
		segp->seg_in.hit_private = (char *)iplane;

		segp->seg_out.hit_dist = out;
		segp->seg_out.hit_private = (char *)oplane;
		return(segp);			/* HIT */
	}
	/* NOTREACHED */
}

/*
 *  			H L F _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
hlf_norm( hitp, stp, rp )
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	register struct half_specific *halfp =
		(struct half_specific *)hitp->hit_private;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	if( hitp->hit_private == (char *)0 )  {
		VREVERSE( hitp->hit_normal, halfp->half_N );	/* "exit" */
	}  else  {
		VMOVE( hitp->hit_normal, halfp->half_N );
	}
}

/*
 *  This is a pretty shabby replacement for the
 *  not always available fmod() routine.
 */
double
ffmod(x,y)
double x,y;
{
	register long i;

	i = (long)x/y;
	return( x - i * y );
}

/*
 *  			H L F _ U V
 *  
 *  For a hit on a face of an HALF, return the (u,v) coordinates
 *  of the hit point.  0 <= u,v <= 1.
 *  u extends along the Xbase direction
 *  v extends along the "Ybase" direction
 *  Note that a "toroidal" map is established, varying each from
 *  0 up to 1 and then back down to 0 again.
 */
hlf_uv( stp, hitp, uvp )
struct soltab *stp;
register struct hit *hitp;
register fastf_t *uvp;
{
	register struct half_specific *halfp =
		(struct half_specific *)hitp->hit_private;
	LOCAL vect_t P_A;
	FAST fastf_t f;

	VSUB2( P_A, hitp->hit_point, stp->st_center );

	f = VDOT( P_A, halfp->half_Xbase )/10000;
	if( f < 0 )  f = -f;
	f = ffmod( f, 1.0 );
	if( f < 0.5 )
		uvp[0] = 2 * f;		/* 0..1 */
	else
		uvp[0] = 2 * (1 - f);	/* 1..0 */

	f = VDOT( P_A, halfp->half_Ybase )/10000;
	if( f < 0 )  f = -f;
	f = ffmod( f, 1.0 );
	if( f < 0.5 )
		uvp[1] = 2 * f;		/* 0..1 */
	else
		uvp[1] = 2 * (1 - f);	/* 1..0 */

	if( uvp[0] < 0 || uvp[1] < 0 )  {
		if( rt_g.debug )
			rt_log("half_uv: bad uv=%f,%f\n", uvp[0], uvp[1]);
		/* Fix it up */
		if( uvp[0] < 0 )  uvp[0] = (-uvp[0]);
		if( uvp[1] < 0 )  uvp[1] = (-uvp[1]);
	}
}
