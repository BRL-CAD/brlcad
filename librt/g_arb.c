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
static char RCSarb[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "db.h"
#include "./debug.h"


/*
 *			Ray/ARB Intersection
 *
 *  An ARB is a convex volume bounded by 4 (pyramid), 5 (wedge), or 6 (box)
 *  planes.  This analysis depends on the properties of objects with convex
 *  hulls.  Let the ray in question be defined such that any point X on the
 *  ray may be expressed as X = P + k D.  Intersect the ray with each of the
 *  planes bounding the ARB as discussed above, and record the values of the
 *  parametric distance k along the ray.
 *
 *  With outward pointing normal vectors,
 *  note that the ray enters the half-space defined by a plane when D cdot N <
 *  0, is parallel to the plane when D cdot N = 0, and exits otherwise.  Find
 *  the entry point farthest away from the starting point bold P, i.e.  it has
 *  the largest value of k among the entry points.
 *  The ray enters the solid at this point.
 *  Similarly, find the exit point closest to point P, i.e. it has
 *  the smallest value of k among the exit points.  The ray exits the solid
 *  here.
 */

/* These hold temp values for the face being prep'ed */
#define ARB_MAXPTS	4		/* All we need are 4 points */
static point_t	arb_points[ARB_MAXPTS];	/* Actual points on plane */
static int	arb_npts;		/* number of points on plane */

/* One of these for each face */
struct arb_specific  {
	float	arb_A[3];		/* "A" point */
	float	arb_N[3];		/* Unit-length Normal (outward) */
	float	arb_NdotA;		/* Normal dot A */
	float	arb_UVorig[3];		/* origin of UV coord system */
	float	arb_U[3];		/* unit U vector (along B-A) */
	float	arb_V[3];		/* unit V vector (perp to N and U) */
	float	arb_Ulen;		/* length of U basis (for du) */
	float	arb_Vlen;		/* length of V basis (for dv) */
	struct arb_specific *arb_forw;	/* Forward link */
};
#define ARB_NULL	((struct arb_specific *)0)

HIDDEN int	arb_face(), arb_add_pt();

static struct arb_specific *FreeArb;	/* Head of free list */

/* Layout of arb in input record */
static struct arb_info {
	char	*ai_title;
	int	ai_sub[4];
} arb_info[6] = {
	{ "1234", 3, 2, 1, 0 },
	{ "8765", 4, 5, 6, 7 },
	{ "1485", 4, 7, 3, 0 },
	{ "2673", 2, 6, 5, 1 },
	{ "1562", 1, 5, 4, 0 },
	{ "4378", 7, 6, 2, 3 }
};

HIDDEN void
arb_getarb()
{
	register struct arb_specific *arbp;
	register int bytes;

	bytes = rt_byte_roundup(64*sizeof(struct arb_specific));
	arbp = (struct arb_specific *)rt_malloc(bytes, "get_arb");
	while( bytes >= sizeof(struct arb_specific) )  {
		arbp->arb_forw = FreeArb;
		FreeArb = arbp++;
		bytes -= sizeof(struct arb_specific);
	}
}

/*
 *  			A R B _ P R E P
 */
int
arb_prep( vec, stp, mat, rtip )
fastf_t		*vec;
struct soltab	*stp;
matp_t		mat;
struct rt_i	*rtip;
{
	register fastf_t *op;		/* Used for scanning vectors */
	LOCAL vect_t	work;		/* Vector addition work area */
	LOCAL vect_t	sum;		/* Sum of all endpoints */
	LOCAL int	faces;		/* # of faces produced */
	register int	i;
	register int	j;
	register int	k;
	LOCAL fastf_t	f;
	register struct arb_specific *arbp;

	/*
	 * Process an ARB8, which is represented as a vector
	 * from the origin to the first point, and 7 vectors
	 * from the first point to the remaining points.
	 *
	 * Convert from vector to point notation IN PLACE
	 * by rotating vectors and adding base vector.
	 */
	VSETALL( sum, 0 );
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

	faces = 0;
	for( i=0; i<6; i++ )  {
		while( (arbp=FreeArb) == ARB_NULL )  arb_getarb();
		FreeArb = arbp->arb_forw;

		arb_npts = 0;
		for( j=0; j<4; j++ )  {
			register pointp_t point;

			point = &vec[arb_info[i].ai_sub[j]*ELEMENTS_PER_VECT];

			/* Verify that this point is not the same
			 * as an earlier point
			 */
			for( k=0; k < arb_npts; k++ )  {
				VSUB2( work, point, arb_points[k] );
				if( MAGSQ( work ) < 0.005 )  {
					/* the same -- skip it */
					goto next_pt;
				}
			}
			VMOVE( arb_points[arb_npts], point );

			arb_add_pt(
				point,
				stp, arbp, arb_info[i].ai_title );
next_pt:		;
		}

		if( arb_npts < 3 )  {
			/* This face is BAD */
			arbp->arb_forw = FreeArb;
			FreeArb = arbp;
			continue;
		}

		/* Scale U and V basis vectors by the inverse of Ulen and Vlen */
		arbp->arb_Ulen = 1.0 / arbp->arb_Ulen;
		arbp->arb_Vlen = 1.0 / arbp->arb_Vlen;
		VSCALE( arbp->arb_U, arbp->arb_U, arbp->arb_Ulen );
		VSCALE( arbp->arb_V, arbp->arb_V, arbp->arb_Vlen );

		/* Add this face onto the linked list for this solid */
		arbp->arb_forw = (struct arb_specific *)stp->st_specific;
		stp->st_specific = (int *)arbp;

		faces++;
	}
	if( faces < 4  || faces > 6 )  {
		rt_log("arb(%s):  only %d faces present\n",
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
		VMINMAX( stp->st_min, stp->st_max, op );
		op += ELEMENTS_PER_VECT;
	}
	VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
	VSUB2SCALE( work, stp->st_max, stp->st_min, 0.5 );

	f = work[X];
	if( work[Y] > f )  f = work[Y];
	if( work[Z] > f )  f = work[Z];
	stp->st_aradius = f;
	stp->st_bradius = MAGNITUDE(work);
	return(0);		/* OK */
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
arb_add_pt( point, stp, arbp, title )
register pointp_t point;
struct soltab *stp;
register struct arb_specific *arbp;
char	*title;
{
	register int i;
	LOCAL vect_t work;
	LOCAL vect_t P_A;		/* new point - A */
	FAST fastf_t f;

	i = arb_npts++;		/* Current point number */

	/* The first 3 points are treated differently */
	switch( i )  {
	case 0:
		VMOVE( arbp->arb_A, point );
		VMOVE( arbp->arb_UVorig, point );
		return(1);				/* OK */
	case 1:
		VSUB2( arbp->arb_U, point, arbp->arb_A );	/* B-A */
		f = MAGNITUDE( arbp->arb_U );
		arbp->arb_Ulen = f;
		f = 1/f;
		VSCALE( arbp->arb_U, arbp->arb_U, f );
		return(1);				/* OK */
	case 2:
		VSUB2( P_A, point, arbp->arb_A );	/* C-A */
		/* Check for co-linear, ie, (B-A)x(C-A) == 0 */
		VCROSS( arbp->arb_N, arbp->arb_U, P_A );
		f = MAGNITUDE( arbp->arb_N );
		if( NEAR_ZERO(f,0.005) )  {
			arb_npts--;
			return(0);			/* BAD */
		}
		f = 1/f;
		VSCALE( arbp->arb_N, arbp->arb_N, f );

		/*
		 * Get vector perp. to AB in face of plane ABC.
		 * Scale by projection of AC, make this V.
		 */
		VCROSS( work, arbp->arb_N, arbp->arb_U );
		VUNITIZE( work );
		f = VDOT( work, P_A );
		VSCALE( arbp->arb_V, work, f );
		f = MAGNITUDE( arbp->arb_V );
		arbp->arb_Vlen = f;
		f = 1/f;
		VSCALE( arbp->arb_V, arbp->arb_V, f );

		/* Check for new Ulen */
		VSUB2( P_A, point, arbp->arb_UVorig );
		f = VDOT( P_A, arbp->arb_U );
		if( f > arbp->arb_Ulen ) {
			arbp->arb_Ulen = f;
		} else if( f < 0.0 ) {
			VJOIN1( arbp->arb_UVorig, arbp->arb_UVorig, f, arbp->arb_U );
			arbp->arb_Ulen += (-f);
		}

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
		VSUB2( P_A, point, arbp->arb_UVorig );
		/* Check for new Ulen, Vlen */
		f = VDOT( P_A, arbp->arb_U );
		if( f > arbp->arb_Ulen ) {
			arbp->arb_Ulen = f;
		} else if( f < 0.0 ) {
			VJOIN1( arbp->arb_UVorig, arbp->arb_UVorig, f, arbp->arb_U );
			arbp->arb_Ulen += (-f);
		}
		f = VDOT( P_A, arbp->arb_V );
		if( f > arbp->arb_Vlen ) {
			arbp->arb_Vlen = f;
		} else if( f < 0.0 ) {
			VJOIN1( arbp->arb_UVorig, arbp->arb_UVorig, f, arbp->arb_V );
			arbp->arb_Vlen += (-f);
		}

		VSUB2( P_A, point, arbp->arb_A );
		VUNITIZE( P_A );		/* Checking direction only */
		f = VDOT( arbp->arb_N, P_A );
		if( ! NEAR_ZERO(f,0.005) )  {
			/* Non-planar face */
			rt_log("arb(%s): face %s non-planar, dot=%g\n",
				stp->st_name, title, f );
#ifdef CONSERVATIVE
			arb_npts--;
			return(0);				/* BAD */
#endif
		}
		return(1);			/* OK */
	}
}

/*
 *  			A R B _ P R I N T
 */
void
arb_print( stp )
register struct soltab *stp;
{
	register struct arb_specific *arbp =
		(struct arb_specific *)stp->st_specific;
	register int i;

	if( arbp == (struct arb_specific *)0 )  {
		rt_log("arb(%s):  no faces\n", stp->st_name);
		return;
	}
	do {
		VPRINT( "A", arbp->arb_A );
		VPRINT( "Normal", arbp->arb_N );
		rt_log( "N.A = %g\n", arbp->arb_NdotA );
		VPRINT( "UVorig", arbp->arb_UVorig );
		VPRINT( "U", arbp->arb_U );
		VPRINT( "V", arbp->arb_V );
		rt_log( "Ulen = %g, Vlen = %g\n",
			arbp->arb_Ulen, arbp->arb_Vlen);
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
arb_shot( stp, rp, ap )
struct soltab *stp;
register struct xray *rp;
struct application	*ap;
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
		if( (dn = -VDOT( arbp->arb_N, rp->r_dir )) < -SQRT_SMALL_FASTF )  {
			/* exit point, when dir.N < 0.  out = min(out,s) */
			if( out > (s = dxbdn/dn) )  {
				out = s;
				oplane = arbp;
			}
		} else if ( dn > SQRT_SMALL_FASTF )  {
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
		if( rt_g.debug & DEBUG_ARB8 )
			rt_log("arb: in=%g, out=%g\n", in, out);
		if( in > out )
			return( SEG_NULL );	/* MISS */
	}
	/* Validate */
	if( iplane == ARB_NULL || oplane == ARB_NULL )  {
		rt_log("arb_shoot(%s): 1 hit => MISS\n",
			stp->st_name);
		return( SEG_NULL );	/* MISS */
	}
	if( in >= out || out >= INFINITY )
		return( SEG_NULL );	/* MISS */

	{
		register struct seg *segp;

		GET_SEG( segp, ap->a_resource );
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
void
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
 *			A R B _ C U R V E
 *
 *  Return the "curvature" of the ARB face.
 *  Pick a principle direction orthogonal to normal, and 
 *  indicate no curvature.
 */
void
arb_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit *hitp;
struct soltab *stp;
{
	register struct arb_specific *arbp =
		(struct arb_specific *)hitp->hit_private;

	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
	cvp->crv_c1 = cvp->crv_c2 = 0;
}

/*
 *  			A R B _ U V
 *  
 *  For a hit on a face of an ARB, return the (u,v) coordinates
 *  of the hit point.  0 <= u,v <= 1.
 *  u extends along the arb_U direction defined by B-A,
 *  v extends along the arb_V direction defined by Nx(B-A).
 */
void
arb_uv( ap, stp, hitp, uvp )
struct application *ap;
struct soltab *stp;
register struct hit *hitp;
register struct uvcoord *uvp;
{
	register struct arb_specific *arbp =
		(struct arb_specific *)hitp->hit_private;
	LOCAL vect_t P_A;
	LOCAL fastf_t r;

	VSUB2( P_A, hitp->hit_point, arbp->arb_UVorig );
	/* Flipping v is an artifact of how the faces are built */
	uvp->uv_u = VDOT( P_A, arbp->arb_U );
	uvp->uv_v = 1.0 - VDOT( P_A, arbp->arb_V );
	if( uvp->uv_u < 0 || uvp->uv_v < 0 || uvp->uv_u > 1 || uvp->uv_v > 1 )  {
		rt_log("arb_uv: bad uv=%g,%g\n", uvp->uv_u, uvp->uv_v);
		/* Fix it up */
		if( uvp->uv_u < 0 )  uvp->uv_u = (-uvp->uv_u);
		if( uvp->uv_v < 0 )  uvp->uv_v = (-uvp->uv_v);
	}
	r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;
	uvp->uv_du = r * arbp->arb_Ulen;
	uvp->uv_dv = r * arbp->arb_Vlen;
}

/*
 *			A R B _ F R E E
 */
void
arb_free( stp )
register struct soltab *stp;
{
	register struct arb_specific *arbp =
		(struct arb_specific *)stp->st_specific;

	while( arbp != ARB_NULL )  {
		register struct arb_specific *nextarb = arbp->arb_forw;

		arbp->arb_forw = FreeArb;
		FreeArb = arbp;
		arbp = nextarb;
	}
}

#define ARB_FACE( valp, a, b, c, d ) \
	ADD_VL( vhead, &valp[a*3], 0 ); \
	ADD_VL( vhead, &valp[b*3], 1 ); \
	ADD_VL( vhead, &valp[c*3], 1 ); \
	ADD_VL( vhead, &valp[d*3], 1 );

/*
 *  			A R B _ P L O T
 *
 * Plot an ARB, which is represented as a vector
 * from the origin to the first point, and 7 vectors
 * from the first point to the remaining points.
 */
void
arb_plot( rp, matp, vhead, dp )
register union record	*rp;
register matp_t		matp;
struct vlhead		*vhead;
{
	register int		i;
	register dbfloat_t	*ip;
	register fastf_t	*op;
	static vect_t		work;
	static fastf_t		points[3*8];
	
	/*
	 * Convert from vector to point notation for drawing.
	 */
	MAT4X3PNT( &points[0], matp, &rp[0].s.s_values[0] );

	ip = &rp[0].s.s_values[1*3];
	op = &points[1*3];
	for( i=1; i<8; i++ )  {
		VADD2( work, &rp[0].s.s_values[0], ip );
		MAT4X3PNT( op, matp, work );
		ip += 3;
		op += ELEMENTS_PER_VECT;
	}

	ARB_FACE( points, 0, 1, 2, 3 );
	ARB_FACE( points, 4, 0, 3, 7 );
	ARB_FACE( points, 5, 4, 7, 6 );
	ARB_FACE( points, 1, 5, 6, 2 );
}

int
arb_class()
{
	return(0);
}
