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
#include "db.h"
#include "debug.h"

/* Describe algorithm here */

#define MAXPTS	4			/* All we need are 4 points */
#define pl_A	pl_points[0]		/* Synonym for A point */

struct plane_specific  {
	int	pl_npts;		/* number of points on plane */
	point_t	pl_points[MAXPTS];	/* Actual points on plane */
	vect_t	pl_Xbasis;		/* X (B-A) vector (for 2d coords) */
	vect_t	pl_Ybasis;		/* Y (C-A) vector (for 2d coords) */
	vect_t	pl_N;			/* Unit-length Normal (outward) */
	fastf_t	pl_NdotA;		/* Normal dot A */
	fastf_t	pl_2d_x[MAXPTS];	/* X 2d-projection of points */
	fastf_t	pl_2d_y[MAXPTS];	/* Y 2d-projection of points */
	fastf_t	pl_2d_com[MAXPTS];	/* pre-computed common-term */
	struct plane_specific *pl_forw;	/* Forward link */
	char	pl_code[MAXPTS+1];	/* Face code string.  Decorative. */
};

#define MINMAX(a,b,c)	{ static float ftemp;\
			if( (ftemp = (c)) < (a) )  a = ftemp;\
			if( ftemp > (b) )  b = ftemp; }

arb8_prep( sp, stp, mat )
struct solidrec *sp;
struct soltab *stp;
matp_t mat;
{
	register float *op;		/* Used for scanning vectors */
	static float xmax, ymax, zmax;	/* For finding the bounding spheres */
	static float xmin, ymin, zmin;	/* For finding the bounding spheres */
	static fastf_t dx, dy, dz;	/* For finding the bounding spheres */
	static vect_t	work;		/* Vector addition work area */
	static vect_t	homog;		/* Vect/Homog.Vect conversion buf */
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
	 * Convert from vector to point notation IN PLACE in s_values[]
	 * by rotating vectors and adding base vector.
	 */
	VMOVE( homog, &sp->s_values[0] );		/* cvt to fastf_t */
	homog[3] = 1;					/* & to homog vec */
	matXvec( work, mat, homog );			/* 4x4: xlate, too */
	htov_move( homog, work );			/* divide out W */
	VMOVE( &sp->s_values[0], homog );		/* cvt to float */

	op = &sp->s_values[1*3];
	for( i=1; i<8; i++ )  {
		MAT3XVEC( homog, mat, op );		/* 3x3: rot only */
		VADD2( op, &sp->s_values[0], homog );
		op += 3;
	}

	/*
	 * Compute bounding sphere.
	 * Find min and max of the point co-ordinates
	 */
	op = &sp->s_values[0];
	for( i=0; i< 8; i++ ) {
		MINMAX( xmin, xmax, *op++ );
		MINMAX( ymin, ymax, *op++ );
		MINMAX( zmin, zmax, *op++ );
	}
	VSET( stp->st_center,
		(xmax + xmin)/2, (ymax + ymin)/2, (zmax + zmin)/2 );

	dx = (xmax - xmin)/2;
	dy = (ymax - ymin)/2;
	dz = (zmax - zmin)/2;
	stp->st_radsq = dx*dx + dy*dy + dz*dz;
	stp->st_specific = (int *) 0;

	faces = 0;
	faces += face( sp->s_values, stp, 3, 2, 1, 0 );	/* 1234 */
	faces += face( sp->s_values, stp, 4, 5, 6, 7 );	/* 8765 */
	faces += face( sp->s_values, stp, 4, 7, 3, 0 );	/* 1485 */
	faces += face( sp->s_values, stp, 2, 6, 5, 1 );	/* 2673 */
	faces += face( sp->s_values, stp, 1, 5, 4, 0 );	/* 1562 */
	faces += face( sp->s_values, stp, 7, 6, 2, 3 );	/* 4378 */
#ifdef reversed
	faces += face( sp->s_values, stp, 0, 1, 2, 3 );	/* 1234 */
	faces += face( sp->s_values, stp, 7, 6, 5, 4 );	/* 8765 */
	faces += face( sp->s_values, stp, 0, 3, 7, 4 );	/* 1485 */
	faces += face( sp->s_values, stp, 1, 5, 6, 2 );	/* 2673 */
	faces += face( sp->s_values, stp, 0, 4, 5, 1 );	/* 1562 */
	faces += face( sp->s_values, stp, 3, 2, 6, 7 );	/* 4378 */
#endif
	if( faces >= 4  && faces <= 6 )
		return(0);		/* OK */

	printf("arb8(%s):  only %d faces present\n", stp->st_name, faces);
	/* Should free storage for good faces */
	return(1);			/* Error */
}

/*static */
face( vects, stp, a, b, c, d )
register float vects[];
struct soltab *stp;
int a, b, c, d;
{
	register struct plane_specific *plp;
	register int pts;
	register int i;

	GETSTRUCT( plp, plane_specific );
	plp->pl_npts = 0;
	pts = add_pt( vects, stp, plp, a );
	pts += add_pt( vects, stp, plp, b );
	pts += add_pt( vects, stp, plp, c );
	pts += add_pt( vects, stp, plp, d );

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
	return(1);					/* OK */
}

#define VERT(x)	(&vects[(x)*3])

/*static */
add_pt( vects, stp, plp, a )
float *vects;
struct soltab *stp;
register struct plane_specific *plp;
int a;
{
	register int i;
	register float *point;
	static vect_t work;
	static vect_t P_A;		/* new point - A */
	static float f;

	point = VERT(a);

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
		f = VDOT( plp->pl_Xbasis, P_A ) -
			( MAGNITUDE(P_A) * MAGNITUDE(plp->pl_Xbasis) );
		if( NEAR_ZERO(f) )  {
			plp->pl_npts--;
			plp->pl_code[2] = '\0';
			return(0);			/* BAD */
		}
		VCROSS( plp->pl_N, plp->pl_Xbasis, P_A );
		VUNITIZE( plp->pl_N );

		/*
		 *  For some reason, some (but not all) of the normals
		 *  come out pointing inwards.  Rather than try to understand
		 *  this, I'm just KLUDGEING it for now, because we have
		 *  enough information to fix it up.  1000 pardons.
		 */
		VSUB2( work, plp->pl_A, stp->st_center );
		f = VDOT( work, plp->pl_N );
		if( f < 0.0 )  {
/**			printf("WARNING: arb8(%s) face %s has bad normal!  (A-cent).N=%f\n", stp->st_name, plp->pl_code, f); * */
			VREVERSE(plp->pl_N, plp->pl_N);	/* "fix" normal */
		}
		/* Generate an arbitrary Y basis perp to Xbasis & Normal */
		VCROSS( plp->pl_Ybasis, plp->pl_N, plp->pl_Xbasis );
		plp->pl_NdotA = VDOT( plp->pl_N, plp->pl_A );
		plp->pl_2d_x[2] = VDOT( P_A, plp->pl_Xbasis );
		plp->pl_2d_y[2] = VDOT( P_A, plp->pl_Ybasis );
		return(1);				/* OK */
	default:
		/* Merely validate 4th and subsequent points */
		VSUB2( P_A, point, plp->pl_A );
		f = VDOT( plp->pl_N, P_A );
		if( NEAR_ZERO(f) )  {
			plp->pl_2d_x[i] = VDOT( P_A, plp->pl_Xbasis );
			plp->pl_2d_y[i] = VDOT( P_A, plp->pl_Ybasis );
			return(1);			/* OK */
		}
		printf("ERROR: arb8(%s) face %s non-planar\n",
			stp->st_name, plp->pl_code );
		plp->pl_npts--;
		plp->pl_code[i] = '\0';
		return(0);				/* BAD */
	}
}

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

	in.hit_dist = out.hit_dist = -INFINITY;	/* 'way back behind eye */

	flags = 0;
	/* consider each face */
	for( ; plp; plp = plp->pl_forw )  {
		FAST fastf_t dn;		/* Direction dot Normal */
		static fastf_t k;	/* (NdotA - (N dot P))/ (N dot D) */
		FAST fastf_t f;
		/*
		 *  Ray Direction dot N.  (N is outward-pointing normal)
		 */
		dn = VDOT( plp->pl_N, rp->r_dir );
		if( debug & DEBUG_ARB8 )
			printf("Shooting at face %s.  N.Dir=%f\n", plp->pl_code, dn );
		if( NEAR_ZERO(dn) )
			continue;

		/* Compute distance along ray of intersection */
		k = (plp->pl_NdotA - VDOT(plp->pl_N, rp->r_pt)) / dn;

		if( dn < 0 )  {
			/* Entering solid */
			f = k - in.hit_dist;
			if( NEAR_ZERO( f ) )  {
				if( debug & DEBUG_ARB8)printf("skipping nearby entry surface, k=%f\n", k);
				continue;
			}
			if( pl_shot( plp, rp, &in, k ) != 0 )
				continue;
			if(debug&DEBUG_ARB8) printf("arb8: entry dist=%f, dn=%f, k=%f\n", in.hit_dist, dn, k );
			flags |= SEG_IN;
		} else {
			/* Exiting solid */
			f = k - out.hit_dist;
			if( NEAR_ZERO( f ) )  {
				if( debug & DEBUG_ARB8)printf("skipping nearby exit surface, k=%f\n", k);
				continue;
			}
			if( pl_shot( plp, rp, &out, k ) != 0 )
				continue;
			if(debug&DEBUG_ARB8) printf("arb8: exit dist=%f, dn=%f, k=%f\n", out.hit_dist, dn, k );
			flags |= SEG_OUT;
		}
	}
	if( flags == 0 )
		return(SEG_NULL);		/* MISS */

	if( flags == SEG_IN )  {
		/* it may start inside, but it can always leave */
		printf("Error: arb8(%s) ray never exited solid!\n",
			stp->st_name);
		return(SEG_NULL);		/* MISS */
	}

	/* SEG_OUT, or SEG_IN|SEG_OUT */
	GET_SEG(segp);
	segp->seg_stp = stp;
	segp->seg_flag = flags;
	segp->seg_in = in;		/* struct copy */
	segp->seg_out = out;		/* struct copy */
	return(segp);			/* HIT */
}

pl_shot( plp, rp, hitp, k )
register struct plane_specific *plp;
register struct ray *rp;
register struct hit *hitp;
double	k;			/* dist along ray */
{
	static vect_t	hit_pt;		/* ray hits solid here */
	static vect_t	work;
	FAST fastf_t	xt, yt;

	VCOMPOSE1( hit_pt, rp->r_pt, k, rp->r_dir );
	/* Project the hit point onto the plane, making this a 2-d problem */
	VSUB2( work, hit_pt, plp->pl_A );
	xt = VDOT( work, plp->pl_Xbasis );
	yt = VDOT( work, plp->pl_Ybasis );

	if( debug & DEBUG_ARB8 )  {
		printf("k = %f, xt,yt=(%f,%f), ", k, xt, yt );
		VPRINT("hit_pt", hit_pt);
	}
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
	if( debug & DEBUG_ARB8 )  printf("\t[Above was a hit]\n");
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
