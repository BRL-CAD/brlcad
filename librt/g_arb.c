/*
 *  			A R B 8 . C
 *  
 *  Function -
 *  	Intersect a ray with an Arbitrary Regular Polyhedron with
 *  	as many as 8 vertices.
 *  
 *  Contributors -
 *	Edwin O. Davisson	(Triangle & Intercept Analysis)
 *	Michael John Muuss	(Programming, Generalization)
 *	Douglas A. Gwyn		("Inside" routine)
 *
 * U. S. Army Ballistic Research Laboratory
 * April 18, 1984.
 *
 * $Revision$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "vmath.h"
#include "ray.h"
#include "debug.h"
#include "plane.h"

/* Describe algorithm here */

#define MINMAX(a,b,c)	{ FAST fastf_t ftemp;\
			if( (ftemp = (c)) < (a) )  a = ftemp;\
			if( ftemp > (b) )  b = ftemp; }

/*
 *  			A R B 8 _ P R E P
 */
arb8_prep( vec, stp, mat )
fastf_t *vec;
struct soltab *stp;
matp_t mat;
{
	register fastf_t *op;		/* Used for scanning vectors */
	static fastf_t xmax, ymax, zmax;/* For finding the bounding spheres */
	static fastf_t xmin, ymin, zmin;/* For finding the bounding spheres */
	static fastf_t dx, dy, dz;	/* For finding the bounding spheres */
	static vect_t	work;		/* Vector addition work area */
	static vect_t	sum;		/* Sum of all endpoints */
	static int	faces;		/* # of faces produced */
	static fastf_t	scale;		/* width across widest axis */
	static int	i;

	/* init maxima and minima */
	xmax = ymax = zmax = -INFINITY;
	xmin = ymin = zmin =  INFINITY;

	/*
	 * Process an ARB8, which is represented as a vector
	 * from the origin to the first point, and 7 vectors
	 * from the first point to the remaining points.
	 *
	 * Convert from vector to point notation IN PLACE
	 * by rotating vectors and adding base vector.
	 */
	vec[3] = 1;					/* cvt to homog vec */
	matXvec( work, mat, vec );			/* 4x4: xlate, too */
	htov_move( vec, work );				/* divide out W */

	op = &vec[1*ELEMENTS_PER_VECT];
	VMOVE( sum, vec );				/* sum=0th element */
	for( i=1; i<8; i++ )  {
		MAT3XVEC( work, mat, op );		/* 3x3: rot only */
		VADD2( op, &vec[0], work );
		VADD2( sum, sum, op );			/* build the sum */
		op += ELEMENTS_PER_VECT;
	}

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
		printf("arb8(%s):  only %d faces present\n",
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
		MINMAX( xmin, xmax, *op++ );
		MINMAX( ymin, ymax, *op++ );
		MINMAX( zmin, zmax, *op++ );
		op++;		/* Depends on ELEMENTS_PER_VECT */
	}
	VSET( stp->st_center,
		(xmax + xmin)/2, (ymax + ymin)/2, (zmax + zmin)/2 );

	dx = (xmax - xmin)/2;
	dy = (ymax - ymin)/2;
	dz = (zmax - zmin)/2;
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
	register struct plane_specific *plp;
	register int pts;
	register int i;

	GETSTRUCT( plp, plane_specific );
	plp->pl_npts = 0;
	pts  = add_pt( ap, stp, plp, a, noise );
	pts += add_pt( bp, stp, plp, b, noise );
	pts += add_pt( cp, stp, plp, c, noise );
	pts += add_pt( dp, stp, plp, d, noise );

	if( pts < 3 )  {
		free(plp);
		return(0);				/* BAD */
	}
	/* Make the 2d-point list contain the origin as start+end */
	plp->pl_2d_x[plp->pl_npts] = plp->pl_2d_y[plp->pl_npts] = 0.0;

	/* Compute the common sub-expression for inside() */
	for( i=0; i < pts; i++ )  {
		static fastf_t f;
		f = (plp->pl_2d_y[i+1] - plp->pl_2d_y[i]);
		if( NEAR_ZERO(f) )
			plp->pl_2d_com[i] = 0.0;	/* anything */
		else
			plp->pl_2d_com[i] =
				(plp->pl_2d_x[i+1] - plp->pl_2d_x[i]) / f;
	}

	/* Add this face onto the linked list for this solid */
	plp->pl_forw = (struct plane_specific *)stp->st_specific;
	stp->st_specific = (int *)plp;
	return(pts);					/* OK */
}

/*
 *			A D D _ P T
 *
 *  Add another point to a struct plane_specific, checking for unique pts.
 *  The first two points are easy.  The third one triggers most of the
 *  plane calculations, and forth and subsequent ones are merely
 *  check for validity.  If noise!=0, then non-planar 4th points give
 *  a warning message.  noise=1 for ARB8's, and noise=0 for ARS's.
 */
add_pt( point, stp, plp, a, noise )
register pointp_t point;
struct soltab *stp;
register struct plane_specific *plp;
int a;
int noise;			/* non-0: check 4,> pts for being planar */
{
	register int i;
	static vect_t work;
	static vect_t P_A;		/* new point - A */
	static fastf_t f;

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
		plp->pl_2d_x[0] = plp->pl_2d_y[0] = 0.0;
		return(1);				/* OK */
	case 1:
		VSUB2( plp->pl_Xbasis, point, plp->pl_A );	/* B-A */
		plp->pl_2d_x[1] = VDOT( plp->pl_Xbasis, plp->pl_Xbasis );
		plp->pl_2d_y[1] = 0.0;
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
		/* Generate an arbitrary Y basis perp to Xbasis & Normal */
		VCROSS( plp->pl_Ybasis, plp->pl_Xbasis, plp->pl_N );
		plp->pl_NdotA = VDOT( plp->pl_N, plp->pl_A );
		plp->pl_2d_x[2] = VDOT( P_A, plp->pl_Xbasis );
		plp->pl_2d_y[2] = VDOT( P_A, plp->pl_Ybasis );
		return(1);				/* OK */
	default:
		/* Merely validate 4th and subsequent points */
		VSUB2( P_A, point, plp->pl_A );
		/* Project into 2-d, even if non-planar */
		plp->pl_2d_x[i] = VDOT( P_A, plp->pl_Xbasis );
		plp->pl_2d_y[i] = VDOT( P_A, plp->pl_Ybasis );
		f = VDOT( plp->pl_N, P_A );
		if( ! NEAR_ZERO(f) )  {
			/* Non-planar face */
			if( noise )  {
				printf("ERROR: arb8(%s) face %s non-planar\n",
				stp->st_name, plp->pl_code );
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
 *  			A R B 8 _ P R I N T
 */
arb8_print( stp )
register struct soltab *stp;
{
	register struct plane_specific *plp =
		(struct plane_specific *)stp->st_specific;
	register int i;

	if( plp == (struct plane_specific *)0 )  {
		printf("arb8(%s):  no faces\n", stp->st_name);
		return;
	}
	do {
		printf( "......Face %s\n", plp->pl_code );
		printf( "%d vertices:\n", plp->pl_npts );
		for( i=0; i < plp->pl_npts; i++ )  {
			VPRINT( "", plp->pl_points[i] );
		}
		VPRINT( "Xbasis", plp->pl_Xbasis );
		VPRINT( "Ybasis", plp->pl_Ybasis );
		VPRINT( "Normal", plp->pl_N );
		printf( "N.A = %f\n", plp->pl_NdotA );
		printf( "2-d projection of vertices:\n");
		for( i=0; i < plp->pl_npts; i++ )  {
			printf( "(%f,%f), ",
				plp->pl_2d_x[i],
				plp->pl_2d_y[i] );
		}
		putchar('\n');
	} while( plp = plp->pl_forw );
}

/*
 *			A R B 8 _ S H O T
 *  
 * Function -
 *	Shoot a ray at an ARB8.
 *  
 * Returns -
 *	0	MISS
 *  	segp	HIT
 */
struct seg *
arb8_shot( stp, rp )
struct soltab *stp;
register struct ray *rp;
{
	register struct plane_specific *plp =
		(struct plane_specific *)stp->st_specific;
	register struct seg *segp;
	static struct hit in, out;
	static int flags;
	static vect_t	hit_pt;		/* ray hits solid here */
	static vect_t	work;
	static fastf_t	xt, yt;

	in.hit_dist = out.hit_dist = -INFINITY;	/* 'way back behind eye */

	flags = 0;
	/* consider each face */
	for( ; plp; plp = plp->pl_forw )  {
		FAST fastf_t dn;		/* Direction dot Normal */
		FAST fastf_t k;		/* (NdotA - (N dot P))/ (N dot D) */
		FAST fastf_t f;
		/*
		 *  Ray Direction dot N.  (N is outward-pointing normal)
		 */
		dn = VDOT( plp->pl_N, rp->r_dir );
		if( debug & DEBUG_ARB8 )
			printf("Shooting at face %s.  N.Dir=%f\n", plp->pl_code, dn );
		/*
		 *  Unless *exactly* along face, need to compute this anyways,
		 *  because other (entrance/exit) point is probably worth it.
		 */
		if( dn == 0.0 )
			continue;

		/* Compute distance along ray of intersection */
		k = (plp->pl_NdotA - VDOT(plp->pl_N, rp->r_pt)) / dn;

		if( dn < 0 )  {
			/* Entering solid */
			if( flags & SEG_IN )  {
				if( debug & DEBUG_ARB8)printf("skipping nearby entry surface, k=%f\n", k);
				continue;
			}
			/* if( pl_shot( plp, rp, &in, k ) != 0 ) continue; */
			VCOMPOSE1( hit_pt, rp->r_pt, k, rp->r_dir );
			VSUB2( work, hit_pt, plp->pl_A );
			xt = VDOT( work, plp->pl_Xbasis );
			yt = VDOT( work, plp->pl_Ybasis );
			if( !inside(
				&xt, &yt,
				plp->pl_2d_x, plp->pl_2d_y, plp->pl_2d_com,
				plp->pl_npts )
			)
				continue;			/* MISS */

			/* HIT is within planar face */
			in.hit_dist = k;
			VMOVE( in.hit_point, hit_pt );
			VMOVE( in.hit_normal, plp->pl_N );
			if(debug&DEBUG_ARB8) printf("arb8: entry dist=%f, dn=%f, k=%f\n", in.hit_dist, dn, k );
			flags |= SEG_IN;
		} else {
			/* Exiting solid */
			if( flags & SEG_OUT )  {
				if( debug & DEBUG_ARB8)printf("skipping nearby exit surface, k=%f\n", k);
				continue;
			}
			/* if( pl_shot( plp, rp, &out, k ) != 0 ) continue; */
			VCOMPOSE1( hit_pt, rp->r_pt, k, rp->r_dir );
			VSUB2( work, hit_pt, plp->pl_A );
			xt = VDOT( work, plp->pl_Xbasis );
			yt = VDOT( work, plp->pl_Ybasis );
			if( !inside(
				&xt, &yt,
				plp->pl_2d_x, plp->pl_2d_y, plp->pl_2d_com,
				plp->pl_npts )
			)
				continue;			/* MISS */

			/* HIT is within planar face */
			out.hit_dist = k;
			VMOVE( out.hit_point, hit_pt );
			VMOVE( out.hit_normal, plp->pl_N );
			if(debug&DEBUG_ARB8) printf("arb8: exit dist=%f, dn=%f, k=%f\n", out.hit_dist, dn, k );
			flags |= SEG_OUT;
		}
	}
	if( flags == 0 )
		return(SEG_NULL);		/* MISS */

	if( flags == SEG_IN )  {
		/* It can start inside, but it should always leave.
		 * If this condition exists, it is almost certainly due to
		 * the dn==0 check above.  Thus, we will make the
		 * surface infinitely thin and just replicate the entry
		 * point as the exit point.  This at least makes the
		 * presence of this solid known.  There may be something
		 * better we can do.
		 */
		out.hit_dist = in.hit_dist;
		VMOVE( out.hit_point, in.hit_point );
		VREVERSE( out.hit_normal, in.hit_point );
		flags |= SEG_OUT;
	}

	if( flags == SEG_OUT )  {
		VSET( in.hit_point, 0, 0, 0 );
		VSET( in.hit_normal, 0, 0, 0 );
	}
	/* SEG_OUT, or SEG_IN|SEG_OUT */
	GET_SEG(segp);
	segp->seg_stp = stp;
	segp->seg_flag = flags;
	segp->seg_in = in;		/* struct copy */
	segp->seg_out = out;		/* struct copy */
	return(segp);			/* HIT */
}

/*
 *  			P L _ S H O T
 *
 *  This routine has been expanded in-line, but is preserved here for
 *  study, and possible re-use elsewhere.
 */
pl_shot( plp, rp, hitp, k )
register struct plane_specific *plp;
register struct ray *rp;
register struct hit *hitp;
double	k;			/* dist along ray */
{
	static vect_t	hit_pt;		/* ray hits solid here */
	static vect_t	work;
	static fastf_t	xt, yt;

	VCOMPOSE1( hit_pt, rp->r_pt, k, rp->r_dir );
	/* Project the hit point onto the plane, making this a 2-d problem */
	VSUB2( work, hit_pt, plp->pl_A );
	xt = VDOT( work, plp->pl_Xbasis );
	yt = VDOT( work, plp->pl_Ybasis );

	if( !inside(
		&xt, &yt,
		plp->pl_2d_x, plp->pl_2d_y, plp->pl_2d_com,
		plp->pl_npts )
	)
		return(1);			/* MISS */

	/* Hit is within planar face */
	hitp->hit_dist = k;
	VMOVE( hitp->hit_point, hit_pt );
	VMOVE( hitp->hit_normal, plp->pl_N );
	return(0);				/* HIT */
}

/*
 *  			I N S I D E
 *  
 * Function -
 *  	Determines whether test point (xt,yt) is inside the polygon
 *  	whose vertices have coordinates (in cyclic order) of
 *  	(x[i],y[i]) for i = 0 to n-1.
 *  
 * Returns -
 *  	1	iff test point is inside polygon
 *  	0	iff test point is outside polygon
 *  
 * Method -
 *  	A horizontal line through the test point intersects an even
 *  	number of the polygon's sides to the right of the point
 *  	IFF the point is exterior to the polygon (Jordan Curve Theorem).
 *  
 * Note -
 *  	For speed, we assume that x[n] == x[0], y[n] == y[0],
 *  	ie, that the starting point is repeated as the ending point.
 *
 * Credit -
 *	Douglas A. Gwyn (Algorithm, original FORTRAN subroutine)
 *	Michael Muuss (This "C" routine)
 */
inside( xt, yt, x, y, com, n )
register fastf_t *xt, *yt;
register fastf_t *x, *y, *com;
int n;
{
	register fastf_t *xend;
	static int ret;

	/*
	 * Starts with 0 intersections, an even number ==> outside.
	 * Proceed around the polygon, testing each side for intersection.
	 */
	xend = &x[n];
	for( ret=0; x < xend; x++,y++,com++ )  {
		/* See if edge is crossed by horiz line through test point */
		if( (*yt > *y || *yt <= y[1])  &&  (*yt <= *y || *yt > y[1]) )
			continue;
		/* Yes.  See if intersection is to the right of test point */
		if( (*xt - *x) < (*yt-*y) * (*com) )
			ret = !ret;
	}
	return(ret);
}
