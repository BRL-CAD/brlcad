/*
 *			G _ P A R T . C
 *
 *  Purpose -
 *	Intersect a ray with a "particle" solid, which can have
 *	three main forms:  sphere, hemisphere-tipped cylinder (lozenge),
 *	and hemisphere-tipped cone.
 *	This code draws on the examples of g_rec (Davisson) & g_sph (Dykstra).
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 *
 *  Algorithm for the hemisphere-tipped cylinder and cone cases -
 *  
 *  Given V, H, vrad, and hrad, there is a set of points on this cylinder
 *  
 *  { (x,y,z) | (x,y,z) is on cylinder }
 *  
 *  Through a series of Affine Transformations, this set of points will be
 *  transformed into a set of points on a unit cylinder (or cone)
 *  with the transformed base (V') located at the origin
 *  with a transformed radius of 1 (vrad').
 *  The height of the cylinder (or cone) along the +Z axis is +1
 *  (ie, H' = (0,0,1) ), with a transformed radius of hrad/vrad.
 *  
 *  
 *  { (x',y',z') | (x',y',z') is on cylinder at origin }
 *  
 *  The transformation from X to X' is accomplished by:
 *
 *  finding two unit vectors A and B mutually perpendicular, and perp. to H.
 *
 *  X' = S(R( X - V ))
 *
 *  where R(X) rotates H to the +Z axis, and S(X) scales vrad' to 1
 *  and |H'| to 1.
 *
 *  where R(X) =  ( A/(|A|) )
 *  		 (  B/(|B|)  ) . X
 *  		  ( H/(|H|) )
 *  
 *  and S(X) =	 (  1/|A|   0     0   )
 *  		(    0    1/|B|   0    ) . X
 *  		 (   0      0   1/|H| )
 *  
 *  To find the intersection of a line with the surface of the cylinder,
 *  consider the parametric line L:
 *  
 *  	L : { P(n) | P + t(n) . D }
 *  
 *  Call W the actual point of intersection between L and the cylinder.
 *  Let W' be the point of intersection between L' and the unit cylinder.
 *  
 *  	L' : { P'(n) | P' + t(n) . D' }
 *  
 *  W = invR( invS( W' ) ) + V
 *  
 *  Where W' = k D' + P'.
 *  
 *  If Dx' and Dy' are both 0, then there is no hit on the cylinder;
 *  but the end spheres need checking.
 *
 *  The equation for the unit cylinder ranging along Z is
 *
 *	x**2 + y**2 - r**2 = 0
 *
 *  and the equation for a unit cone ranging along Z is
 *
 *	x**2 + y**2 - f(z)**2 = 0
 *
 *  where in this case f(z) linearly interpolates the radius of the
 *  cylinder from vrad (r1) to hrad (r2) as z ranges from 0 to 1, i.e.:
 *
 *	f(z) = (r2-r1)/1 * z + r1
 *
 *  let m = (r2-r1)/1, and substitute:
 *
 *	x**2 + y**2 - (m*z+r1)**2 = 0 .
 *
 *  For the cylinder case, r1 == r2, so m == 0, and everything simplifies.
 *
 *  The parametric formulation for line L' is P' + t * D', or
 *
 *	x = Px' + t * Dx'
 *	y = Py' + t * Dy'
 *	z = Pz' + t * Dz' .
 *
 *  Substituting these definitions into the formula for the unit cone gives
 *
 *	(Px'+t*Dx')**2 + (Py'+t*Dy')**2 + (m*(Pz'+t*Dz')+r1)**2 = 0
 *
 *  Expanding and regrouping terms gives a quadratic in "t"
 *  which has the form
 *
 *	a * t**2 + b * t + c = 0
 *
 *  where
 *
 *	a = Dx'**2 + Dy'**2 - m**2 * Dz'**2
 *	b = 2 * (Px'*Dx' + Py'*Dy' - m**2 * Pz'*Dz' - m*r1*Dz')
 *	c = Px'**2 + Py'**2 - m**2 * Pz'**2 - 2*m*r1*Pz' - r1**2
 *
 *  Line L' hits the infinitely tall unit cone at point(s) W'
 *  which correspond to the roots of the quadratic.
 *  The quadratic formula yields values for "t"
 *
 *	t = [ -b +/- sqrt( b** - 4 * a * c ) ] / ( 2 * a )
 *
 *  This parameter "t" can be substituted into the formulas for either
 *  L' or L, because affine transformations preserve distances along lines.
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
 *  Note that ``t'' is constant, and is the same in the formulations
 *  for both W and W'.
 *
 *  The hit at ``t'' is a hit on the height=1 unit cylinder IFF
 *  0 <= Wz' <= 1.
 *  
 *  NORMALS.  Given the point W on the surface of the cylinder,
 *  what is the vector normal to the tangent plane at that point?
 *  
 *  Map W onto the unit cylinder, ie:  W' = S( R( W - V ) ).
 *  
 *  Plane on unit cylinder at W' has a normal vector N' of the same value
 *  as W' in x and y, with z set to zero, ie, (Wx', Wy', 0)
 *  
 *  The plane transforms back to the tangent plane at W, and this
 *  new plane (on the original cylinder) has a normal vector of N, viz:
 *  
 *  N = inverse[ transpose(invR o invS) ] ( N' )
 *    = inverse[ transpose(invS) o transpose(invR) ] ( N' )
 *    = inverse[ inverse(S) o R ] ( N' )
 *    = invR o S ( N' )
 *
 *  Note that the normal vector produced above will not have unit length.
 *
 *  THE END PLATES.
 *
 *  If Dz' == 0, line L' is parallel to the end plates, so there is no hit.
 *
 *  Otherwise, the line L' hits the bottom plate with k = (0 - Pz') / Dz',
 *  and hits the top plate with k = (1 - Pz') / Dz'.
 *
 *  The solution W' is within the end plate IFF
 *
 *	Wx'**2 + Wy'**2 <= 1.0
 *
 *  The normal for a hit on the bottom plate is -Hunit, and
 *  the normal for a hit on the top plate is +Hunit.
 */
#ifndef lint
static char RCSpart[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "nmg.h"
#include "rtgeom.h"
#include "./debug.h"

struct part_specific {
	struct rt_part_internal	part_int;
	mat_t			part_SoR;	/* Scale(Rot(vect)) */
	mat_t			part_invRoS;	/* invRot(Scale(vect)) */
};

/* hit_surfno flags for which end was hit */
#define RT_PARTICLE_SURF_VSPHERE	1
#define RT_PARTICLE_SURF_BODY		2
#define RT_PARTICLE_SURF_HSPHERE	3

struct bu_structparse rt_part_parse[] = {
    { "%f", 3, "V",  offsetof(struct rt_part_internal, part_V[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "H",  offsetof(struct rt_part_internal, part_H[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "r_v",offsetof(struct rt_part_internal, part_vrad), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "r_h",offsetof(struct rt_part_internal, part_hrad), BU_STRUCTPARSE_FUNC_NULL },
    {0} };
	    
RT_EXTERN( void rt_part_ifree, (struct rt_db_internal *ip) );

/*
 *  			R T _ P A R T _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid particle, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	particle is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct part_specific is created, and it's address is stored in
 *  	stp->st_specific for use by part_shot().
 */
int
rt_part_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	register struct part_specific *part;
	struct rt_part_internal	*pip;
	vect_t		Hunit;
	vect_t		a, b;
	mat_t		R, Rinv;
	mat_t		S;
	vect_t		max, min;
	vect_t		tip;

	RT_CK_DB_INTERNAL( ip );
	pip = (struct rt_part_internal *)ip->idb_ptr;
	RT_PART_CK_MAGIC(pip);

	GETSTRUCT( part, part_specific );
	stp->st_specific = (genptr_t)part;
	part->part_int = *pip;			/* struct copy */
	pip = &part->part_int;

	if( pip->part_type == RT_PARTICLE_TYPE_SPHERE )  {
		/* Compute bounding sphere and RPP */
		VMOVE( stp->st_center, pip->part_V );
		stp->st_aradius = stp->st_bradius = pip->part_vrad;
		stp->st_min[X] = pip->part_V[X] - pip->part_vrad;
		stp->st_max[X] = pip->part_V[X] + pip->part_vrad;
		stp->st_min[Y] = pip->part_V[Y] - pip->part_vrad;
		stp->st_max[Y] = pip->part_V[Y] + pip->part_vrad;
		stp->st_min[Z] = pip->part_V[Z] - pip->part_vrad;
		stp->st_max[Z] = pip->part_V[Z] + pip->part_vrad;
		return(0);		/* OK */
	}

	VMOVE( Hunit, pip->part_H );
	VUNITIZE( Hunit );
	vec_ortho( a, Hunit );
	VCROSS( b, Hunit, a );

	/* Compute R and Rinv */
	mat_idn( R );
	VMOVE( &R[0], a );		/* has unit length */
	VMOVE( &R[4], b );		/* has unit length */
	VMOVE( &R[8], Hunit );
	mat_trn( Rinv, R );

	/* Compute scale matrix S */
	mat_idn( S );
	S[ 0] = 1.0 / pip->part_vrad;	/* |A| = |B| */
	S[ 5] = S[0];
	S[10] = 1.0 / MAGNITUDE( pip->part_H );

	mat_mul( part->part_SoR, S, R );
	mat_mul( part->part_invRoS, Rinv, S );

	/* RPP and bounding sphere */
	VJOIN1( stp->st_center, pip->part_V, 0.5, pip->part_H );

	stp->st_aradius = stp->st_bradius = pip->part_vrad;

	stp->st_min[X] = pip->part_V[X] - pip->part_vrad;
	stp->st_max[X] = pip->part_V[X] + pip->part_vrad;
	stp->st_min[Y] = pip->part_V[Y] - pip->part_vrad;
	stp->st_max[Y] = pip->part_V[Y] + pip->part_vrad;
	stp->st_min[Z] = pip->part_V[Z] - pip->part_vrad;
	stp->st_max[Z] = pip->part_V[Z] + pip->part_vrad;

	VADD2( tip, pip->part_V, pip->part_H );
	min[X] = tip[X] - pip->part_hrad;
	max[X] = tip[X] + pip->part_hrad;
	min[Y] = tip[Y] - pip->part_hrad;
	max[Y] = tip[Y] + pip->part_hrad;
	min[Z] = tip[Z] - pip->part_hrad;
	max[Z] = tip[Z] + pip->part_hrad;
	VMINMAX( stp->st_min, stp->st_max, min );
	VMINMAX( stp->st_min, stp->st_max, max );

	/* Determine bounding sphere from the RPP */
	{
		register fastf_t	f;
		vect_t			work;
		VSUB2SCALE( work, stp->st_max, stp->st_min, 0.5 );	/* radius */
		f = work[X];
		if( work[Y] > f )  f = work[Y];
		if( work[Z] > f )  f = work[Z];
		stp->st_aradius = f;
		stp->st_bradius = MAGNITUDE(work);
	}
	return(0);			/* OK */
}

/*
 *			R T _ P A R T _ P R I N T
 */
void
rt_part_print( stp )
register CONST struct soltab *stp;
{
	register CONST struct part_specific *part =
		(struct part_specific *)stp->st_specific;

	VPRINT("part_V", part->part_int.part_V );
	VPRINT("part_H", part->part_int.part_V );
	rt_log("part_vrad=%g\n", part->part_int.part_vrad );
	rt_log("part_hrad=%g\n", part->part_int.part_hrad );

	switch( part->part_int.part_type )  {
	case RT_PARTICLE_TYPE_SPHERE:
		rt_log("part_type = SPHERE\n");
		break;
	case RT_PARTICLE_TYPE_CYLINDER:
		rt_log("part_type = CYLINDER\n");
		mat_print("part_SoR", part->part_SoR );
		mat_print("part_invRoS", part->part_invRoS );
		break;
	case RT_PARTICLE_TYPE_CONE:
		rt_log("part_type = CONE\n");
		mat_print("part_SoR", part->part_SoR );
		mat_print("part_invRoS", part->part_invRoS );
		break;
	default:
		rt_log("part_type = %d ???\n", part->part_int.part_type );
		break;
	}
}

/*
 *  			R T _ P A R T _ S H O T
 *  
 *  Intersect a ray with a part.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_part_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct part_specific *part =
		(struct part_specific *)stp->st_specific;
	register struct seg *segp;
	LOCAL vect_t	dprime;		/* D' */
	LOCAL point_t	pprime;		/* P' */
	LOCAL point_t	xlated;		/* translated ray start point */
	LOCAL fastf_t	t1, t2;		/* distance constants of solution */
	LOCAL fastf_t	f;
	LOCAL struct hit hits[4];	/* 4 potential hit points */
	register struct hit *hitp = &hits[0];
	int		check_v, check_h;

	if( part->part_int.part_type == RT_PARTICLE_TYPE_SPHERE )  {
		LOCAL vect_t	ov;		/* ray orgin to center (V - P) */
		FAST fastf_t	vrad_sq;
		FAST fastf_t	magsq_ov;	/* length squared of ov */
		FAST fastf_t	b;		/* second term of quadratic eqn */
		FAST fastf_t	root;		/* root of radical */

		VSUB2( ov, part->part_int.part_V, rp->r_pt );
		b = VDOT( rp->r_dir, ov );
		magsq_ov = MAGSQ(ov);

		if( magsq_ov >= (vrad_sq = part->part_int.part_vrad *
		    part->part_int.part_vrad) ) {
			/* ray origin is outside of sphere */
			if( b < 0 ) {
				/* ray direction is away from sphere */
				return(0);		/* No hit */
			}
			root = b*b - magsq_ov + vrad_sq;
			if( root <= 0 ) {
				/* no real roots */
				return(0);		/* No hit */
			}
		} else {
			root = b*b - magsq_ov + vrad_sq;
		}
		root = sqrt(root);

		RT_GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;

		/* we know root is positive, so we know the smaller t */
		segp->seg_in.hit_dist = b - root;
		segp->seg_in.hit_surfno = RT_PARTICLE_SURF_VSPHERE;
		segp->seg_out.hit_dist = b + root;
		segp->seg_out.hit_surfno = RT_PARTICLE_SURF_VSPHERE;
		RT_LIST_INSERT( &(seghead->l), &(segp->l) );
		return(2);			/* HIT */
	}

	/* Transform ray to coordinate system of unit cone at origin */
	MAT4X3VEC( dprime, part->part_SoR, rp->r_dir );
	VSUB2( xlated, rp->r_pt, part->part_int.part_V );
	MAT4X3VEC( pprime, part->part_SoR, xlated );

	if( NEAR_ZERO(dprime[X], SMALL) && NEAR_ZERO(dprime[Y], SMALL) )  {
		check_v = check_h = 1;
		goto check_hemispheres;
	}
	check_v = check_h = 0;

	/* Find roots of the equation, using forumla for quadratic */
	/* Note that vrad' = 1 and hrad' = hrad/vrad */
	if( part->part_int.part_type == RT_PARTICLE_TYPE_CYLINDER )  {
		/* Cylinder case, hrad == vrad, m = 0 */
		FAST fastf_t	a, b, c;
		FAST fastf_t	root;		/* root of radical */

		a = dprime[X]*dprime[X] + dprime[Y]*dprime[Y];
		b = dprime[X]*pprime[X] + dprime[Y]*pprime[Y];
		c = pprime[X]*pprime[X] + pprime[Y]*pprime[Y] - 1;
		if( (root = b*b - a * c) <= 0 )
			goto check_hemispheres;
		root = sqrt(root);
		t1 = (root-b) / a;
		t2 = -(root+b) / a;
	} else {
		/* Cone case */
		FAST fastf_t	a, b, c;
		FAST fastf_t	root;		/* root of radical */
		FAST fastf_t	m, msq;

#define VRAD_PRIME	1
#define HRAD_PRIME	(part->part_int.part_hrad / part->part_int.part_vrad)
		m = HRAD_PRIME - VRAD_PRIME;

		/* This quadratic has had a factor of 2 divided out of "b"
		 * throughout.  More efficient, but the same answers.
		 */
		a = dprime[X]*dprime[X] + dprime[Y]*dprime[Y] -
			(msq = m*m) * dprime[Z]*dprime[Z];
		b = dprime[X]*pprime[X] + dprime[Y]*pprime[Y] -
			msq * dprime[Z]*pprime[Z] -
			m * dprime[Z];		/* * VRAD_PRIME */
		c = pprime[X]*pprime[X] + pprime[Y]*pprime[Y] -
			msq * pprime[Z]*pprime[Z] -
			2 * m * pprime[Z] - 1;
			/* was: ... -2m * vrad' * Pz' - vrad'**2 */

		if( (root = b*b - a * c) <= 0 )
			goto check_hemispheres;
		root = sqrt(root);

		t1 = (root-b) / a;
		t2 = -(root+b) / a;
	}

	/*
	 *  t1 and t2 are potential solutions to intersection with side.
	 *  Find hit' point, see if Z values fall in range.
	 */
	if( (f = pprime[Z] + t1 * dprime[Z]) >= 0.0 )  {
		check_h = 1;		/* may also hit off end */
		if( f <= 1.0 )  {
			/** VJOIN1( hitp->hit_vpriv, pprime, t1, dprime ); **/
			hitp->hit_vpriv[X] = pprime[X] + t1 * dprime[X];
			hitp->hit_vpriv[Y] = pprime[Y] + t1 * dprime[Y];
			hitp->hit_vpriv[Z] = f;
			hitp->hit_dist = t1;
			hitp->hit_surfno = RT_PARTICLE_SURF_BODY;
			hitp++;
		}
	} else {
		check_v = 1;
	}

	if( (f = pprime[Z] + t2 * dprime[Z]) >= 0.0 )  {
		check_h = 1;		/* may also hit off end */
		if( f <= 1.0 )  {
			/** VJOIN1( hitp->hit_vpriv, pprime, t2, dprime ); **/
			hitp->hit_vpriv[X] = pprime[X] + t2 * dprime[X];
			hitp->hit_vpriv[Y] = pprime[Y] + t2 * dprime[Y];
			hitp->hit_vpriv[Z] = f;
			hitp->hit_dist = t2;
			hitp->hit_surfno = RT_PARTICLE_SURF_BODY;
			hitp++;
		}
	} else {
		check_v = 1;
	}

	/*
	 *  Check for hitting the end hemispheres.
	 */
check_hemispheres:
	if( check_v )  {
		LOCAL vect_t	ov;		/* ray orgin to center (V - P) */
		FAST fastf_t	rad_sq;
		FAST fastf_t	magsq_ov;	/* length squared of ov */
		FAST fastf_t	b;
		FAST fastf_t	root;		/* root of radical */

		/*
		 *  First, consider a hit on V hemisphere.
		 */
		VSUB2( ov, part->part_int.part_V, rp->r_pt );
		b = VDOT( rp->r_dir, ov );
		magsq_ov = MAGSQ(ov);
		if( magsq_ov >= (rad_sq = part->part_int.part_vrad *
		    part->part_int.part_vrad) ) {
			/* ray origin is outside of sphere */
			if( b < 0 ) {
				/* ray direction is away from sphere */
				goto do_check_h;
			}
			root = b*b - magsq_ov + rad_sq;
			if( root <= 0 ) {
				/* no real roots */
				goto do_check_h;
			}
		} else {
			root = b*b - magsq_ov + rad_sq;
		}
		root = sqrt(root);
		t1 = b - root;
		/* see if hit'[Z] is below end of cylinder */
		if( pprime[Z] + t1 * dprime[Z] <= 0.0 )  {
			hitp->hit_dist = t1;
			hitp->hit_surfno = RT_PARTICLE_SURF_VSPHERE;
			hitp++;
		}
		t2 = b + root;
		if( pprime[Z] + t2 * dprime[Z] <= 0.0 )  {
			hitp->hit_dist = t2;
			hitp->hit_surfno = RT_PARTICLE_SURF_VSPHERE;
			hitp++;
		}
	}

do_check_h:
	if( check_h )  {
		LOCAL vect_t	ov;		/* ray orgin to center (V - P) */
		FAST fastf_t	rad_sq;
		FAST fastf_t	magsq_ov;	/* length squared of ov */
		FAST fastf_t	b;		/* second term of quadratic eqn */
		FAST fastf_t	root;		/* root of radical */

		/*
		 *  Next, consider a hit on H hemisphere
		 */
		VADD2( ov, part->part_int.part_V, part->part_int.part_H );
		VSUB2( ov, ov, rp->r_pt );
		b = VDOT( rp->r_dir, ov );
		magsq_ov = MAGSQ(ov);
		if( magsq_ov >= (rad_sq = part->part_int.part_hrad *
		    part->part_int.part_hrad) ) {
			/* ray origin is outside of sphere */
			if( b < 0 ) {
				/* ray direction is away from sphere */
				goto out;
			}
			root = b*b - magsq_ov + rad_sq;
			if( root <= 0 ) {
				/* no real roots */
				goto out;
			}
		} else {
			root = b*b - magsq_ov + rad_sq;
		}
		root = sqrt(root);
		t1 = b - root;
		if( pprime[Z] + t1 * dprime[Z] >= 1.0 )  {
			hitp->hit_dist = t1;
			hitp->hit_surfno = RT_PARTICLE_SURF_HSPHERE;
			hitp++;
		}
		t2 = b + root;
		if( pprime[Z] + t2 * dprime[Z] >= 1.0 )  {
			hitp->hit_dist = t2;
			hitp->hit_surfno = RT_PARTICLE_SURF_HSPHERE;
			hitp++;
		}
	}
out:
	if( hitp == &hits[0] )
		return(0);	/* MISS */
	if( hitp == &hits[1] )  {
		/* Only one hit, make it a 0-thickness segment */
		hits[1] = hits[0];		/* struct copy */
		hitp++;
	} else if( hitp > &hits[2] )  {
		rt_log("rt_part_shot(%s): %d hits? surf0=%d\n",
			stp->st_name, hitp - &hits[0],
			hits[0].hit_surfno );
	}

	if( hits[0].hit_dist < hits[1].hit_dist )  {
		/* entry is [0], exit is [1] */
		register struct seg *segp;

		RT_GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;
		segp->seg_in = hits[0];		/* struct copy */
		segp->seg_out = hits[1];	/* struct copy */
		RT_LIST_INSERT( &(seghead->l), &(segp->l) );
	} else {
		/* entry is [1], exit is [0] */
		register struct seg *segp;

		RT_GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;
		segp->seg_in = hits[1];		/* struct copy */
		segp->seg_out = hits[0];	/* struct copy */
		RT_LIST_INSERT( &(seghead->l), &(segp->l) );
	}
	return(2);			/* HIT */
}

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;	

/*
 *			R T _ P A R T _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_part_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap ); 
}

/*
 *  			R T _ P A R T _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_part_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	register struct part_specific *part =
		(struct part_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	switch( hitp->hit_surfno )  {
	case RT_PARTICLE_SURF_VSPHERE:
		VSUB2( hitp->hit_normal, hitp->hit_point, part->part_int.part_V );
		VUNITIZE( hitp->hit_normal );
		break;
	case RT_PARTICLE_SURF_HSPHERE:
		VSUB3( hitp->hit_normal, hitp->hit_point,
			part->part_int.part_V, part->part_int.part_H );
		VUNITIZE( hitp->hit_normal );
		break;
	case RT_PARTICLE_SURF_BODY:
		/* compute it */
		if( part->part_int.part_type == RT_PARTICLE_TYPE_CYLINDER )  {
			/* The X' and Y' components of hit' are N' */
			hitp->hit_vpriv[Z] = 0;
			MAT4X3VEC( hitp->hit_normal, part->part_invRoS,
				hitp->hit_vpriv );
			VUNITIZE( hitp->hit_normal );
		} else {
			/* The cone case */
			FAST fastf_t	s, m;
			/* Rescale X' and Y' into unit circle */
			s = 1 / ( VRAD_PRIME + (m = HRAD_PRIME-VRAD_PRIME) *
				hitp->hit_vpriv[Z] );
			hitp->hit_vpriv[X] *= s;
			hitp->hit_vpriv[Y] *= s;
			/* Z' is constant, from slope of cylinder wall*/
			hitp->hit_vpriv[Z] = -m / sqrt(m*m+1);
			MAT4X3VEC( hitp->hit_normal, part->part_invRoS,
				hitp->hit_vpriv );
			VUNITIZE( hitp->hit_normal );
		}
		break;
	}
}

/*
 *			R T _ P A R T _ C U R V E
 *
 *  Return the curvature of the part.
 */
void
rt_part_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ P A R T _ U V
 *  
 *  For a hit on the surface of an part, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_part_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
/*	register struct part_specific *part =
		(struct part_specific *)stp->st_specific; */
}

/*
 *		R T _ P A R T _ F R E E
 */
void
rt_part_free( stp )
register struct soltab *stp;
{
	register struct part_specific *part =
		(struct part_specific *)stp->st_specific;

	rt_free( (char *)part, "part_specific" );
}

/*
 *			R T _ P A R T _ C L A S S
 */
int
rt_part_class()
{
	return(0);
}

/*
 *			R T _ P A R T _ H E M I S P H E R E 8
 *
 *  Produce a crude approximation to a hemisphere,
 *  8 points around the rim [0]..[7],
 *  4 points around a midway latitude [8]..[11], and
 *  1 point at the pole [12].
 *
 *  For the dome, connect up:
 *	0 8 12 10 4
 *	2 9 12 11 6
 */
HIDDEN void
rt_part_hemisphere( ov, v, a, b, h )
register point_t ov[];
register point_t v;
vect_t		a;
vect_t		b;
vect_t		h;
{
	register float cos45 = 0.707107;

	/* This is the top of the dome */
	VADD2( ov[12], v, h );

	VADD2( ov[0], v, a );
	VJOIN2( ov[1], v, cos45, a, cos45, b );
	VADD2( ov[2], v, b );
	VJOIN2( ov[3], v, -cos45, a, cos45, b );
	VSUB2( ov[4], v, a );
	VJOIN2( ov[5], v, -cos45, a, -cos45, b );
	VSUB2( ov[6], v, b );
	VJOIN2( ov[7], v, cos45, a, -cos45, b );

	VJOIN2( ov[8], v, cos45, a, cos45, h );
	VJOIN2( ov[10], v, -cos45, a, cos45, h );

	VJOIN2( ov[9], v, cos45, b, cos45, h );
	VJOIN2( ov[11], v, -cos45, b, cos45, h );
	/* Obviously, this could be optimized quite a lot more */
}

/*
 *			R T _ P A R T _ P L O T
 */
int
rt_part_plot( vhead, ip, ttol, tol )
struct rt_list	*vhead;
struct rt_db_internal *ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	struct rt_part_internal	*pip;
	point_t		tail;
	point_t		sphere_rim[16];
	point_t		vhemi[13];
	point_t		hhemi[13];
	vect_t		a, b, c;		/* defines coord sys of part */
	vect_t		as, bs, hs;		/* scaled by radius */
	vect_t		Hunit;
	register int	i;

	RT_CK_DB_INTERNAL(ip);
	pip = (struct rt_part_internal *)ip->idb_ptr;
	RT_PART_CK_MAGIC(pip);

	if( pip->part_type == RT_PARTICLE_TYPE_SPHERE )  {
		/* For the sphere, 3 rings of 16 points */
		VSET( a, pip->part_vrad, 0, 0 );
		VSET( b, 0, pip->part_vrad, 0 );
		VSET( c, 0, 0, pip->part_vrad );

		rt_ell_16pts( sphere_rim, pip->part_V, a, b );
		RT_ADD_VLIST( vhead, sphere_rim[15], RT_VLIST_LINE_MOVE );
		for( i=0; i<16; i++ )  {
			RT_ADD_VLIST( vhead, sphere_rim[i], RT_VLIST_LINE_DRAW );
		}

		rt_ell_16pts( sphere_rim, pip->part_V, b, c );
		RT_ADD_VLIST( vhead, sphere_rim[15], RT_VLIST_LINE_MOVE );
		for( i=0; i<16; i++ )  {
			RT_ADD_VLIST( vhead, sphere_rim[i], RT_VLIST_LINE_DRAW );
		}

		rt_ell_16pts( sphere_rim, pip->part_V, a, c );
		RT_ADD_VLIST( vhead, sphere_rim[15], RT_VLIST_LINE_MOVE );
		for( i=0; i<16; i++ )  {
			RT_ADD_VLIST( vhead, sphere_rim[i], RT_VLIST_LINE_DRAW );
		}
		return(0);		/* OK */
	}

	VMOVE( Hunit, pip->part_H );
	VUNITIZE( Hunit );
	vec_perp( a, Hunit );
	VUNITIZE(a);
	VCROSS( b, Hunit, a );
	VUNITIZE(b);

	VSCALE( as, a, pip->part_vrad );
	VSCALE( bs, b, pip->part_vrad );
	VSCALE( hs, Hunit, -pip->part_vrad );
	rt_part_hemisphere( vhemi, pip->part_V, as, bs, hs );

	VADD2( tail, pip->part_V, pip->part_H );
	VSCALE( as, a, pip->part_hrad );
	VSCALE( bs, b, pip->part_hrad );
	VSCALE( hs, Hunit, pip->part_hrad );
	rt_part_hemisphere( hhemi, tail, as, bs, hs );

	/* Draw V end hemisphere */
	RT_ADD_VLIST( vhead, vhemi[0], RT_VLIST_LINE_MOVE );
	for( i=7; i >= 0; i-- )  {
		RT_ADD_VLIST( vhead, vhemi[i], RT_VLIST_LINE_DRAW );
	}
	RT_ADD_VLIST( vhead, vhemi[8], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, vhemi[12], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, vhemi[10], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, vhemi[4], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, vhemi[2], RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vhead, vhemi[9], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, vhemi[12], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, vhemi[11], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, vhemi[6], RT_VLIST_LINE_DRAW );

	/* Draw H end hemisphere */
	RT_ADD_VLIST( vhead, hhemi[0], RT_VLIST_LINE_MOVE );
	for( i=7; i >= 0; i-- )  {
		RT_ADD_VLIST( vhead, hhemi[i], RT_VLIST_LINE_DRAW );
	}
	RT_ADD_VLIST( vhead, hhemi[8], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, hhemi[12], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, hhemi[10], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, hhemi[4], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, hhemi[2], RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vhead, hhemi[9], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, hhemi[12], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, hhemi[11], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, hhemi[6], RT_VLIST_LINE_DRAW );

	/* Draw 4 connecting lines */
	RT_ADD_VLIST( vhead, vhemi[0], RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vhead, hhemi[0], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, vhemi[2], RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vhead, hhemi[2], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, vhemi[4], RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vhead, hhemi[4], RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, vhemi[6], RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( vhead, hhemi[6], RT_VLIST_LINE_DRAW );

	return(0);
}

/*
 *			R T _ P A R T _ T E S S
 */
int
rt_part_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	return(-1);
}

/*
 *			R T _ P A R T _ I M P O R T
 */
int
rt_part_import( ip, ep, mat )
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
register CONST mat_t		mat;
{
	point_t		v;
	vect_t		h;
	double		vrad;
	double		hrad;
	fastf_t		maxrad, minrad;
	union record	*rp;
	struct rt_part_internal	*part;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_PARTICLE )  {
		rt_log("rt_part_import: defective record\n");
		return(-1);
	}

	/* Convert from database to internal format */
	ntohd( v, rp->part.p_v, 3 );
	ntohd( h, rp->part.p_h, 3 );
	ntohd( &vrad, rp->part.p_vrad, 1 );
	ntohd( &hrad, rp->part.p_hrad, 1 );

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_PARTICLE;
	ip->idb_ptr = rt_malloc( sizeof(struct rt_part_internal), "rt_part_internal");
	part = (struct rt_part_internal *)ip->idb_ptr;
	part->part_magic = RT_PART_INTERNAL_MAGIC;

	/* Apply modeling transformations */
	MAT4X3PNT( part->part_V, mat, v );
	MAT4X3VEC( part->part_H, mat, h );
	if( (part->part_vrad = vrad / mat[15]) < 0 )  {
		rt_free( ip->idb_ptr, "rt_part_internal" );
		return(-2);
	}
	if( (part->part_hrad = hrad / mat[15]) < 0 )  {
		rt_free( ip->idb_ptr, "rt_part_internal" );
		return(-3);
	}

	if( part->part_vrad > part->part_hrad )  {
		maxrad = part->part_vrad;
		minrad = part->part_hrad;
	} else {
		maxrad = part->part_hrad;
		minrad = part->part_vrad;
	}
	if( maxrad <= 0 )  {
		rt_free( ip->idb_ptr, "rt_part_internal" );
		return(-4);
	}

	if( MAGSQ( part->part_H ) * 1000000 < maxrad * maxrad )  {
		/* Height vector is insignificant, particle is a sphere */
		part->part_vrad = part->part_hrad = maxrad;
		VSETALL( part->part_H, 0 );		/* sanity */
		part->part_type = RT_PARTICLE_TYPE_SPHERE;
		return(0);		/* OK */
	}

	if( (maxrad - minrad) / maxrad < 0.001 )  {
		/* radii are nearly equal, particle is a cylinder (lozenge) */
		part->part_vrad = part->part_hrad = maxrad;
		part->part_type = RT_PARTICLE_TYPE_CYLINDER;
		return(0);		/* OK */
	}

	part->part_type = RT_PARTICLE_TYPE_CONE;
	return(0);		/* OK */
}

/*
 *			R T _ P A R T _ E X P O R T
 */
int
rt_part_export( ep, ip, local2mm )
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
{
	struct rt_part_internal	*pip;
	union record		*rec;
	point_t		vert;
	vect_t		hi;
	fastf_t		vrad;
	fastf_t		hrad;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_PARTICLE )  return(-1);
	pip = (struct rt_part_internal *)ip->idb_ptr;
	RT_PART_CK_MAGIC(pip);

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "part external");
	rec = (union record *)ep->ext_buf;

	/* Convert from user units to mm */
	VSCALE( vert, pip->part_V, local2mm );
	VSCALE( hi, pip->part_H, local2mm );
	vrad = pip->part_vrad * local2mm;
	hrad = pip->part_hrad * local2mm;
	/* pip->part_type is not converted -- internal only */

	rec->part.p_id = DBID_PARTICLE;
	htond( rec->part.p_v, vert, 3 );
	htond( rec->part.p_h, hi, 3 );
	htond( rec->part.p_vrad, &vrad, 1 );
	htond( rec->part.p_hrad, &hrad, 1 );

	return(0);
}

/*
 *			R T _ P A R T _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_part_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_part_internal	*pip =
		(struct rt_part_internal *)ip->idb_ptr;
	char	buf[256];

	RT_PART_CK_MAGIC(pip);
	switch( pip->part_type )  {
	case RT_PARTICLE_TYPE_SPHERE:
		rt_vls_strcat( str, "spherical particle\n");
		sprintf(buf, "\tV (%g, %g, %g)\n",
			pip->part_V[X] * mm2local,
			pip->part_V[Y] * mm2local,
			pip->part_V[Z] * mm2local );
		rt_vls_strcat( str, buf );
		sprintf(buf, "\tradius = %g\n",
			pip->part_vrad * mm2local );
		rt_vls_strcat( str, buf );
		break;
	case RT_PARTICLE_TYPE_CYLINDER:
		rt_vls_strcat( str, "cylindrical particle (lozenge)\n");
		sprintf(buf, "\tV (%g, %g, %g)\n",
			pip->part_V[X] * mm2local,
			pip->part_V[Y] * mm2local,
			pip->part_V[Z] * mm2local );
		rt_vls_strcat( str, buf );
		sprintf(buf, "\tH (%g, %g, %g)\n",
			pip->part_H[X] * mm2local,
			pip->part_H[Y] * mm2local,
			pip->part_H[Z] * mm2local );
		rt_vls_strcat( str, buf );
		sprintf(buf, "\tradius = %g\n",
			pip->part_vrad * mm2local );
		rt_vls_strcat( str, buf );
		break;
	case RT_PARTICLE_TYPE_CONE:
		rt_vls_strcat( str, "conical particle\n");
		sprintf(buf, "\tV (%g, %g, %g)\n",
			pip->part_V[X] * mm2local,
			pip->part_V[Y] * mm2local,
			pip->part_V[Z] * mm2local );
		rt_vls_strcat( str, buf );
		sprintf(buf, "\tH (%g, %g, %g)\n",
			pip->part_H[X] * mm2local,
			pip->part_H[Y] * mm2local,
			pip->part_H[Z] * mm2local );
		rt_vls_strcat( str, buf );
		sprintf(buf, "\tv end radius = %g\n",
			pip->part_vrad * mm2local );
		rt_vls_strcat( str, buf );
		sprintf(buf, "\th end radius = %g\n",
			pip->part_hrad * mm2local );
		rt_vls_strcat( str, buf );
		break;
	default:
		rt_vls_strcat( str, "Unknown particle type\n");
		return(-1);
	}
	return(0);
}

/*
 *			R T _ P A R T _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_part_ifree( ip )
struct rt_db_internal	*ip;
{
	RT_CK_DB_INTERNAL(ip);
	rt_free( ip->idb_ptr, "particle ifree" );
	ip->idb_ptr = GENPTR_NULL;
}
