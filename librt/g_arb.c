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
#include "../h/machine.h"
#include "../h/vmath.h"
#include "raytrace.h"
#include "debug.h"


/* Describe algorithm here */

#define MAXPTS	4			/* All we need are 4 points */
#define pl_A	pl_points[0]		/* Synonym for A point */

struct arb_specific  {
	int	pl_npts;		/* number of points on plane */
	point_t	pl_points[MAXPTS];	/* Actual points on plane */
	vect_t	pl_Xbasis;		/* X (B-A) vector (for 2d coords) */
	vect_t	pl_N;			/* Unit-length Normal (outward) */
	fastf_t	pl_NdotA;		/* Normal dot A */
	struct arb_specific *pl_forw;	/* Forward link */
	char	pl_code[MAXPTS+1];	/* Face code string.  Decorative. */
};

#define MINMAX(a,b,c)	{ FAST fastf_t ftemp;\
			if( (ftemp = (c)) < (a) )  a = ftemp;\
			if( ftemp > (b) )  b = ftemp; }

#undef EPSILON
#define EPSILON	0.005		/* More appropriate for NEAR_ZERO here */

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
	LOCAL int	i;

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
	 *  needed only for add_pt, which demands a point inside the solid.
	 *  The center of the enclosing RPP strategy used for the bounding
	 *  sphere can be tricked by thin plates which are non-axis aligned,
	 *  so this dual-strategy is required.  (What a bug hunt!).
	 *  The actual work is done in the loop, above.
	 */
	VSCALE( stp->st_center, sum, 0.125 );	/* sum/8 */

	stp->st_specific = (int *) 0;

#define P(x)	(&vec[(x)*ELEMENTS_PER_VECT])
	faces = 0;
	if( face( stp, 3, 2, 1, 0, P(3), P(2), P(1), P(0), 1 ) )
		faces++;					/* 1234 */
	if( face( stp, 4, 5, 6, 7, P(4), P(5), P(6), P(7), 1 ) )
		faces++;					/* 8765 */
	if( face( stp, 4, 7, 3, 0, P(4), P(7), P(3), P(0), 1 ) )
		faces++;					/* 1485 */
	if( face( stp, 2, 6, 5, 1, P(2), P(6), P(5), P(1), 1 ) )
		faces++;					/* 2673 */
	if( face( stp, 1, 5, 4, 0, P(1), P(5), P(4), P(0), 1 ) )
		faces++;					/* 1562 */
	if( face( stp, 7, 6, 2, 3, P(7), P(6), P(2), P(3), 1 ) )
		faces++;					/* 4378 */
#undef P

	if( faces < 4  || faces > 6 )  {
		fprintf(stderr,"arb(%s):  only %d faces present\n",
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
	dy = (stp->st_max[Y] - stp->st_min[Y])/2;
	dz = (stp->st_max[Z] - stp->st_min[Z])/2;
	stp->st_radsq = dx*dx + dy*dy + dz*dz;
	return(0);		/* OK */
}

/*
 *			F A C E
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
face( stp, a, b, c, d, ap, bp, cp, dp, noise )
struct soltab *stp;
int a, b, c, d;
pointp_t ap, bp, cp, dp;
int noise;
{
	register struct arb_specific *plp;
	register int pts;
	register int i;

	GETSTRUCT( plp, arb_specific );
	plp->pl_npts = 0;
	pts  = add_pt( ap, stp, plp, a, noise );
	pts += add_pt( bp, stp, plp, b, noise );
	pts += add_pt( cp, stp, plp, c, noise );
	pts += add_pt( dp, stp, plp, d, noise );

	if( pts < 3 )  {
		free(plp);
		return(0);				/* BAD */
	}

	/* Add this face onto the linked list for this solid */
	plp->pl_forw = (struct arb_specific *)stp->st_specific;
	stp->st_specific = (int *)plp;
	return(pts);					/* OK */
}

/*
 *			A D D _ P T
 *
 *  Add another point to a struct arb_specific, checking for unique pts.
 *  The first two points are easy.  The third one triggers most of the
 *  plane calculations, and forth and subsequent ones are merely
 *  check for validity.  If noise!=0, then non-planar 4th points give
 *  a warning message.  noise=1 for ARB8's, and noise=0 for ARS's.
 */
add_pt( point, stp, plp, a, noise )
register pointp_t point;
struct soltab *stp;
register struct arb_specific *plp;
int a;
int noise;			/* non-0: check 4,> pts for being planar */
{
	register int i;
	LOCAL vect_t work;
	LOCAL vect_t P_A;		/* new point - A */
	LOCAL fastf_t f;

	/* Verify that this point is not the same as an earlier point */
	for( i=0; i < plp->pl_npts; i++ )  {
		VSUB2( work, point, plp->pl_points[i] );
		if( MAGSQ( work ) < EPSILON )
			return(0);			/* BAD */
	}
	i = plp->pl_npts++;		/* Current point number */
	VMOVE( plp->pl_points[i], point );
	plp->pl_code[i] = '0'+a;
	plp->pl_code[i+1] = '\0';

	/* The first 3 points are treated differently */
	switch( i )  {
	case 0:
		return(1);				/* OK */
	case 1:
		VSUB2( plp->pl_Xbasis, point, plp->pl_A );	/* B-A */
		return(1);				/* OK */
	case 2:
		VSUB2( P_A, point, plp->pl_A );	/* C-A */
		/* Check for co-linear, ie, (B-A)x(C-A) == 0 */
		VCROSS( plp->pl_N, plp->pl_Xbasis, P_A );
		f = MAGNITUDE( plp->pl_N );
		if( NEAR_ZERO(f) )  {
			plp->pl_npts--;
			plp->pl_code[2] = '\0';
			return(0);			/* BAD */
		}
		VUNITIZE( plp->pl_N );

		/*
		 *  If C-A is clockwise from B-A, then the normal
		 *  points inwards, so we need to fix it here.
		 */
		VSUB2( work, plp->pl_A, stp->st_center );
		f = VDOT( work, plp->pl_N );
		if( f < 0.0 )  {
			VREVERSE(plp->pl_N, plp->pl_N);	/* "fix" normal */
		}
		plp->pl_NdotA = VDOT( plp->pl_N, plp->pl_A );
		return(1);				/* OK */
	default:
		/* Merely validate 4th and subsequent points */
		VSUB2( P_A, point, plp->pl_A );
		f = VDOT( plp->pl_N, P_A );
		if( ! NEAR_ZERO(f) )  {
			/* Non-planar face */
			if( noise )  {
				fprintf(stderr,"ERROR: arb(%s) face %s non-planar, dot=%f\n",
				stp->st_name, plp->pl_code, f );
			}
#ifdef CONSERVATIVE
			plp->pl_npts--;
			plp->pl_code[i] = '\0';
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
	register struct arb_specific *plp =
		(struct arb_specific *)stp->st_specific;
	register int i;

	if( plp == (struct arb_specific *)0 )  {
		fprintf(stderr,"arb(%s):  no faces\n", stp->st_name);
		return;
	}
	do {
		fprintf(stderr, "......Face %s\n", plp->pl_code );
		fprintf(stderr, "%d vertices:\n", plp->pl_npts );
		for( i=0; i < plp->pl_npts; i++ )  {
			VPRINT( "", plp->pl_points[i] );
		}
		VPRINT( "Xbasis", plp->pl_Xbasis );
		VPRINT( "Normal", plp->pl_N );
		fprintf(stderr, "N.A = %f\n", plp->pl_NdotA );
		putc('\n',stderr);
	} while( plp = plp->pl_forw );
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
	register struct arb_specific *plp =
		(struct arb_specific *)stp->st_specific;
	register struct seg *segp;
	LOCAL struct arb_specific *iplane, *oplane;
	LOCAL fastf_t	in, out;	/* ray in/out distances */
	FAST fastf_t	dxbdn;
	FAST fastf_t	dn;		/* Direction dot Normal */
	FAST fastf_t	s;

	in = -INFINITY;
	out = INFINITY;
#define PLANE_NULL	(struct arb_specific *)0
	iplane = oplane = PLANE_NULL;

	/* consider each face */
	for( ; plp; plp = plp->pl_forw )  {

		/* Ray Direction dot N.  (N is outward-pointing normal) */
		dn = -VDOT( plp->pl_N, rp->r_dir );
		dxbdn = VDOT( plp->pl_N, rp->r_pt ) - plp->pl_NdotA;
		if( debug & DEBUG_ARB8 )
			fprintf(stderr,"arb: face %s.  N.Dir=%f dxbdn=%f\n", plp->pl_code, dn, dxbdn );

		if( dn < -EPSILON )  {
			/* exit point, when dir.N < 0.  out = min(out,s) */
			if( out > (s = dxbdn/dn) )  {
				out = s;
				oplane = plp;
			}
		} else if ( dn > EPSILON )  {
			/* entry point, when dir.N > 0.  in = max(in,s) */
			if( in < (s = dxbdn/dn) )  {
				in = s;
				iplane = plp;
			}
		}  else  {
			/* ray is parallel to plane when dir.N == 0.
			 * If it is outside the solid, stop now */
			if( dxbdn > 0.0 )
				return( SEG_NULL );	/* MISS */
		}
		if( debug & DEBUG_ARB8 )
			fprintf(stderr,"arb: in=%f, out=%f\n", in, out);
		if( in > out )
			return( SEG_NULL );	/* MISS */
	}
	/* Validate */
	if( iplane == PLANE_NULL || oplane == PLANE_NULL )  {
		fprintf(stderr,"ERROR: arb(%s): 1 hit => MISS\n",
			stp->st_name);
		return( SEG_NULL );	/* MISS */
	}
	if( out < 0.0 || in >= out )
		return( SEG_NULL );	/* MISS */

	GET_SEG( segp );
	segp->seg_stp = stp;
	segp->seg_flag = SEG_IN|SEG_OUT;
	segp->seg_in.hit_dist = in;
	VJOIN1( segp->seg_in.hit_point, rp->r_pt, in, rp->r_dir );
	VMOVE( segp->seg_in.hit_normal, iplane->pl_N );

	segp->seg_out.hit_dist = out;
	VJOIN1( segp->seg_out.hit_point, rp->r_pt, out, rp->r_dir );
	VMOVE( segp->seg_out.hit_normal, oplane->pl_N );
	return(segp);			/* HIT */
}
