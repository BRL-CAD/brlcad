/*
 *			S P H . C
 *
 *  Purpose -
 *	Intersect a ray with a Sphere
 *	Special case of the Generalized Ellipsoid
 *
 *  Authors -
 *	Phillip Dykstra
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
static char RCSsph[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

/*
 *  Algorithm:
 *  
 *  Given V, A, where |A| = Radius, there is a set of points on this sphere
 *  
 *  { (x,y,z) | (x,y,z) is on sphere defined by V, A }
 *  
 *  To find the intersection of a line with the sphere, consider
 *  the parametric line L:
 *  
 *  	L : { P(n) | P + t(n) . D }
 *  
 *  Call W the actual point of intersection between L and the sphere.
 *
 *  NORMALS.  Given the point W on the sphere, what is the vector
 *  normal to the tangent plane at that point?
 *  
 *  N = (W - V) / |A|
 */

struct sph_specific {
	vect_t	sph_V;		/* Vector to center of sphere */
	fastf_t	sph_radsq;	/* Radius squared */
	fastf_t	sph_invrad;	/* Inverse radius (for normal) */
	fastf_t	sph_rad;	/* Radius */
	mat_t	sph_SoR;	/* Rotate and scale for UV mapping */
};

/*
 *  			S P H _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid sphere, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	SPH is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct sph_specific is created, and it's address is stored in
 *  	stp->st_specific for use by sph_shot().
 *	If the ELL is really a SPH, stp->st_id is modified to ID_SPH.
 */
int
sph_prep( vec, stp, mat, rtip )
register fastf_t	*vec;
struct soltab		*stp;
matp_t			mat;
struct rt_i		*rtip;
{
	register struct sph_specific *sph;
	LOCAL fastf_t	magsq_a, magsq_b, magsq_c;
	LOCAL vect_t	A, B, C;
	LOCAL vect_t	Au, Bu, Cu;	/* A,B,C with unit length */
	LOCAL vect_t	work;
	LOCAL vect_t	vbc;	/* used for bounding RPP */
	LOCAL fastf_t	f;

#define ELL_V	&vec[0*ELEMENTS_PER_VECT]
#define ELL_A	&vec[1*ELEMENTS_PER_VECT]
#define ELL_B	&vec[2*ELEMENTS_PER_VECT]
#define ELL_C	&vec[3*ELEMENTS_PER_VECT]

	/*
	 * Apply rotation only to A,B,C
	 */
	MAT4X3VEC( A, mat, ELL_A );
	MAT4X3VEC( B, mat, ELL_B );
	MAT4X3VEC( C, mat, ELL_C );

	/* Validate that |A| > 0, |B| > 0, |C| > 0 */
	magsq_a = MAGSQ( A );
	magsq_b = MAGSQ( B );
	magsq_c = MAGSQ( C );
	if( NEAR_ZERO(magsq_a, 0.005) ||
	     NEAR_ZERO(magsq_b, 0.005) ||
	     NEAR_ZERO(magsq_c, 0.005) ) {
		rt_log("sph(%s):  zero length A, B, or C vector\n",
			stp->st_name );
		return(1);		/* BAD */
	}

	/* Validate that |A|, |B|, and |C| are nearly equal */
	if( fabs(magsq_a - magsq_b) > 0.0001
	    || fabs(magsq_a - magsq_c) > 0.0001 ) {
		/*rt_log("sph(%s):  non-equal length A, B, C vectors\n",
			stp->st_name );*/
		return(1);		/* ELL, not SPH */
	}

	/* Create unit length versions of A,B,C */
	f = 1.0/sqrt(magsq_a);
	VSCALE( Au, A, f );
	f = 1.0/sqrt(magsq_b);
	VSCALE( Bu, B, f );
	f = 1.0/sqrt(magsq_c);
	VSCALE( Cu, C, f );

	/* Validate that A.B == 0, B.C == 0, A.C == 0 (check dir only) */
	f = VDOT( Au, Bu );
	if( ! NEAR_ZERO(f, 0.005) )  {
		rt_log("sph(%s):  A not perpendicular to B, f=%f\n",stp->st_name, f);
		return(1);		/* BAD */
	}
	f = VDOT( Bu, Cu );
	if( ! NEAR_ZERO(f, 0.005) )  {
		rt_log("sph(%s):  B not perpendicular to C, f=%f\n",stp->st_name, f);
		return(1);		/* BAD */
	}
	f = VDOT( Au, Cu );
	if( ! NEAR_ZERO(f, 0.005) )  {
		rt_log("sph(%s):  A not perpendicular to C, f=%f\n",stp->st_name, f);
		return(1);		/* BAD */
	}

	/*
	 *  This ELL is really an SPH
	 */
	stp->st_id = ID_SPH;		/* "fix" soltab ID */

	/* Solid is OK, compute constant terms now */
	GETSTRUCT( sph, sph_specific );
	stp->st_specific = (int *)sph;

	/* Apply full 4x4mat to V */
	MAT4X3PNT( sph->sph_V, mat, ELL_V );

	sph->sph_radsq = magsq_a;
	sph->sph_rad = sqrt(sph->sph_radsq);
	sph->sph_invrad = 1.0 / sph->sph_rad;

	/*
	 * Save the matrix which rotates our ABC to world
	 * XYZ respectively, and scales points on surface
	 * to unit length.  Used here in UV mapping.
	 * See ell.c for details.
	 */
	mat_idn( sph->sph_SoR );
	VSCALE( &sph->sph_SoR[0], A, 1.0/magsq_a );
	VSCALE( &sph->sph_SoR[4], B, 1.0/magsq_b );
	VSCALE( &sph->sph_SoR[8], C, 1.0/magsq_c );

	/* Compute bounding sphere */
	VMOVE( stp->st_center, sph->sph_V );
	stp->st_aradius = stp->st_bradius = sph->sph_rad;

	/* Compute bounding RPP */
	stp->st_min[X] = sph->sph_V[X] - sph->sph_rad;
	stp->st_max[X] = sph->sph_V[X] + sph->sph_rad;
	stp->st_min[Y] = sph->sph_V[Y] - sph->sph_rad;
	stp->st_max[Y] = sph->sph_V[Y] + sph->sph_rad;
	stp->st_min[Z] = sph->sph_V[Z] - sph->sph_rad;
	stp->st_max[Z] = sph->sph_V[Z] + sph->sph_rad;

	return(0);			/* OK */
}

void
sph_print( stp )
register struct soltab *stp;
{
	register struct sph_specific *sph =
		(struct sph_specific *)stp->st_specific;

	VPRINT("V", sph->sph_V);
	rt_log("Rad %g\n", sph->sph_rad);
	rt_log("Radsq %g\n", sph->sph_radsq);
	rt_log("Invrad %g\n", sph->sph_invrad);
	mat_print("S o R", sph->sph_SoR );
}

/*
 *  			S P H _ S H O T
 *  
 *  Intersect a ray with a sphere.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *  	segp	HIT
 */
struct seg *
sph_shot( stp, rp, ap )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
{
	register struct sph_specific *sph =
		(struct sph_specific *)stp->st_specific;
	register struct seg *segp;
	LOCAL vect_t	delta;		/* P - V */
	LOCAL fastf_t	t1, t2;		/* distances in solution */
	FAST fastf_t	a, b;		/* two terms of radical */
	FAST fastf_t	root;		/* root of radical */

	VSUB2( delta, rp->r_pt, sph->sph_V );
	a = MAGSQ( rp->r_dir );
	b = VDOT( rp->r_dir, delta );

	if( (root = b*b - a * (MAGSQ(delta)-sph->sph_radsq)) < 0 )
		return(SEG_NULL);		/* No hit */
	root = sqrt(root);

	GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	if( (t1=(-b+root)/a) <= (t2=(-b-root)/a) )  {
		/* t1 is entry, t2 is exit */
		segp->seg_in.hit_dist = t1;
		segp->seg_out.hit_dist = t2;
	} else {
		/* t2 is entry, t1 is exit */
		segp->seg_in.hit_dist = t2;
		segp->seg_out.hit_dist = t1;
	}
	return(segp);			/* HIT */
}

/*
 *  			S P H _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
sph_norm( hitp, stp, rp )
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	register struct sph_specific *sph =
		(struct sph_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	VSUB2( hitp->hit_normal, hitp->hit_point, sph->sph_V );
	/* XXX is /= more efficient? */
	VSCALE( hitp->hit_normal, hitp->hit_normal, sph->sph_invrad );
}

/*
 *			S P H _ C U R V E
 *
 *  Return the curvature of the sphere.
 */
void
sph_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit *hitp;
struct soltab *stp;
{
	register struct sph_specific *sph =
		(struct sph_specific *)stp->st_specific;

 	cvp->crv_c1 = cvp->crv_c2 = - sph->sph_invrad;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			S P H _ U V
 *  
 *  For a hit on the surface of an SPH, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
/*double rt_inv2pi =  0.15915494309189533619;	/* 1/(pi*2) */
/*double rt_invpi = 0.31830988618379067153;	/* 1/pi */

void
sph_uv( ap, stp, hitp, uvp )
struct application *ap;
struct soltab *stp;
register struct hit *hitp;
register struct uvcoord *uvp;
{
	register struct sph_specific *sph =
		(struct sph_specific *)stp->st_specific;
	LOCAL fastf_t r;
	LOCAL vect_t work;
	LOCAL vect_t pprime;

	/* hit_point is on surface;  project back to unit sphere,
	 * creating a vector from vertex to hit point which always
	 * has length=1.0
	 */
	VSUB2( work, hitp->hit_point, sph->sph_V );
	MAT4X3VEC( pprime, sph->sph_SoR, work );
	/* Assert that pprime has unit length */

	uvp->uv_u = atan2( pprime[Y], pprime[X] ) * rt_inv2pi + 0.5;
	uvp->uv_v = asin( pprime[Z] ) * rt_invpi + 0.5;

	/* approximation: r / (circumference, 2 * pi * aradius) */
	r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;
	uvp->uv_du = uvp->uv_dv =
		rt_inv2pi * r / stp->st_aradius;
}

/*
 *		S P H _ F R E E
 */
void
sph_free( stp )
register struct soltab *stp;
{
	register struct sph_specific *sph =
		(struct sph_specific *)stp->st_specific;

	rt_free( (char *)sph, "sph_specific" );
}

int
sph_class()
{
	return(0);
}

void
sph_plot()
{
}
