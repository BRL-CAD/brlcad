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

struct arb_opt {
	float	arb_UVorig[3];		/* origin of UV coord system */
	float	arb_U[3];		/* unit U vector (along B-A) */
	float	arb_V[3];		/* unit V vector (perp to N and U) */
	float	arb_Ulen;		/* length of U basis (for du) */
	float	arb_Vlen;		/* length of V basis (for dv) */
};

/* One of these for each face */
struct aface {
	fastf_t	A[3];			/* "A" point */
	fastf_t	N[3];			/* Unit-length Normal (outward) */
	fastf_t	NdotA;			/* Normal dot A */
};

/* One of these for each ARB, custom allocated to size */
struct arb_specific  {
	struct arb_opt	*arb_opt;	/* pointer to optional info */
	int		arb_nmfaces;	/* number of faces */
	struct aface	arb_face[4];	/* May really be up to [6] faces */
};

/* These hold temp values for the face being prep'ed */
#define ARB_MAXPTS	4		/* All we need are 4 points */
static point_t	arb_points[ARB_MAXPTS];	/* Actual points on plane */
static int	arb_npts;		/* number of points on plane */

static int	faces;			/* Number of faces done so far */
static struct aface	face[6];	/* work area */
static struct arb_opt	opt[6];		/* work area */

HIDDEN void	arb_add_pt();

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
	register int	i;
	register int	j;
	register int	k;
	LOCAL fastf_t	f;

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
#	include "noalias.h"
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
				stp, arb_info[i].ai_title );
next_pt:		;
		}

		if( arb_npts < 3 )  {
			/* This face is BAD */
			continue;
		}

		/* Scale U and V basis vectors by the inverse of Ulen and Vlen */
		opt[faces].arb_Ulen = 1.0 / opt[faces].arb_Ulen;
		opt[faces].arb_Vlen = 1.0 / opt[faces].arb_Vlen;
		VSCALE( opt[faces].arb_U, opt[faces].arb_U, opt[faces].arb_Ulen );
		VSCALE( opt[faces].arb_V, opt[faces].arb_V, opt[faces].arb_Vlen );

		faces++;
	}
	if( faces < 4  || faces > 6 )  {
		rt_log("arb(%s):  only %d faces present\n",
			stp->st_name, faces);
		return(1);			/* Error */
	}

	/*
	 *  Allocate a private copy of the accumulated parameters
	 *  of exactly the right size.
	 *  The size to malloc is chosen based upon the
	 *  exact number of faces.
	 */
	{
		register struct arb_specific *arbp;
		arbp = (struct arb_specific *)rt_malloc(
			sizeof(struct arb_specific) +
			sizeof(struct aface) * (faces - 4),
			"arb_specific" );
		arbp->arb_nmfaces = faces;
		for( i=0; i < faces; i++ )
			arbp->arb_face[i] = face[i];	/* struct copy */

		/* Eventually this will be optional */
		arbp->arb_opt = (struct arb_opt *)rt_malloc(
			faces * sizeof(struct arb_opt), "arb_opt");
		for( i=0; i < faces; i++ )
			arbp->arb_opt[i] = opt[i];	/* struct copy */

		stp->st_specific = (int *)arbp;
	}

	/*
	 * Compute bounding sphere which contains the bounding RPP.
	 * Find min and max of the point co-ordinates to find the
	 * bounding RPP.  Note that this center is NOT guaranteed
	 * to be contained within the solid!
	 */
	op = &vec[0];
#	include "noalias.h"
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
 *
 *  Static externs are used to build up the state of the current faces.
 */
HIDDEN void
arb_add_pt( point, stp, title )
register pointp_t point;
struct soltab *stp;
char	*title;
{
	register int i;
	LOCAL vect_t work;
	LOCAL vect_t P_A;		/* new point - A */
	FAST fastf_t f;
	register struct aface	*afp;

	afp = &face[faces];
	i = arb_npts++;		/* Current point number */

	/* The first 3 points are treated differently */
	switch( i )  {
	case 0:
		VMOVE( afp->A, point );
		VMOVE( opt[faces].arb_UVorig, point );
		return;					/* OK */
	case 1:
		VSUB2( opt[faces].arb_U, point, afp->A );	/* B-A */
		f = MAGNITUDE( opt[faces].arb_U );
		opt[faces].arb_Ulen = f;
		f = 1/f;
		VSCALE( opt[faces].arb_U, opt[faces].arb_U, f );
		return;					/* OK */
	case 2:
		VSUB2( P_A, point, afp->A );	/* C-A */
		/* Check for co-linear, ie, (B-A)x(C-A) == 0 */
		VCROSS( afp->N, opt[faces].arb_U, P_A );
		f = MAGNITUDE( afp->N );
		if( NEAR_ZERO(f,0.005) )  {
			arb_npts--;
			return;				/* BAD */
		}
		f = 1/f;
		VSCALE( afp->N, afp->N, f );

		/*
		 * Get vector perp. to AB in face of plane ABC.
		 * Scale by projection of AC, make this V.
		 */
		VCROSS( work, afp->N, opt[faces].arb_U );
		VUNITIZE( work );
		f = VDOT( work, P_A );
		VSCALE( opt[faces].arb_V, work, f );
		f = MAGNITUDE( opt[faces].arb_V );
		opt[faces].arb_Vlen = f;
		f = 1/f;
		VSCALE( opt[faces].arb_V, opt[faces].arb_V, f );

		/* Check for new Ulen */
		VSUB2( P_A, point, opt[faces].arb_UVorig );
		f = VDOT( P_A, opt[faces].arb_U );
		if( f > opt[faces].arb_Ulen ) {
			opt[faces].arb_Ulen = f;
		} else if( f < 0.0 ) {
			VJOIN1( opt[faces].arb_UVorig, opt[faces].arb_UVorig, f, opt[faces].arb_U );
			opt[faces].arb_Ulen += (-f);
		}

		/*
		 *  If C-A is clockwise from B-A, then the normal
		 *  points inwards, so we need to fix it here.
		 */
		VSUB2( work, afp->A, stp->st_center );
		f = VDOT( work, afp->N );
		if( f < 0.0 )  {
			VREVERSE(afp->N, afp->N);	/* "fix" normal */
		}
		afp->NdotA = VDOT( afp->N, afp->A );
		return;					/* OK */
	default:
		/* Merely validate 4th and subsequent points */
		VSUB2( P_A, point, opt[faces].arb_UVorig );
		/* Check for new Ulen, Vlen */
		f = VDOT( P_A, opt[faces].arb_U );
		if( f > opt[faces].arb_Ulen ) {
			opt[faces].arb_Ulen = f;
		} else if( f < 0.0 ) {
			VJOIN1( opt[faces].arb_UVorig, opt[faces].arb_UVorig, f, opt[faces].arb_U );
			opt[faces].arb_Ulen += (-f);
		}
		f = VDOT( P_A, opt[faces].arb_V );
		if( f > opt[faces].arb_Vlen ) {
			opt[faces].arb_Vlen = f;
		} else if( f < 0.0 ) {
			VJOIN1( opt[faces].arb_UVorig, opt[faces].arb_UVorig, f, opt[faces].arb_V );
			opt[faces].arb_Vlen += (-f);
		}

		VSUB2( P_A, point, afp->A );
		VUNITIZE( P_A );		/* Checking direction only */
		f = VDOT( afp->N, P_A );
		if( ! NEAR_ZERO(f,0.005) )  {
			/* Non-planar face */
			rt_log("arb(%s): face %s non-planar, dot=%g\n",
				stp->st_name, title, f );
#ifdef CONSERVATIVE
			arb_npts--;
			return;				/* BAD */
#endif
		}
		return;					/* OK */
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
	register struct aface	*afp;
	register int i;

	if( arbp == (struct arb_specific *)0 )  {
		rt_log("arb(%s):  no faces\n", stp->st_name);
		return;
	}
	rt_log("%d faces:\n", arbp->arb_nmfaces);
	for( i=0; i < arbp->arb_nmfaces; i++ )  {
		afp = &(arbp->arb_face[i]);
		VPRINT( "A", afp->A );
		VPRINT( "Normal", afp->N );
		rt_log( "N.A = %g\n", afp->NdotA );
		if( arbp->arb_opt )  {
			register struct arb_opt	*op;
			op = &(arbp->arb_opt[i]);
			VPRINT( "UVorig", op->arb_UVorig );
			VPRINT( "U", op->arb_U );
			VPRINT( "V", op->arb_V );
			rt_log( "Ulen = %g, Vlen = %g\n",
				op->arb_Ulen, op->arb_Vlen);
		}
	}
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
	LOCAL int iplane, oplane;
	LOCAL fastf_t	in, out;	/* ray in/out distances */
	register int j;

	in = -INFINITY;
	out = INFINITY;
	iplane = oplane = -1;

	/* consider each face */
	for( j = arbp->arb_nmfaces-1; j >= 0; j-- )  {
		FAST fastf_t	dn;		/* Direction dot Normal */
		FAST fastf_t	dxbdn;
		FAST fastf_t	s;

		dxbdn = VDOT( arbp->arb_face[j].N, rp->r_pt ) - arbp->arb_face[j].NdotA;
		if( (dn = -VDOT( arbp->arb_face[j].N, rp->r_dir )) < -SQRT_SMALL_FASTF )  {
			/* exit point, when dir.N < 0.  out = min(out,s) */
			if( out > (s = dxbdn/dn) )  {
				out = s;
				oplane = j;
			}
		} else if ( dn > SQRT_SMALL_FASTF )  {
			/* entry point, when dir.N > 0.  in = max(in,s) */
			if( in < (s = dxbdn/dn) )  {
				in = s;
				iplane = j;
			}
		}  else  {
			/* ray is parallel to plane when dir.N == 0.
			 * If it is outside the solid, stop now */
			if( dxbdn > 0.0 )
				return( SEG_NULL );	/* MISS */
		}
		if( in > out )
			return( SEG_NULL );	/* MISS */
	}
	/* Validate */
	if( iplane == -1 || oplane == -1 )  {
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

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;	
/*
 *			A R B _ V S H O T
 *
 *  This is the Becker vector version
 */
void
arb_vshot( stp, rp, segp, n, resp)
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		 	    n; /* Number of ray/object pairs */
struct resource         *resp; /* pointer to a list of free segs */
{
	register int    j, i;
	register struct arb_specific *arbp;
	FAST fastf_t	dn;		/* Direction dot Normal */
	FAST fastf_t	dxbdn;
	FAST fastf_t	s;

	/* Intialize return values */
#	include "noalias.h"
	for(i = 0; i < n; i++){
		segp[i].seg_stp = stp[i];	/* Assume hit, if 0 then miss */
                segp[i].seg_in.hit_dist = -INFINITY;        /* used as in */
                segp[i].seg_in.hit_private = (char *) -1;   /* used as iplane */
                segp[i].seg_out.hit_dist = INFINITY;        /* used as out */
                segp[i].seg_out.hit_private = (char *) -1;  /* used as oplane */
                segp[i].seg_next = SEG_NULL;
	}

	/* consider each face */
	for(j = 0; j < 6; j++)  {
		/* for each ray/arb_face pair */
#		include "noalias.h"
		for(i = 0; i < n; i++)  {
			if (stp[i] == 0) continue;	/* skip this ray */
			if ( segp[i].seg_stp == 0 ) continue;	/* miss */

			arbp= (struct arb_specific *) stp[i]->st_specific;
			if ( arbp->arb_nmfaces <= j )
				continue; /* faces of this ARB are done */

			dxbdn = VDOT( arbp->arb_face[j].N, rp[i]->r_pt ) -
				arbp->arb_face[j].NdotA;
			if( (dn = -VDOT( arbp->arb_face[j].N, rp[i]->r_dir )) <
							-SQRT_SMALL_FASTF )  {
			   /* exit point, when dir.N < 0.  out = min(out,s) */
			   if( segp[i].seg_out.hit_dist > (s = dxbdn/dn) )  {
			   	   segp[i].seg_out.hit_dist = s;
				   segp[i].seg_out.hit_private = (char *) j;
			   }
			} else if ( dn > SQRT_SMALL_FASTF )  {
			   /* entry point, when dir.N > 0.  in = max(in,s) */
			   if( segp[i].seg_in.hit_dist < (s = dxbdn/dn) )  {
				   segp[i].seg_in.hit_dist = s;
				   segp[i].seg_in.hit_private = (char *) j;
			   }
		        }  else  {
			   /* ray is parallel to plane when dir.N == 0.
			    * If it is outside the solid, stop now */
			   if( dxbdn > 0.0 ) {
				SEG_MISS(segp[i]);		/* MISS */
			   }
			}
		        if(segp[i].seg_in.hit_dist > segp[i].seg_out.hit_dist) {
			   SEG_MISS(segp[i]);		/* MISS */
			}
		} /* for each ray/arb_face pair */
	} /* for each arb_face */

	/*
	 *  Validate for each ray/arb_face pair
	 *  Segment was initialized as "good" (seg_stp set valid);
	 *  that is revoked here on misses.
	 */
#	include "noalias.h"
	for(i = 0; i < n; i++){
		if (stp[i] == 0) continue;		/* skip this ray */
		if ( segp[i].seg_stp == 0 ) continue;	/* missed */

		if( segp[i].seg_in.hit_private == (char *) -1 ||
		    segp[i].seg_out.hit_private == (char *) -1 )  {
			SEG_MISS(segp[i]);		/* MISS */
		}
		else if(segp[i].seg_in.hit_dist >= segp[i].seg_out.hit_dist ||
			segp[i].seg_out.hit_dist >= INFINITY ) {
			SEG_MISS(segp[i]);		/* MISS */
		}
	}
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
		(struct arb_specific *)stp->st_specific;
	int	h;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	h = (int)hitp->hit_private;
	VMOVE( hitp->hit_normal, arbp->arb_face[h].N );
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
		(struct arb_specific *)stp->st_specific;
	int	h;
	LOCAL vect_t P_A;
	LOCAL fastf_t r;

	h = (int)hitp->hit_private;

	VSUB2( P_A, hitp->hit_point, arbp->arb_opt[h].arb_UVorig );
	/* Flipping v is an artifact of how the faces are built */
	uvp->uv_u = VDOT( P_A, arbp->arb_opt[h].arb_U );
	uvp->uv_v = 1.0 - VDOT( P_A, arbp->arb_opt[h].arb_V );
	if( uvp->uv_u < 0 || uvp->uv_v < 0 || uvp->uv_u > 1 || uvp->uv_v > 1 )  {
		rt_log("arb_uv: bad uv=%g,%g\n", uvp->uv_u, uvp->uv_v);
		/* Fix it up */
		if( uvp->uv_u < 0 )  uvp->uv_u = (-uvp->uv_u);
		if( uvp->uv_v < 0 )  uvp->uv_v = (-uvp->uv_v);
	}
	r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;
	uvp->uv_du = r * arbp->arb_opt[h].arb_Ulen;
	uvp->uv_dv = r * arbp->arb_opt[h].arb_Vlen;
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

	if( arbp->arb_opt )
		rt_free( (char *)arbp->arb_opt, "arb_opt" );
	rt_free( (char *)arbp, "arb_specific" );
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
#	include "noalias.h"
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
