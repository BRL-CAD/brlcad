/*                         G _ S P H . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup g_ */
/** @{ */
/** @file g_sph.c
 *
 *	Intersect a ray with a Sphere.
 *	Special case of the Generalized Ellipsoid
 *
 *  Authors -
 *	Phillip Dykstra
 *	Dave Becker		(Vectorization)
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
/** @} */
#ifndef lint
static const char RCSsph[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"


#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
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

/**
 *  			R T _ S P H _ P R E P
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
 *  	stp->st_specific for use by rt_sph_shot().
 *	If the ELL is really a SPH, stp->st_id is modified to ID_SPH.
 */
int
rt_sph_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	register struct sph_specific *sph;
	LOCAL fastf_t	magsq_a, magsq_b, magsq_c;
	LOCAL vect_t	Au, Bu, Cu;	/* A,B,C with unit length */
	LOCAL fastf_t	f;
	struct rt_ell_internal	*eip;

	eip = (struct rt_ell_internal *)ip->idb_ptr;
	RT_ELL_CK_MAGIC(eip);

	/* Validate that |A| > 0, |B| > 0, |C| > 0 */
	magsq_a = MAGSQ( eip->a );
	magsq_b = MAGSQ( eip->b );
	magsq_c = MAGSQ( eip->c );
	if( magsq_a < rtip->rti_tol.dist || magsq_b < rtip->rti_tol.dist || magsq_c < rtip->rti_tol.dist ) {
		bu_log("sph(%s):  zero length A(%g), B(%g), or C(%g) vector\n",
			stp->st_name, magsq_a, magsq_b, magsq_c );
		return(1);		/* BAD */
	}

	/* Validate that |A|, |B|, and |C| are nearly equal */
	if( fabs(magsq_a - magsq_b) > 0.0001
	    || fabs(magsq_a - magsq_c) > 0.0001 ) {
#if 0
		/* Ordinarily, don't say anything here, will handle as ELL */
		bu_log("sph(%s):  non-equal length A, B, C vectors\n",
			stp->st_name );
#endif
		return(1);		/* ELL, not SPH */
	}

	/* Create unit length versions of A,B,C */
	f = 1.0/sqrt(magsq_a);
	VSCALE( Au, eip->a, f );
	f = 1.0/sqrt(magsq_b);
	VSCALE( Bu, eip->b, f );
	f = 1.0/sqrt(magsq_c);
	VSCALE( Cu, eip->c, f );

	/* Validate that A.B == 0, B.C == 0, A.C == 0 (check dir only) */
	f = VDOT( Au, Bu );
	if( ! NEAR_ZERO(f, rtip->rti_tol.dist) )  {
		bu_log("sph(%s):  A not perpendicular to B, f=%f\n",stp->st_name, f);
		return(1);		/* BAD */
	}
	f = VDOT( Bu, Cu );
	if( ! NEAR_ZERO(f, rtip->rti_tol.dist) )  {
		bu_log("sph(%s):  B not perpendicular to C, f=%f\n",stp->st_name, f);
		return(1);		/* BAD */
	}
	f = VDOT( Au, Cu );
	if( ! NEAR_ZERO(f, rtip->rti_tol.dist) )  {
		bu_log("sph(%s):  A not perpendicular to C, f=%f\n",stp->st_name, f);
		return(1);		/* BAD */
	}

	/*
	 *  This ELL is really an SPH
	 */
	stp->st_id = ID_SPH;		/* "fix" soltab ID */
	stp->st_meth = &rt_functab[ID_SPH];

	/* Solid is OK, compute constant terms now */
	BU_GETSTRUCT( sph, sph_specific );
	stp->st_specific = (genptr_t)sph;

	VMOVE( sph->sph_V, eip->v );

	sph->sph_radsq = magsq_a;
	sph->sph_rad = sqrt(sph->sph_radsq);
	sph->sph_invrad = 1.0 / sph->sph_rad;

	/*
	 * Save the matrix which rotates our ABC to world
	 * XYZ respectively, and scales points on surface
	 * to unit length.  Used here in UV mapping.
	 * See ell.c for details.
	 */
	MAT_IDN( sph->sph_SoR );
	VSCALE( &sph->sph_SoR[0], eip->a, 1.0/magsq_a );
	VSCALE( &sph->sph_SoR[4], eip->b, 1.0/magsq_b );
	VSCALE( &sph->sph_SoR[8], eip->c, 1.0/magsq_c );

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

/**
 *			R T _ S P H _ P R I N T
 */
void
rt_sph_print(register const struct soltab *stp)
{
	register const struct sph_specific *sph =
		(struct sph_specific *)stp->st_specific;

	VPRINT("V", sph->sph_V);
	bu_log("Rad %g\n", sph->sph_rad);
	bu_log("Radsq %g\n", sph->sph_radsq);
	bu_log("Invrad %g\n", sph->sph_invrad);
	bn_mat_print("S o R", sph->sph_SoR );
}

/**
 *  			R T _ S P H _ S H O T
 *
 *  Intersect a ray with a sphere.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  Notes: In the quadratic equation,
 *   A is MAGSQ(r_dir) which is always equal to 1, so it does not appear.
 *   The sign of B is reversed (vector is reversed) to save negation.
 *   We have factored out the 2 and 4 constants.
 *  Claim: The straight quadratic formula leads to precision problems
 *   if either A or C are small.  In our case A is always 1.  C is a
 *   radial distance of the ray origin from the sphere surface.  Thus
 *   if we are shooting from near the surface we may have problems.
 *   XXX - investigate this.
 *
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_sph_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	register struct sph_specific *sph =
		(struct sph_specific *)stp->st_specific;
	register struct seg *segp;
	LOCAL vect_t	ov;		/* ray orgin to center (V - P) */
	FAST fastf_t	magsq_ov;	/* length squared of ov */
	FAST fastf_t	b;		/* second term of quadratic eqn */
	FAST fastf_t	root;		/* root of radical */

	VSUB2( ov, sph->sph_V, rp->r_pt );
	b = VDOT( rp->r_dir, ov );
	magsq_ov = MAGSQ(ov);

	if( magsq_ov >= sph->sph_radsq ) {
		/* ray origin is outside of sphere */
		if( b < 0 ) {
			/* ray direction is away from sphere */
			return(0);		/* No hit */
		}
		root = b*b - magsq_ov + sph->sph_radsq;
		if( root <= 0 ) {
			/* no real roots */
			return(0);		/* No hit */
		}
	} else {
		root = b*b - magsq_ov + sph->sph_radsq;
	}
	root = sqrt(root);

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;

	/* we know root is positive, so we know the smaller t */
	segp->seg_in.hit_dist = b - root;
	segp->seg_out.hit_dist = b + root;
	BU_LIST_INSERT( &(seghead->l), &(segp->l) );
	return(2);			/* HIT */
}

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;
/**
 *			R T _ S P H _ V S H O T
 *
 *  This is the Becker vectorized version
 */
void
rt_sph_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
			       /* An array of solid pointers */
			       /* An array of ray pointers */
			       /* array of segs (results returned) */
			       /* Number of ray/object pairs */

{
	register struct sph_specific *sph;
	LOCAL vect_t	ov;		/* ray orgin to center (V - P) */
	FAST fastf_t	magsq_ov;	/* length squared of ov */
	FAST fastf_t	b;		/* second term of quadratic eqn */
	FAST fastf_t	root;		/* root of radical */
	register int    i;

	/* for each ray/sphere pair */
#	include "noalias.h"
	for(i = 0; i < n; i++){
#if !CRAY	/* XXX currently prevents vectorization on cray */
		if (stp[i] == 0) continue; /* stp[i] == 0 signals skip ray */
#endif

		sph = (struct sph_specific *)stp[i]->st_specific;
		VSUB2( ov, sph->sph_V, rp[i]->r_pt );
		b = VDOT( rp[i]->r_dir, ov );
		magsq_ov = MAGSQ(ov);

		if( magsq_ov >= sph->sph_radsq ) {
			/* ray origin is outside of sphere */
			if( b < 0 ) {
				/* ray direction is away from sphere */
				SEG_MISS(segp[i]);		/* No hit */
				continue;
			}
			root = b*b - magsq_ov + sph->sph_radsq;
			if( root <= 0 ) {
				/* no real roots */
				SEG_MISS(segp[i]);		/* No hit */
				continue;
			}
		} else {
			root = b*b - magsq_ov + sph->sph_radsq;
		}
		root = sqrt(root);

		segp[i].seg_stp = stp[i];

		/* we know root is positive, so we know the smaller t */
		segp[i].seg_in.hit_dist = b - root;
		segp[i].seg_out.hit_dist = b + root;
	}
}

/**
 *  			R T _ S P H _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_sph_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
	register struct sph_specific *sph =
		(struct sph_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	VSUB2( hitp->hit_normal, hitp->hit_point, sph->sph_V );
	VSCALE( hitp->hit_normal, hitp->hit_normal, sph->sph_invrad );
}

/**
 *			R T _ S P H _ C U R V E
 *
 *  Return the curvature of the sphere.
 */
void
rt_sph_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
	register struct sph_specific *sph =
		(struct sph_specific *)stp->st_specific;

	cvp->crv_c1 = cvp->crv_c2 = - sph->sph_invrad;

	/* any tangent direction */
	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/**
 *  			R T _ S P H _ U V
 *
 *  For a hit on the surface of an SPH, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_sph_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
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

	/* U is azimuth, atan() range: -pi to +pi */
	uvp->uv_u = bn_atan2( pprime[Y], pprime[X] ) * bn_inv2pi;
	if( uvp->uv_u < 0 )
		uvp->uv_u += 1.0;
	/*
	 *  V is elevation, atan() range: -pi/2 to +pi/2,
	 *  because sqrt() ensures that X parameter is always >0
	 */
	uvp->uv_v = bn_atan2( pprime[Z],
		sqrt( pprime[X] * pprime[X] + pprime[Y] * pprime[Y]) ) *
		bn_invpi + 0.5;

	/* approximation: r / (circumference, 2 * pi * aradius) */
	r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;
	uvp->uv_du = uvp->uv_dv =
		bn_inv2pi * r / stp->st_aradius;
}

/**
 *		R T _ S P H _ F R E E
 */
void
rt_sph_free(register struct soltab *stp)
{
	register struct sph_specific *sph =
		(struct sph_specific *)stp->st_specific;

	bu_free( (char *)sph, "sph_specific" );
}

int
rt_sph_class(void)
{
	return(0);
}

/* ELL versions of the plot and tess functions are used */


#if 0
/**
 *			R T _ S P H _ I M P O R T 5
 *
 *  Import a sphere from the v5 database format to
 *  the internal structure.
 *  Apply modeling transformations as well.
 */
int
rt_sph_import5( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
const struct bu_external	*ep;
register const mat_t		mat;
const struct db_i		*dbip;
{
	struct rt_sph_internal	*sip;
	LOCAL fastf_t		vec[3+1];

	BU_CK_EXTERNAL( ep );

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_SPH;
	ip->idb_meth = &rt_functab[ID_SPH];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_sph_internal), "rt_sph_internal");

	sip = (struct rt_sph_internal *)ip->idb_ptr;
	sip->magic = RT_SPH_INTERNAL_MAGIC;

	/* Convert from database to internal format */
	htond( vec, ep->ext_buf, 3+1 );

	/* Apply modeling transformations */
	MAT4X3PNT( sip->v, mat, &vec[0*3] );
	MAT4XSCALOR( sip->r, mat, vec[1*3] );

	return(0);		/* OK */
}

/**
 *			R T _ S P H _ E X P O R T 5
 */
int
rt_sph_export5( ep, ip, local2mm, dbip )
struct bu_external		*ep;
const struct rt_db_internal	*ip;
double				local2mm;
const struct db_i		*dbip;
{
	struct rt_sph_internal	*tip;
	union record		*rec;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_ELL )  return(-1);
	tip = (struct rt_sph_internal *)ip->idb_ptr;
	RT_ELL_CK_MAGIC(tip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "sph external");
	rec = (union record *)ep->ext_buf;

	rec->s.s_id = ID_SOLID;
	rec->s.s_type = GENELL;

	/* NOTE: This also converts to dbfloat_t */
	VSCALE( &rec->s.s_values[0], tip->v, local2mm );
	VSCALE( &rec->s.s_values[3], tip->a, local2mm );
	VSCALE( &rec->s.s_values[6], tip->b, local2mm );
	VSCALE( &rec->s.s_values[9], tip->c, local2mm );

	return(0);
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
