/*
 *			E L L . C
 *
 *  Purpose -
 *	Intersect a ray with a Generalized Ellipsoid
 *
 *  Authors -
 *	Edwin O. Davisson	(Analysis)
 *	Michael John Muuss	(Programming)
 *	Peter F. Stiller	(Curvature Analysis)
 *	Phillip Dykstra		(RPPs, Curvature)
 *	Dave Becker		(Vectorization)
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
static char RCSell[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "nmg.h"
#include "./debug.h"

extern int sph_prep();

/*
 *  Algorithm:
 *  
 *  Given V, A, B, and C, there is a set of points on this ellipsoid
 *  
 *  { (x,y,z) | (x,y,z) is on ellipsoid defined by V, A, B, C }
 *  
 *  Through a series of Affine Transformations, this set will be
 *  transformed into a set of points on a unit sphere at the origin
 *  
 *  { (x',y',z') | (x',y',z') is on Sphere at origin }
 *  
 *  The transformation from X to X' is accomplished by:
 *  
 *  X' = S(R( X - V ))
 *  
 *  where R(X) =  ( A/(|A|) )
 *  		 (  B/(|B|)  ) . X
 *  		  ( C/(|C|) )
 *  
 *  and S(X) =	 (  1/|A|   0     0   )
 *  		(    0    1/|B|   0    ) . X
 *  		 (   0      0   1/|C| )
 *  
 *  To find the intersection of a line with the ellipsoid, consider
 *  the parametric line L:
 *  
 *  	L : { P(n) | P + t(n) . D }
 *  
 *  Call W the actual point of intersection between L and the ellipsoid.
 *  Let W' be the point of intersection between L' and the unit sphere.
 *  
 *  	L' : { P'(n) | P' + t(n) . D' }
 *  
 *  W = invR( invS( W' ) ) + V
 *  
 *  Where W' = k D' + P'.
 *  
 *  Let dp = D' dot P'
 *  Let dd = D' dot D'
 *  Let pp = P' dot P'
 *  
 *  and k = [ -dp +/- sqrt( dp*dp - dd * (pp - 1) ) ] / dd
 *  which is constant.
 *  
 *  Now, D' = S( R( D ) )
 *  and  P' = S( R( P - V ) )
 *  
 *  Substituting,
 *  
 *  W = V + invR( invS[ k *( S( R( D ) ) ) + S( R( P - V ) ) ] )
 *    = V + invR( ( k * R( D ) ) + R( P - V ) )
 *    = V + k * D + P - V
 *    = k * D + P
 *  
 *  Note that ``k'' is constant, and is the same in the formulations
 *  for both W and W'.
 *  
 *  NORMALS.  Given the point W on the ellipsoid, what is the vector
 *  normal to the tangent plane at that point?
 *  
 *  Map W onto the unit sphere, ie:  W' = S( R( W - V ) ).
 *  
 *  Plane on unit sphere at W' has a normal vector of the same value(!).
 *  
 *  The plane transforms back to the tangent plane at W, and this
 *  new plane (on the ellipsoid) has a normal vector of N, viz:
 *  
 *  N = inverse[ transpose(invR o invS) ] ( W' )
 *    = inverse[ transpose(invS) o transpose(invR) ] ( W' )
 *    = inverse[ inverse(S) o R ] ( W' )
 *    = invR o S ( W' )
 *    = invR( S( S( R( W - V ) ) ) )
 *
 *  Note that the normal vector produced above will not have unit length.
 */

struct ell_specific {
	vect_t	ell_V;		/* Vector to center of ellipsoid */
	vect_t	ell_Au;		/* unit-length A vector */
	vect_t	ell_Bu;
	vect_t	ell_Cu;
	vect_t	ell_invsq;	/* [ 1/(|A|**2), 1/(|B|**2), 1/(|C|**2) ] */
	mat_t	ell_SoR;	/* Scale(Rot(vect)) */
	mat_t	ell_invRSSR;	/* invRot(Scale(Scale(Rot(vect)))) */
};

/* Should be in a header file to share betwee g_ell.c and g_sph.c */
struct ell_internal  {
	point_t	v;
	vect_t	a;
	vect_t	b;
	vect_t	c;
};

/*
 *			E L L _ I M P O R T
 *
 *  Import an ellipsoid/sphere from the database format to
 *  the internal structure.
 *  Apply modeling transformations as well.
 */
int
ell_import( eip, rp, mat )
struct ell_internal	*eip;
union record		*rp;
register mat_t		mat;
{
	LOCAL fastf_t	vec[3*4];

	/* Check record type */
	if( rp->u_id != ID_SOLID )  {
		rt_log("ell_import: defective record\n");
		return(-1);
	}

	/* Convert from database to internal format */
	rt_fastf_float( vec, rp->s.s_values, 4 );

	/* Apply modeling transformations */
	MAT4X3PNT( eip->v, mat, &vec[0*3] );
	MAT4X3VEC( eip->a, mat, &vec[1*3] );
	MAT4X3VEC( eip->b, mat, &vec[2*3] );
	MAT4X3VEC( eip->c, mat, &vec[3*3] );

	return(0);		/* OK */
}

/*
 *  			E L L _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid ellipsoid, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	ELL is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct ell_specific is created, and it's address is stored in
 *  	stp->st_specific for use by ell_shot().
 */
int
ell_prep( stp, rec, rtip )
struct soltab		*stp;
union record		*rec;
struct rt_i		*rtip;
{
	register struct ell_specific *ell;
	LOCAL fastf_t	magsq_a, magsq_b, magsq_c;
	LOCAL mat_t	R;
	LOCAL mat_t	Rinv;
	LOCAL mat_t	SS;
	LOCAL mat_t	mtemp;
	LOCAL vect_t	Au, Bu, Cu;	/* A,B,C with unit length */
	LOCAL vect_t	w1, w2, P;	/* used for bounding RPP */
	LOCAL fastf_t	f;
	struct ell_internal ei;
	int		i;

	/*
	 *  For a fast way out, hand this solid off to the SPH routine.
	 *  If it takes it, then there is nothing to do, otherwise
	 *  the solid is an ELL.
	 */
	if( sph_prep( stp, rec, rtip ) == 0 )
		return(0);		/* OK */

	if( rec == (union record *)0 )  {
		rec = db_getmrec( rtip->rti_dbip, stp->st_dp );
		i = ell_import( &ei, rec, stp->st_pathmat );
		rt_free( (char *)rec, "ell record" );
	} else {
		i = ell_import( &ei, rec, stp->st_pathmat );
	}
	if( i < 0 )  {
		rt_log("ell_setup(%s): db import failure\n", stp->st_name);
		return(-1);		/* BAD */
	}

	/* Validate that |A| > 0, |B| > 0, |C| > 0 */
	magsq_a = MAGSQ( ei.a );
	magsq_b = MAGSQ( ei.b );
	magsq_c = MAGSQ( ei.c );
	if( magsq_a < 0.005 || magsq_b < 0.005 || magsq_c < 0.005 ) {
		rt_log("ell(%s):  zero length A, B, or C vector\n",
			stp->st_name );
		return(1);		/* BAD */
	}

	/* Create unit length versions of A,B,C */
	f = 1.0/sqrt(magsq_a);
	VSCALE( Au, ei.a, f );
	f = 1.0/sqrt(magsq_b);
	VSCALE( Bu, ei.b, f );
	f = 1.0/sqrt(magsq_c);
	VSCALE( Cu, ei.c, f );

	/* Validate that A.B == 0, B.C == 0, A.C == 0 (check dir only) */
	f = VDOT( Au, Bu );
	if( ! NEAR_ZERO(f, 0.005) )  {
		rt_log("ell(%s):  A not perpendicular to B, f=%f\n",stp->st_name, f);
		return(1);		/* BAD */
	}
	f = VDOT( Bu, Cu );
	if( ! NEAR_ZERO(f, 0.005) )  {
		rt_log("ell(%s):  B not perpendicular to C, f=%f\n",stp->st_name, f);
		return(1);		/* BAD */
	}
	f = VDOT( Au, Cu );
	if( ! NEAR_ZERO(f, 0.005) )  {
		rt_log("ell(%s):  A not perpendicular to C, f=%f\n",stp->st_name, f);
		return(1);		/* BAD */
	}

	/* Solid is OK, compute constant terms now */
	GETSTRUCT( ell, ell_specific );
	stp->st_specific = (genptr_t)ell;

	VMOVE( ell->ell_V, ei.v );

	VSET( ell->ell_invsq, 1.0/magsq_a, 1.0/magsq_b, 1.0/magsq_c );
	VMOVE( ell->ell_Au, Au );
	VMOVE( ell->ell_Bu, Bu );
	VMOVE( ell->ell_Cu, Cu );

	mat_idn( ell->ell_SoR );
	mat_idn( R );

	/* Compute R and Rinv matrices */
	VMOVE( &R[0], Au );
	VMOVE( &R[4], Bu );
	VMOVE( &R[8], Cu );
	mat_trn( Rinv, R );			/* inv of rot mat is trn */

	/* Compute SoS (Affine transformation) */
	mat_idn( SS );
	SS[ 0] = ell->ell_invsq[0];
	SS[ 5] = ell->ell_invsq[1];
	SS[10] = ell->ell_invsq[2];

	/* Compute invRSSR */
	mat_mul( mtemp, SS, R );
	mat_mul( ell->ell_invRSSR, Rinv, mtemp );

	/* Compute SoR */
	VSCALE( &ell->ell_SoR[0], ei.a, ell->ell_invsq[0] );
	VSCALE( &ell->ell_SoR[4], ei.b, ell->ell_invsq[1] );
	VSCALE( &ell->ell_SoR[8], ei.c, ell->ell_invsq[2] );

	/* Compute bounding sphere */
	VMOVE( stp->st_center, ei.v );
	f = magsq_a;
	if( magsq_b > f )
		f = magsq_b;
	if( magsq_c > f )
		f = magsq_c;
	stp->st_aradius = stp->st_bradius = sqrt(f);

	/* Compute bounding RPP */
	VSET( w1, magsq_a, magsq_b, magsq_c );

	/* X */
	VSET( P, 1.0, 0, 0 );		/* bounding plane normal */
	MAT3X3VEC( w2, R, P );		/* map plane to local coord syst */
	VELMUL( w2, w2, w2 );		/* square each term */
	f = VDOT( w1, w2 );
	f = f / sqrt(f);
	stp->st_min[X] = ell->ell_V[X] - f;	/* V.P +/- f */
	stp->st_max[X] = ell->ell_V[X] + f;

	/* Y */
	VSET( P, 0, 1.0, 0 );		/* bounding plane normal */
	MAT3X3VEC( w2, R, P );		/* map plane to local coord syst */
	VELMUL( w2, w2, w2 );		/* square each term */
	f = VDOT( w1, w2 );
	f = f / sqrt(f);
	stp->st_min[Y] = ell->ell_V[Y] - f;	/* V.P +/- f */
	stp->st_max[Y] = ell->ell_V[Y] + f;

	/* Z */
	VSET( P, 0, 0, 1.0 );		/* bounding plane normal */
	MAT3X3VEC( w2, R, P );		/* map plane to local coord syst */
	VELMUL( w2, w2, w2 );		/* square each term */
	f = VDOT( w1, w2 );
	f = f / sqrt(f);
	stp->st_min[Z] = ell->ell_V[Z] - f;	/* V.P +/- f */
	stp->st_max[Z] = ell->ell_V[Z] + f;

	return(0);			/* OK */
}

void
ell_print( stp )
register struct soltab *stp;
{
	register struct ell_specific *ell =
		(struct ell_specific *)stp->st_specific;

	VPRINT("V", ell->ell_V);
	mat_print("S o R", ell->ell_SoR );
	mat_print("invRSSR", ell->ell_invRSSR );
}

/*
 *  			E L L _ S H O T
 *  
 *  Intersect a ray with an ellipsoid, where all constant terms have
 *  been precomputed by ell_prep().  If an intersection occurs,
 *  a struct seg will be acquired and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *  	segp	HIT
 */
struct seg *
ell_shot( stp, rp, ap )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
{
	register struct ell_specific *ell =
		(struct ell_specific *)stp->st_specific;
	register struct seg *segp;
	LOCAL vect_t	dprime;		/* D' */
	LOCAL vect_t	pprime;		/* P' */
	LOCAL vect_t	xlated;		/* translated vector */
	LOCAL fastf_t	dp, dd;		/* D' dot P', D' dot D' */
	LOCAL fastf_t	k1, k2;		/* distance constants of solution */
	FAST fastf_t	root;		/* root of radical */

	/* out, Mat, vect */
	MAT4X3VEC( dprime, ell->ell_SoR, rp->r_dir );
	VSUB2( xlated, rp->r_pt, ell->ell_V );
	MAT4X3VEC( pprime, ell->ell_SoR, xlated );

	dp = VDOT( dprime, pprime );
	dd = VDOT( dprime, dprime );

	if( (root = dp*dp - dd * (VDOT(pprime,pprime)-1.0)) < 0 )
		return(SEG_NULL);		/* No hit */
	root = sqrt(root);

	GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	if( (k1=(-dp+root)/dd) <= (k2=(-dp-root)/dd) )  {
		/* k1 is entry, k2 is exit */
		segp->seg_in.hit_dist = k1;
		segp->seg_out.hit_dist = k2;
	} else {
		/* k2 is entry, k1 is exit */
		segp->seg_in.hit_dist = k2;
		segp->seg_out.hit_dist = k1;
	}
	return(segp);			/* HIT */
}


#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;	

/*
 *			E L L _ V S H O T
 *
 *  This is the Becker vector version.
 */
void
ell_vshot( stp, rp, segp, n, resp)
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct resource         *resp; /* pointer to a list of free segs */
{
	register int    i;
	register struct ell_specific *ell;
	LOCAL vect_t	dprime;		/* D' */
	LOCAL vect_t	pprime;		/* P' */
	LOCAL vect_t	xlated;		/* translated vector */
	LOCAL fastf_t	dp, dd;		/* D' dot P', D' dot D' */
	LOCAL fastf_t	k1, k2;		/* distance constants of solution */
	FAST fastf_t	root;		/* root of radical */

	/* for each ray/ellipse pair */
#	include "noalias.h"
	for(i = 0; i < n; i++){
#if !CRAY /* XXX currently prevents vectorization on cray */
	 	if (stp[i] == 0) continue; /* stp[i] == 0 signals skip ray */
#endif
		ell = (struct ell_specific *)stp[i]->st_specific;

		MAT4X3VEC( dprime, ell->ell_SoR, rp[i]->r_dir );
		VSUB2( xlated, rp[i]->r_pt, ell->ell_V );
		MAT4X3VEC( pprime, ell->ell_SoR, xlated );

		dp = VDOT( dprime, pprime );
		dd = VDOT( dprime, dprime );

		if( (root = dp*dp - dd * (VDOT(pprime,pprime)-1.0)) < 0 ) {
			SEG_MISS(segp[i]);		/* No hit */
		}
	        else {
			root = sqrt(root);

			segp[i].seg_next = SEG_NULL;
			segp[i].seg_stp = stp[i];

			if( (k1=(-dp+root)/dd) <= (k2=(-dp-root)/dd) )  {
				/* k1 is entry, k2 is exit */
				segp[i].seg_in.hit_dist = k1;
				segp[i].seg_out.hit_dist = k2;
			} else {
				/* k2 is entry, k1 is exit */
				segp[i].seg_in.hit_dist = k2;
				segp[i].seg_out.hit_dist = k1;
			}
		}
	}
}

/*
 *  			E L L _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
ell_norm( hitp, stp, rp )
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	register struct ell_specific *ell =
		(struct ell_specific *)stp->st_specific;
	LOCAL vect_t xlated;
	LOCAL fastf_t scale;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	VSUB2( xlated, hitp->hit_point, ell->ell_V );
	MAT4X3VEC( hitp->hit_normal, ell->ell_invRSSR, xlated );
	scale = 1.0 / MAGNITUDE( hitp->hit_normal );
	VSCALE( hitp->hit_normal, hitp->hit_normal, scale );

	/* tuck away this scale for the curvature routine */
	hitp->hit_vpriv[X] = scale;
}

/*
 *			E L L _ C U R V E
 *
 *  Return the curvature of the ellipsoid.
 */
void
ell_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit *hitp;
struct soltab *stp;
{
	register struct ell_specific *ell =
		(struct ell_specific *)stp->st_specific;
	vect_t	u, v;			/* basis vectors (with normal) */
	vect_t	vec1, vec2;		/* eigen vectors */
	vect_t	tmp;
	fastf_t	a, b, c, scale;

	/*
	 * choose a tangent plane coordinate system
	 *  (u, v, normal) form a right-handed triple
	 */
	vec_ortho( u, hitp->hit_normal );
	VCROSS( v, hitp->hit_normal, u );

	/* get the saved away scale factor */
	scale = - hitp->hit_vpriv[X];

	/* find the second fundamental form */
	MAT4X3VEC( tmp, ell->ell_invRSSR, u );
	a = VDOT(u, tmp) * scale;
	b = VDOT(v, tmp) * scale;
	MAT4X3VEC( tmp, ell->ell_invRSSR, v );
	c = VDOT(v, tmp) * scale;

	eigen2x2( &cvp->crv_c1, &cvp->crv_c2, vec1, vec2, a, b, c );
	VCOMB2( cvp->crv_pdir, vec1[X], u, vec1[Y], v );
	VUNITIZE( cvp->crv_pdir );
}

/*
 *  			E L L _ U V
 *  
 *  For a hit on the surface of an ELL, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
ell_uv( ap, stp, hitp, uvp )
struct application *ap;
struct soltab *stp;
register struct hit *hitp;
register struct uvcoord *uvp;
{
	register struct ell_specific *ell =
		(struct ell_specific *)stp->st_specific;
	LOCAL vect_t work;
	LOCAL vect_t pprime;
	LOCAL fastf_t r;

	/* hit_point is on surface;  project back to unit sphere,
	 * creating a vector from vertex to hit point which always
	 * has length=1.0
	 */
	VSUB2( work, hitp->hit_point, ell->ell_V );
	MAT4X3VEC( pprime, ell->ell_SoR, work );
	/* Assert that pprime has unit length */

	/* U is azimuth, atan() range: -pi to +pi */
	uvp->uv_u = mat_atan2( pprime[Y], pprime[X] ) * rt_inv2pi;
	if( uvp->uv_u < 0 )
		uvp->uv_u += 1.0;
	/*
	 *  V is elevation, atan() range: -pi/2 to +pi/2,
	 *  because sqrt() ensures that X parameter is always >0
	 */
	uvp->uv_v = mat_atan2( pprime[Z],
		sqrt( pprime[X] * pprime[X] + pprime[Y] * pprime[Y]) ) *
		rt_invpi + 0.5;

	/* approximation: r / (circumference, 2 * pi * aradius) */
	r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;
	uvp->uv_du = uvp->uv_dv =
		rt_inv2pi * r / stp->st_aradius;
}

/*
 *			E L L _ F R E E
 */
void
ell_free( stp )
register struct soltab *stp;
{
	register struct ell_specific *ell =
		(struct ell_specific *)stp->st_specific;

	rt_free( (char *)ell, "ell_specific" );
}

int
ell_class()
{
	return(0);
}

/*
 *			E L L _ 1 6 P T S
 */
#define ELLOUT(n)	ov+(n-1)*3
void
ell_16pts( ov, V, A, B )
register fastf_t *ov;
register fastf_t *V;
fastf_t *A, *B;
{
	static fastf_t c, d, e, f,g,h;

	e = h = .92388;			/* cos(22.5) */
	c = d = .707107;		/* cos(45) */
	g = f = .382683;		/* cos(67.5) */

	/*
	 * For angle theta, compute surface point as
	 *
	 *	V  +  cos(theta) * A  + sin(theta) * B
	 *
	 * note that sin(theta) is cos(90-theta).
	 */

	VADD2( ELLOUT(1), V, A );
	VJOIN2( ELLOUT(2), V, e, A, f, B );
	VJOIN2( ELLOUT(3), V, c, A, d, B );
	VJOIN2( ELLOUT(4), V, g, A, h, B );
	VADD2( ELLOUT(5), V, B );
	VJOIN2( ELLOUT(6), V, -g, A, h, B );
	VJOIN2( ELLOUT(7), V, -c, A, d, B );
	VJOIN2( ELLOUT(8), V, -e, A, f, B );
	VSUB2( ELLOUT(9), V, A );
	VJOIN2( ELLOUT(10), V, -e, A, -f, B );
	VJOIN2( ELLOUT(11), V, -c, A, -d, B );
	VJOIN2( ELLOUT(12), V, -g, A, -h, B );
	VSUB2( ELLOUT(13), V, B );
	VJOIN2( ELLOUT(14), V, g, A, -h, B );
	VJOIN2( ELLOUT(15), V, c, A, -d, B );
	VJOIN2( ELLOUT(16), V, e, A, -f, B );
}

/*
 *			E L L _ P L O T
 */
int
ell_plot( rp, matp, vhead, dp )
union record	*rp;
register matp_t matp;
struct vlhead	*vhead;
struct directory *dp;
{
	register int		i;
	struct ell_internal	ei;
	fastf_t top[16*3];
	fastf_t middle[16*3];
	fastf_t bottom[16*3];
	fastf_t	points[3*8];

	if( ell_import( &ei, rp, matp ) < 0 )  {
		rt_log("ell_plot(%s): db import failure\n", dp->d_namep);
		return(-1);
	}

	ell_16pts( top, ei.v, ei.a, ei.b );
	ell_16pts( bottom, ei.v, ei.b, ei.c );
	ell_16pts( middle, ei.v, ei.a, ei.c );

	ADD_VL( vhead, &top[15*ELEMENTS_PER_VECT], 0 );
	for( i=0; i<16; i++ )  {
		ADD_VL( vhead, &top[i*ELEMENTS_PER_VECT], 1 );
	}

	ADD_VL( vhead, &bottom[15*ELEMENTS_PER_VECT], 0 );
	for( i=0; i<16; i++ )  {
		ADD_VL( vhead, &bottom[i*ELEMENTS_PER_VECT], 1 );
	}

	ADD_VL( vhead, &middle[15*ELEMENTS_PER_VECT], 0 );
	for( i=0; i<16; i++ )  {
		ADD_VL( vhead, &middle[i*ELEMENTS_PER_VECT], 1 );
	}
	return(0);
}

#define XPLUS 0
#define XMIN  1
#define YPLUS 2
#define YMIN  3
#define ZPLUS 4
#define ZMIN  5

/* Vertices of a unit octahedron */
/* These need to be organized properly to give reasonable normals */
static struct usvert {
	int	a;
	int	b;
	int	c;
} octahedron[8] = {
    { XPLUS, ZPLUS, YPLUS },
    { YPLUS, ZPLUS, XMIN  },
    { XMIN , ZPLUS, YMIN  },
    { YMIN , ZPLUS, XPLUS },
    { XPLUS, YPLUS, ZMIN  },
    { YPLUS, XMIN , ZMIN  },
    { XMIN , YMIN , ZMIN  },
    { YMIN , XPLUS, ZMIN  }
};

/*
 *			E L L _ T E S S
 *
 *  Tessellate an ellipsoid.
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
ell_tess( r, m, rp, mat, dp )
struct nmgregion	**r;
struct model		*m;
register union record	*rp;
register mat_t		mat;
struct directory	*dp;
{
	struct shell		*s;
	struct ell_internal	ei;
	register int		i;
	struct faceuse		*outfaceuses[8];
	struct vertex		*verts[6];
	struct vertex		*vertlist[6];
	struct edgeuse		*eu;
	int			face;
	plane_t			plane;
	point_t			pt;

	if( ell_import( &ei, rp, mat ) < 0 )  {
		rt_log("ell_tess(%s): import failure\n", dp->d_namep);
		return(-1);
	}

	for( i=0; i<6; i++ )  verts[i] = (struct vertex *)0;

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = m->r_p->s_p;

	/* Build all 8 faces of the octahedron, 3 verts each */
	for( i=0; i<8; i++ )  {
		register struct usvert *ohp = &octahedron[i];
		vertlist[0] = verts[ohp->a];
		vertlist[1] = verts[ohp->b];
		vertlist[2] = verts[ohp->c];
		outfaceuses[i] = nmg_cface(s, vertlist, 3 );
		verts[ohp->a] = vertlist[0];
		verts[ohp->b] = vertlist[1];
		verts[ohp->c] = vertlist[2];
	}

	/* Associate initial vertex geometry:
	 * Six points lying on the ellipsoid.
	 */
	VADD2( pt, ei.v, ei.a );
	nmg_vertex_gv(verts[XPLUS], pt );
	VSUB2( pt, ei.v, ei.a );
	nmg_vertex_gv(verts[XMIN], pt );

	VADD2( pt, ei.v, ei.b );
	nmg_vertex_gv(verts[YPLUS], pt );
	VSUB2( pt, ei.v, ei.b );
	nmg_vertex_gv(verts[YMIN], pt );

	VADD2( pt, ei.v, ei.c );
	nmg_vertex_gv(verts[ZPLUS], pt );
	VSUB2( pt, ei.v, ei.c );
	nmg_vertex_gv(verts[ZMIN], pt );

	/* Associate face geometry */
	for (i=0 ; i < 8 ; ++i) {
		eu = outfaceuses[i]->lu_p->down.eu_p;
		if (rt_mk_plane_3pts(plane, eu->vu_p->v_p->vg_p->coord,
					eu->next->vu_p->v_p->vg_p->coord,
					eu->last->vu_p->v_p->vg_p->coord)) {
			rt_log("At %d in %s\n", __LINE__, __FILE__);
			rt_bomb("cannot make plane equation\n");
		}
		else if (plane[0] == 0.0 && plane[1] == 0.0 && plane[2] == 0.0) {
			rt_log("Bad plane equation from rt_mk_plane_3pts at %d in %s\n",
					__LINE__, __FILE__);
			rt_bomb("BAD Plane Equation");
		}
		else nmg_face_g(outfaceuses[i], plane);
	}

	/* Glue the edges of different outward pointing face uses together */
	nmg_gluefaces( outfaceuses, 8 );

	return(0);
}

#if 0
/*
 * sphere - generate a polygon mesh approximating a sphere by
 *  recursive subdivision. First approximation is an octahedron;
 *  each level of refinement increases the number of polygons by
 *  a factor of 4.
 * Level 3 (128 polygons) is a good tradeoff if gouraud
 *  shading is used to render the database.
 *
 * Usage: sphere [level]
 *	level is an integer >= 1 setting the recursion level (default 1).
 *
 * Notes:
 *
 *  The triangles are generated with vertices in clockwise order as
 *  viewed from the outside in a right-handed coordinate system.
 *  To reverse the order, compile with COUNTERCLOCKWISE defined.
 *
 *  Shared vertices are not retained, so numerical errors may produce
 *  cracks between polygons at high subdivision levels.
 *
 *  The subroutines print_object() and print_triangle() should
 *  be changed to generate whatever the desired database format is.
 *  If UNC is defined, a PPHIGS text archive is generated.
 *
 * Jon Leech 3/24/89
 */
#include <stdio.h>
#include <math.h>
#include <gl.h>

typedef struct {
    double  x, y, z;
} point;

typedef struct {
    point     pt[3];	/* Vertices of triangle */
    double    area;	/* Unused; might be used for adaptive subdivision */
} triangle;

typedef struct {
    int       npoly;	/* # of polygons in object */
    triangle *poly;	/* Polygons in no particular order */
} object;

/* Six equidistant points lying on the unit sphere */
#define XPLUS {  1,  0,  0 }	/*  X */
#define XMIN  { -1,  0,  0 }	/* -X */
#define YPLUS {  0,  1,  0 }	/*  Y */
#define YMIN  {  0, -1,  0 }	/* -Y */
#define ZPLUS {  0,  0,  1 }	/*  Z */
#define ZMIN  {  0,  0, -1 }	/* -Z */

/* Vertices of a unit octahedron */
triangle octahedron[] = {
    { XPLUS, ZPLUS, YPLUS }, 0.0,
    { YPLUS, ZPLUS, XMIN  }, 0.0,
    { XMIN , ZPLUS, YMIN  }, 0.0,
    { YMIN , ZPLUS, XPLUS }, 0.0,
    { XPLUS, YPLUS, ZMIN  }, 0.0,
    { YPLUS, XMIN , ZMIN  }, 0.0,
    { XMIN , YMIN , ZMIN  }, 0.0,
    { YMIN , XPLUS, ZMIN  }, 0.0
};

/* An octahedron */
object oct = {
    sizeof(octahedron) / sizeof(octahedron[0]),
    &octahedron[0]
};

/* Forward declarations */
point *normalize(/* point *p */);
point *midpoint(/* point *a, point *b */);
void print_object(/* object *obj, int level */);
void print_triangle(/* triangle *t */);
double sqr(/* double x */);
double area_of_triangle(/* triangle *t */);

extern char *malloc(/* unsigned */);
object *old;
int maxlevels;
void disp_object();

main(ac, av)
int ac;
char *av[];
{
    object *new;
    int     i, level;
    double min[3], max[3];
    maxlevels = 1;

    if (ac > 1)
	if ((maxlevels = atoi(av[1])) < 1) {
	    fprintf(stderr, "%s: # of levels must be >= 1\n", av[0]);
	    exit(1);
	}

    

#ifdef COUNTERCLOCKWISE
    /* Reverse order of points in each triangle */
    for (i = 0; i < oct.npoly; i++) {
	point tmp;
		      tmp = oct.poly[i].pt[0];
	oct.poly[i].pt[0] = oct.poly[i].pt[2];
	oct.poly[i].pt[2] = tmp;
    }
#endif

    old = &oct;

    /* Subdivide each starting triangle (maxlevels - 1) times */
    for (level = 1; level < maxlevels; level++) {
	/* Allocate a new object */
	new = (object *)malloc(sizeof(object));
	if (new == NULL) {
	    fprintf(stderr, "%s: Out of memory on subdivision level %d\n",
		av[0], level);
	    exit(1);
	}
	new->npoly = old->npoly * 4;

	/* Allocate 4* the number of points in the current approximation */
	new->poly  = (triangle *)malloc(new->npoly * sizeof(triangle));
	if (new->poly == NULL) {
	    fprintf(stderr, "%s: Out of memory on subdivision level %d\n",
		av[0], level);
	    exit(1);
	}

	/* Subdivide each polygon in the old approximation and normalize
	 *  the new points thus generated to lie on the surface of the unit
	 *  sphere.
	 * Each input triangle with vertices labelled [0,1,2] as shown
	 *  below will be turned into four new triangles:
	 *
	 *			Make new points
	 *			    a = (0+2)/2
	 *			    b = (0+1)/2
	 *			    c = (1+2)/2
	 *	  1
	 *	 /\		Normalize a, b, c
	 *	/  \
	 *    b/____\ c		Construct new triangles
	 *    /\    /\		    [0,b,a]
	 *   /	\  /  \		    [b,1,c]
	 *  /____\/____\	    [a,b,c]
	 * 0	  a	2	    [a,c,2]
	 */
	for (i = 0; i < old->npoly; i++) {
	    triangle
		 *oldt = &old->poly[i],
		 *newt = &new->poly[i*4];
	    point a, b, c;

	    a = *normalize(midpoint(&oldt->pt[0], &oldt->pt[2]));
	    b = *normalize(midpoint(&oldt->pt[0], &oldt->pt[1]));
	    c = *normalize(midpoint(&oldt->pt[1], &oldt->pt[2]));

	    newt->pt[0] = oldt->pt[0];
	    newt->pt[1] = b;
	    newt->pt[2] = a;
	    newt++;

	    newt->pt[0] = b;
	    newt->pt[1] = oldt->pt[1];
	    newt->pt[2] = c;
	    newt++;

	    newt->pt[0] = a;
	    newt->pt[1] = b;
	    newt->pt[2] = c;
	    newt++;

	    newt->pt[0] = a;
	    newt->pt[1] = c;
	    newt->pt[2] = oldt->pt[2];
	}

	if (level > 1) {
	    free(old->poly);
	    free(old);
	}

	/* Continue subdividing new triangles */
	old = new;
    }

    disp_Init();
	
    min[0] = -2;
    min[1] = -2;
    min[2] = -2;

    max[0] = 2;
    max[1] = 2;
    max[2] = 2;

    disp_Autoscale(min, max);
    /* Print out resulting approximation */
    disp_Viewloop( disp_object);

}

/* Normalize a point p */
point *normalize(p)
point *p;
{
    static point r;
    double mag;

    r = *p;
    mag = r.x * r.x + r.y * r.y + r.z * r.z;
    if (mag != 0.0) {
	mag = 1.0 / sqrt(mag);
	r.x *= mag;
	r.y *= mag;
	r.z *= mag;
    }

    return &r;
}

/* Return the average of two points */
point *midpoint(a, b)
point *a, *b;
{
    static point r;

    r.x = (a->x + b->x) * 0.5;
    r.y = (a->y + b->y) * 0.5;
    r.z = (a->z + b->z) * 0.5;

    return &r;
}
#endif
