/*
 *  			A R B 8 . C
 *  
 *  Function -
 *  	Intersect a ray with an Arbitrary Regular Polyhedron with
 *  	as many as 8 vertices.
 *  
 *  Authors -
 *	Edwin O. Davisson	(Analysis)
 *	Michael John Muuss	(Programming)
 *
 * U. S. Army Ballistic Research Laboratory
 * March 29, 1984.
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

struct triangle_specific  {
	struct triangle_specific *tri_forw;	/* Forward link */
	vect_t	tri_A;		/* A, temp */
	vect_t	tri_B;		/* B, temp */
	vect_t	tri_C;		/* C, temp */
	vect_t	tri_AxB;	/* VCROSS( A, B ) */
	vect_t	tri_BxC;	/* VCROSS( B, C ) */
	vect_t	tri_CxA;	/* VCROSS( C, A ) */
	vect_t	tri_Q;		/* (B-A) cross (C-A) */
	vect_t	tri_N;		/* Unit-length Normal (from Q) */
	float	tri_vol;	/* Q dot A */
	char	tri_code[4];	/* face code string.  Decorative. */
};

#define MINMAX(a,b,c)	{ register float ftemp;\
			if( (ftemp = (c)) < (a) )  a = ftemp;\
			if( ftemp > (b) )  b = ftemp; }

static struct triangle_specific *facet();

arb8_prep( sp, stp, mat )
struct solidrec *sp;
struct soltab *stp;
matp_t mat;
{
	register float *op;		/* Used for scanning vectors */
	static float xmax, ymax, zmax;	/* For finding the bounding spheres */
	static float xmin, ymin, zmin;	/* For finding the bounding spheres */
	static float dx, dy, dz;	/* For finding the bounding spheres */
	static vect_t	work;		/* Vector addition work area */
	static vect_t	homog;		/* Vect/Homog.Vect conversion buf */
	static int	facets;		/* # of facets produced */
	static float	scale;		/* width across widest axis */
	static int	i;

	/* init maxima and minima */
	xmax = ymax = zmax = -100000000.0;
	xmin = ymin = zmin =  100000000.0;

	/*
	 * Process an ARB8, which is represented as a vector
	 * from the origin to the first point, and 7 vectors
	 * from the first point to the remaining points.
	 *
	 * Convert from vector to point notation in place
	 * by rotating vectors and adding base vector.
	 */
	vtoh_move( homog, sp->s_values );
	matXvec( work, mat, homog );
	htov_move( sp->s_values, work );		/* divide out W */

	op = &sp->s_values[1*3];
	for( i=1; i<8; i++ )  {
		MAT3XVEC( homog, mat, op );
		VADD2( op, sp->s_values, homog );
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

	facets = 0;
	facets += face( sp->s_values, stp, 3, 2, 1, 0 );	/* 1234 */
	facets += face( sp->s_values, stp, 4, 5, 6, 7 );	/* 8765 */
	facets += face( sp->s_values, stp, 4, 7, 3, 0 );	/* 1485 */
	facets += face( sp->s_values, stp, 2, 6, 5, 1 );	/* 2673 */
	facets += face( sp->s_values, stp, 1, 5, 4, 0 );	/* 1562 */
	facets += face( sp->s_values, stp, 7, 6, 2, 3 );	/* 4378 */
#ifdef reversed
	facets += face( sp->s_values, stp, 0, 1, 2, 3 );	/* 1234 */
	facets += face( sp->s_values, stp, 7, 6, 5, 4 );	/* 8765 */
	facets += face( sp->s_values, stp, 0, 3, 7, 4 );	/* 1485 */
	facets += face( sp->s_values, stp, 1, 5, 6, 2 );	/* 2673 */
	facets += face( sp->s_values, stp, 0, 4, 5, 1 );	/* 1562 */
	facets += face( sp->s_values, stp, 3, 2, 6, 7 );	/* 4378 */
#endif
	if( facets >= 4  && facets <= 12 )
		return(0);		/* OK */

	printf("arb8(%s):  only %d facets present\n", stp->st_name, facets);
	/* Should free storage for good facets */
	return(1);			/* Error */
}

#define VERT(x)	(&vects[(x)*3])

static
face( vects, stp, a, b, c, d )
register float vects[];
struct soltab *stp;
int a, b, c, d;
{
	register struct triangle_specific *trip1;
	register struct triangle_specific *trip2;
	static vect_t X_A;
	static float f;

if(debug&DEBUG_ARB8)printf("face %d%d%d%d\n", a, b, c, d );
	trip1 = facet( vects, stp, a, b, c );
	trip2 = facet( vects, stp, a, c, d );

if(debug&DEBUG_ARB8)printf("face values x%x x%x\n", trip1, trip2);
	if( trip1 && trip2 )  {
		/*
		 *  If both facets exist, check to see if face is planar.
		 *  Check [ (B-A)x(C-A) ] dot (D-A) == 0
		 */
		VSUB2( X_A, VERT(d), VERT(a) );
		f = VDOT( trip1->tri_Q, X_A );
		if( NEAR_ZERO(f) )
			return(2);		/* OK */
		printf("arb8(%s):  face %d,%d,%d,%d non-planar (dot=%f)\n",
			stp->st_name, a,b,c,d, f);
		return(0);			/* BAD */
	}
	if( trip1 || trip2 )
		return(1);			/* OK */
	return(0);				/* BAD */
}

static struct triangle_specific *
facet( vects, stp, a, b, c )
register float *vects;
struct soltab *stp;
int a, b, c;
{
	register struct triangle_specific *trip;
	static vect_t B_A;		/* B - A */
	static vect_t C_A;		/* C - A */
	static vect_t B_C;		/* B - C */
	static float scale;		/* for scaling normal vector */

	VSUB2( B_A, VERT(b), VERT(a) );
	VSUB2( C_A, VERT(c), VERT(a) );
	VSUB2( B_C, VERT(b), VERT(c) );

	/* If points are coincident, ignore facet */
	if( MAGSQ( B_A ) < EPSILON )  return(0);
	if( MAGSQ( C_A ) < EPSILON )  return(0);
	if( MAGSQ( B_C ) < EPSILON )  return(0);

	GETSTRUCT( trip, triangle_specific );

	VMOVE( trip->tri_A, VERT(a) );	/* Temp */
	VMOVE( trip->tri_B, VERT(b) );
	VMOVE( trip->tri_C, VERT(c) );
	trip->tri_code[0] = '0'+a;
	trip->tri_code[1] = '0'+b;
	trip->tri_code[2] = '0'+c;
	trip->tri_code[3] = '\0';

	VCROSS( trip->tri_AxB, VERT(a), VERT(b) );
	VCROSS( trip->tri_BxC, VERT(b), VERT(c) );
	VCROSS( trip->tri_CxA, VERT(c), VERT(a) );
	VCROSS( trip->tri_Q, B_A, C_A );
	trip->tri_vol = VDOT( trip->tri_Q, VERT(a) );
	if( NEAR_ZERO(trip->tri_vol) )  {
		printf("arb8(%s): Zero volume (%f), facet %s dropped\n",
			stp->st_name, trip->tri_vol, trip->tri_code );
		free(trip);
		return(0);		/* FAIL */
	}

	/* Compute Normal with unit length, and outward direction */
	scale = 1.0 / MAGNITUDE( trip->tri_Q );
	VSCALE( trip->tri_N, trip->tri_Q, scale );

	/* Add to linked list */
	trip->tri_forw = (struct triangle_specific *)stp->st_specific;
	stp->st_specific = (int *)trip;

	return(trip);
}

arb8_print( stp )
register struct soltab *stp;
{
	register struct triangle_specific *trip =
		(struct triangle_specific *)stp->st_specific;

	if( trip == (struct triangle_specific *)0 )  {
		printf("arb8(%s):  no facets\n", stp->st_name);
		return;
	}
	do {
		printf( "......Facet %s\n", trip->tri_code );
		VPRINT( "A", trip->tri_A );
		VPRINT( "B", trip->tri_B );
		VPRINT( "C", trip->tri_C );
		VPRINT( "AxB", trip->tri_AxB );
		VPRINT( "BxC", trip->tri_BxC );
		VPRINT( "CxA", trip->tri_CxA );
		VPRINT( "Q", trip->tri_Q );
		printf( "Vol=Q.A=%f\n", trip->tri_vol );
		VPRINT( "Normal", trip->tri_N );
	} while( trip = trip->tri_forw );
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
struct ray *rp;
{
	register struct triangle_specific *trip =
		(struct triangle_specific *)stp->st_specific;
	register struct seg *segp;
	static struct hit in, out;
	static int flags;

	in.hit_dist = out.hit_dist = -1000000;	/* 'way back behind eye */

	flags = 0;
	for( ; trip; trip = trip->tri_forw )  {
		register float dq;	/* D dot Q */
		static float k;		/* (vol - (Q dot P))/ (Q dot D) */
		/*
		 *  Ray Direction dot Q.  (Q is outward-pointing normal)
		 */
		dq = VDOT( trip->tri_Q, rp->r_dir );
		if( debug & DEBUG_ARB8 )
			printf("Shooting at face %s.  Q.D=%f\n", trip->tri_code, dq );
		if( NEAR_ZERO(dq) )
			continue;

		/* Compute distance along ray of intersection */
		k = (trip->tri_vol - VDOT(trip->tri_Q, rp->r_pt)) / dq;

		if( dq < 0 )  {
			/* Entering solid */
			if( NEAR_ZERO( k - in.hit_dist ) )  {
				if( debug & DEBUG_ARB8)printf("skipping nearby entry surface, k=%f\n", k);
				continue;
			}
			if( tri_shot( trip, rp, &in, k ) != 0 )
				continue;
			if(debug&DEBUG_ARB8) printf("arb8: entry dist=%f, dq=%f, k=%f\n", in.hit_dist, dq, k );
			flags |= SEG_IN;
		} else {
			/* Exiting solid */
			if( NEAR_ZERO( k - out.hit_dist ) )  {
				if( debug & DEBUG_ARB8)printf("skipping nearby exit surface, k=%f\n", k);
				continue;
			}
			if( tri_shot( trip, rp, &out, k ) != 0 )
				continue;
			if(debug&DEBUG_ARB8) printf("arb8: exit dist=%f, dq=%f, k=%f\n", out.hit_dist, dq, k );
			flags |= SEG_OUT;
		}
	}
	if( flags == 0 )
		return(SEG_NULL);		/* MISS */

	GETSTRUCT(segp, seg);
	segp->seg_stp = stp;
	segp->seg_flag = flags;
	segp->seg_in = in;
	segp->seg_out = out;
	return(segp);			/* HIT */
}

tri_shot( trip, rp, hitp, k )
register struct triangle_specific *trip;
register struct ray *rp;
register struct hit *hitp;
register float	k;			/* (v - (Q dot P))/ (Q dot D) */
{
	static float	av, bv, cv;	/* coeff's of linear combination */
	static vect_t	hit_pt;		/* ray hits solid here */

	VCOMPOSE1( hit_pt, rp->r_pt, k, rp->r_dir );

	av = VDOT( hit_pt, trip->tri_BxC );
	bv = VDOT( hit_pt, trip->tri_CxA );
	cv = VDOT( hit_pt, trip->tri_AxB );
	if( debug & DEBUG_ARB8 )  {
		printf("k = %f,  ", k );
		VPRINT("hit_pt", hit_pt);
		printf("av=%f, bv=%f, cv=%f\n", av, bv, cv);
	}

	{
		register float f;		/* XXX */
		f = av + bv + cv - trip->tri_vol;
		if( !NEAR_ZERO(f) )
			return(1);		/* MISS */
	}
	if( av < 0.0 )  {
		if( bv > 0.0 || cv > 0.0 )
			return(1);		/* MISS */
	} else if( av > 0.0 )  {
		if( bv < 0.0 || cv < 0.0 )
			return(1);		/* MISS */
	} else {
		/* av == 0.0 */
		if( bv < 0.0 )  {
			if( cv > 0.0 )
				return(1);	/* MISS */
		} else if( bv > 0.0 )  {
			if( cv < 0.0 )
				return(1);	/* MISS */
		} /* av == bv == 0.0 */
	}

	/* Hit is within the triangle */
	hitp->hit_dist = k;
	VMOVE( hitp->hit_point, hit_pt );
	VMOVE( hitp->hit_normal, trip->tri_N );
	if( debug & DEBUG_ARB8 )  printf("\t[Above was a hit]\n");
	return(0);				/* HIT */
}
