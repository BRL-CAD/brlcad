/*
 *  			A R B . C
 *  
 *  Function -
 *  	Intersect a ray with an Arbitrary Regular Polyhedron with
 *  	as many as 8 vertices.
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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "raytrace.h"
#include "debug.h"


/* Describe algorithm here */

#define MAXPTS	4			/* All we need are 4 points */
static point_t	points[MAXPTS];		/* Actual points on plane */
static int	arb_npts;		/* number of points on plane */
static char	arb_code[MAXPTS+1];	/* Face code string.  Decorative. */

struct arb_specific  {
	float	arb_A[3];		/* "A" point */
	float	arb_Xbasis[3];		/* X (B-A) vector (for 2d coords) */
	float	arb_Ybasis[3];		/* "Y" vector (perp to N and X) */
	float	arb_N[3];		/* Unit-length Normal (outward) */
	float	arb_NdotA;		/* Normal dot A */
	float	arb_XXlen;		/* 1/(Xbasis dot Xbasis) */
	float	arb_YYlen;		/* 1/(Ybasis dot Ybasis) */
	struct arb_specific *arb_forw;	/* Forward link */
};
#define ARB_NULL	((struct arb_specific *)0)

#define MINMAX(a,b,c)	{ FAST fastf_t ftemp;\
			if( (ftemp = (c)) < (a) )  a = ftemp;\
			if( ftemp > (b) )  b = ftemp; }

#undef EPSILON
#define EPSILON	0.005		/* More appropriate for NEAR_ZERO here */

HIDDEN int	arb_face(), arb_add_pt();

static struct arb_specific *FreeArb;	/* Head of free list */

/*
 *  			A R B _ P R E P
 */
arb_prep( vec, stp, mat )
fastf_t *vec;
struct soltab *stp;
matp_t mat;
{
	register fastf_t *op;		/* Used for scanning vectors */
	LOCAL fastf_t dx, dy, dz;	/* For finding the bounding spheres */
	LOCAL vect_t	work;		/* Vector addition work area */
	LOCAL vect_t	sum;		/* Sum of all endpoints */
	LOCAL int	faces;		/* # of faces produced */
	register int	i;
	LOCAL fastf_t	f;

	/*
	 * Process an ARB8, which is represented as a vector
	 * from the origin to the first point, and 7 vectors
	 * from the first point to the remaining points.
	 *
	 * Convert from vector to point notation IN PLACE
	 * by rotating vectors and adding base vector.
	 */
	VSET( sum, 0, 0, 0 );
	op = &vec[1*ELEMENTS_PER_VECT];
	for( i=1; i<8; i++ )  {
		VADD2( work, &vec[0], op );
		MAT4X3PNT( op, mat, work );
		VADD2( sum, sum, op );			/* build the sum */
		op += ELEMENTS_PER_VECT;
	}
	MAT4X3PNT( work, mat, vec );			/* first point */
	VMOVE( vec, work );				/* 1st: IN PLACE*/
	VADD2( sum, sum, vec );				/* sum=0th element */

	/*
	 *  Determine a point which is guaranteed to be within the solid.
	 *  This is done by averaging all the vertices.  This center is
	 *  needed for arb_add_pt, which demands a point inside the solid.
	 *  The center of the enclosing RPP strategy used for the bounding
	 *  sphere can be tricked by thin plates which are non-axis aligned,
	 *  so this dual-strategy is required.  (What a bug hunt!).
	 *  The actual work is done in the loop, above.
	 */
	VSCALE( stp->st_center, sum, 0.125 );	/* sum/8 */

	stp->st_specific = (int *) 0;

#define P(x)	(&vec[(x)*ELEMENTS_PER_VECT])
	faces = 0;
	if( arb_face( stp, 3, 2, 1, 0, P(3), P(2), P(1), P(0) ) )
		faces++;					/* 1234 */
	if( arb_face( stp, 4, 5, 6, 7, P(4), P(5), P(6), P(7) ) )
		faces++;					/* 8765 */
	if( arb_face( stp, 4, 7, 3, 0, P(4), P(7), P(3), P(0) ) )
		faces++;					/* 1485 */
	if( arb_face( stp, 2, 6, 5, 1, P(2), P(6), P(5), P(1) ) )
		faces++;					/* 2673 */
	if( arb_face( stp, 1, 5, 4, 0, P(1), P(5), P(4), P(0) ) )
		faces++;					/* 1562 */
	if( arb_face( stp, 7, 6, 2, 3, P(7), P(6), P(2), P(3) ) )
		faces++;					/* 4378 */
#undef P

	if( faces < 4  || faces > 6 )  {
		rtlog("arb(%s):  only %d faces present\n",
			stp->st_name, faces);
		/* Should free storage for good faces */
		return(1);			/* Error */
	}

	/*
	 * Compute bounding sphere which contains the bounding RPP.
	 * Find min and max of the point co-ordinates to find the
	 * bounding RPP.  Note that this center is NOT guaranteed
	 * to be contained within the solid!
	 */
	op = &vec[0];
	for( i=0; i< 8; i++ ) {
		MINMAX( stp->st_min[X], stp->st_max[X], *op++ );
		MINMAX( stp->st_min[Y], stp->st_max[Y], *op++ );
		MINMAX( stp->st_min[Z], stp->st_max[Z], *op++ );
		op++;		/* Depends on ELEMENTS_PER_VECT */
	}
	VSET( stp->st_center,
		(stp->st_max[X] + stp->st_min[X])/2,
		(stp->st_max[Y] + stp->st_min[Y])/2,
		(stp->st_max[Z] + stp->st_min[Z])/2 );

	dx = (stp->st_max[X] - stp->st_min[X])/2;
	f = dx;
	dy = (stp->st_max[Y] - stp->st_min[Y])/2;
	if( dy > f )  f = dy;
	dz = (stp->st_max[Z] - stp->st_min[Z])/2;
	if( dz > f )  f = dz;
	stp->st_aradius = f;
	stp->st_bradius = sqrt(dx*dx + dy*dy + dz*dz);
	return(0);		/* OK */
}

/*
 *			A R B _ F A C E
 *
 *  This function is called with pointers to 4 points,
 *  and is used to prepare both ARS and ARB8 faces.
 *  a,b,c,d are "index" values, merely decorative.
 *  ap, bp, cp, dp point to vect_t points.
 *  noise is non-zero for ARB8, for non-planar face complaints.
 *
 * Return -
 *	0	if the 4 points didn't form a plane (eg, colinear, etc).
 *	#pts	(>=3) if a valid plane resulted.  # valid pts is returned.
 */
HIDDEN int
arb_face( stp, a, b, c, d, ap, bp, cp, dp )
struct soltab *stp;
int a, b, c, d;
pointp_t ap, bp, cp, dp;
{
	register struct arb_specific *arbp;
	register int pts;

	while( (arbp=FreeArb) == ARB_NULL )  {
		register struct arb_specific *arbp;
		register int bytes;

		bytes = byte_roundup(64*sizeof(struct arb_specific));
		arbp = (struct arb_specific *)vmalloc(bytes, "get_arb");
		while( bytes >= sizeof(struct arb_specific) )  {
			arbp->arb_forw = FreeArb;
			FreeArb = arbp++;
			bytes -= sizeof(struct arb_specific);
		}
	}
	FreeArb = arbp->arb_forw;

	arb_npts = 0;
	arb_add_pt( ap, stp, arbp, a );
	arb_add_pt( bp, stp, arbp, b );
	arb_add_pt( cp, stp, arbp, c );
	arb_add_pt( dp, stp, arbp, d );

	if( arb_npts < 3 )  {
		arbp->arb_forw = FreeArb;
		FreeArb = arbp;
		return(0);				/* BAD */
	}

	/* Add this face onto the linked list for this solid */
	arbp->arb_forw = (struct arb_specific *)stp->st_specific;
	stp->st_specific = (int *)arbp;
	return(pts);					/* OK */
}

/*
 *			A R B _ A D D _ P T
 *
 *  Add another point to a struct arb_specific, checking for unique pts.
 *  The first two points are easy.  The third one triggers most of the
 *  plane calculations, and forth and subsequent ones are merely
 *  checked for validity.
 */
HIDDEN int
arb_add_pt( point, stp, arbp, a )
register pointp_t point;
struct soltab *stp;
register struct arb_specific *arbp;
int a;
{
	register int i;
	LOCAL vect_t work;
	LOCAL vect_t P_A;		/* new point - A */
	LOCAL fastf_t f;

	/* Verify that this point is not the same as an earlier point */
	for( i=0; i < arb_npts; i++ )  {
		VSUB2( work, point, points[i] );
		if( MAGSQ( work ) < EPSILON )
			return(0);			/* BAD */
	}
	i = arb_npts++;		/* Current point number */
	VMOVE( points[i], point );
	arb_code[i] = '0'+a;
	arb_code[i+1] = '\0';

	/* The first 3 points are treated differently */
	switch( i )  {
	case 0:
		VMOVE( arbp->arb_A, point );
		return(1);				/* OK */
	case 1:
		VSUB2( arbp->arb_Xbasis, point, arbp->arb_A );	/* B-A */
		arbp->arb_XXlen = 1.0 / VDOT( arbp->arb_Xbasis, arbp->arb_Xbasis );
		return(1);				/* OK */
	case 2:
		VSUB2( P_A, point, arbp->arb_A );	/* C-A */
		/* Check for co-linear, ie, (B-A)x(C-A) == 0 */
		VCROSS( arbp->arb_N, arbp->arb_Xbasis, P_A );
		f = MAGNITUDE( arbp->arb_N );
		if( NEAR_ZERO(f) )  {
			arb_npts--;
			arb_code[2] = '\0';
			return(0);			/* BAD */
		}
		VUNITIZE( arbp->arb_N );
		VCROSS( arbp->arb_Ybasis, arbp->arb_N, arbp->arb_Xbasis );
		arbp->arb_YYlen = 1.0 / VDOT( arbp->arb_Ybasis, arbp->arb_Ybasis );

		/*
		 *  If C-A is clockwise from B-A, then the normal
		 *  points inwards, so we need to fix it here.
		 */
		VSUB2( work, arbp->arb_A, stp->st_center );
		f = VDOT( work, arbp->arb_N );
		if( f < 0.0 )  {
			VREVERSE(arbp->arb_N, arbp->arb_N);	/* "fix" normal */
		}
		arbp->arb_NdotA = VDOT( arbp->arb_N, arbp->arb_A );
		return(1);				/* OK */
	default:
		/* Merely validate 4th and subsequent points */
		VSUB2( P_A, point, arbp->arb_A );
		VUNITIZE( P_A );		/* Checking direction only */
		f = VDOT( arbp->arb_N, P_A );
		if( ! NEAR_ZERO(f) )  {
			/* Non-planar face */
			rtlog("arb(%s): face %s non-planar, dot=%f\n",
				stp->st_name, arb_code, f );
#ifdef CONSERVATIVE
			arb_npts--;
			arb_code[i] = '\0';
			return(0);				/* BAD */
#endif
		}
		return(1);			/* OK */
	}
}

/*
 *  			A R B _ P R I N T
 */
arb_print( stp )
register struct soltab *stp;
{
	register struct arb_specific *arbp =
		(struct arb_specific *)stp->st_specific;
	register int i;

	if( arbp == (struct arb_specific *)0 )  {
		rtlog("arb(%s):  no faces\n", stp->st_name);
		return;
	}
	do {
		VPRINT( "A", arbp->arb_A );
		VPRINT( "Xbasis", arbp->arb_Xbasis );
		VPRINT( "Ybasis", arbp->arb_Ybasis );
		rtlog("XX fact =%f, YY fact = %f\n",
			arbp->arb_XXlen, arbp->arb_YYlen);
		VPRINT( "Normal", arbp->arb_N );
		rtlog( "N.A = %f\n", arbp->arb_NdotA );
		rtlog("\n");
	} while( arbp = arbp->arb_forw );
}

/*
 *			A R B _ S H O T
 *  
 * Function -
 *	Shoot a ray at an ARB8.
 *
 * Algorithm -
 * 	The intersection distance is computed for each face.
 *  The largest IN distance and the smallest OUT distance are
 *  used as the entry and exit points.
 *  
 * Returns -
 *	0	MISS
 *  	segp	HIT
 */
struct seg *
arb_shot( stp, rp )
struct soltab *stp;
register struct xray *rp;
{
	register struct arb_specific *arbp =
		(struct arb_specific *)stp->st_specific;
	LOCAL struct arb_specific *iplane, *oplane;
	LOCAL fastf_t	in, out;	/* ray in/out distances */

	in = -INFINITY;
	out = INFINITY;
	iplane = oplane = ARB_NULL;

	/* consider each face */
	for( ; arbp; arbp = arbp->arb_forw )  {
		FAST fastf_t	dn;		/* Direction dot Normal */
		FAST fastf_t	dxbdn;
		FAST fastf_t	s;

		dxbdn = VDOT( arbp->arb_N, rp->r_pt ) - arbp->arb_NdotA;
		if( (dn = -VDOT( arbp->arb_N, rp->r_dir )) < -EPSILON )  {
			/* exit point, when dir.N < 0.  out = min(out,s) */
			if( out > (s = dxbdn/dn) )  {
				out = s;
				oplane = arbp;
			}
		} else if ( dn > EPSILON )  {
			/* entry point, when dir.N > 0.  in = max(in,s) */
			if( in < (s = dxbdn/dn) )  {
				in = s;
				iplane = arbp;
			}
		}  else  {
			/* ray is parallel to plane when dir.N == 0.
			 * If it is outside the solid, stop now */
			if( dxbdn > 0.0 )
				return( SEG_NULL );	/* MISS */
		}
		if( debug & DEBUG_ARB8 )
			rtlog("arb: in=%f, out=%f\n", in, out);
		if( in > out )
			return( SEG_NULL );	/* MISS */
	}
	/* Validate */
	if( iplane == ARB_NULL || oplane == ARB_NULL )  {
		rtlog("arb_shoot(%s): 1 hit => MISS\n",
			stp->st_name);
		return( SEG_NULL );	/* MISS */
	}
	if( out < 0.0 || in >= out )
		return( SEG_NULL );	/* MISS */

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
 *  			A R B _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
arb_norm( hitp, stp, rp )
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	register struct arb_specific *arbp =
		(struct arb_specific *)hitp->hit_private;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	VMOVE( hitp->hit_normal, arbp->arb_N );
}

/*
 *  			A R B _ U V
 *  
 *  For a hit on a face of an ARB, return the (u,v) coordinates
 *  of the hit point.  0 <= u,v <= 1.
 *  u extends along the Xbasis direction defined by B-A,
 *  v extends along the "Ybasis" direction defined by (B-A)xN.
 */
arb_uv( stp, hitp, uvp )
struct soltab *stp;
register struct hit *hitp;
register fastf_t *uvp;
{
	register struct arb_specific *arbp =
		(struct arb_specific *)hitp->hit_private;
	LOCAL vect_t P_A;

	VSUB2( P_A, hitp->hit_point, arbp->arb_A );
	/* Flipping v is an artifact of how the faces are built */
	uvp[0] = VDOT( P_A, arbp->arb_Xbasis ) * arbp->arb_XXlen;
	uvp[1] = 1.0 - ( VDOT( P_A, arbp->arb_Ybasis ) * arbp->arb_YYlen );
	if( uvp[0] < 0 || uvp[1] < 0 )  {
		rtlog("arb_uv: bad uv=%f,%f\n", uvp[0], uvp[1]);
		/* Fix it up */
		if( uvp[0] < 0 )  uvp[0] = (-uvp[0]);
		if( uvp[1] < 0 )  uvp[1] = (-uvp[1]);
	}
}
