/*
 *			G _ P A R T . C
 *
 *  Purpose -
 *	Intersect a ray with a "particle" solid.
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
 */
#ifndef lint
static char RCSpart[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

struct part_internal {
	point_t	part_V;
	vect_t	part_H;
	fastf_t	part_vrad;
	fastf_t	part_hrad;
	int	part_type;		/* sphere, cylinder, cone */
};
#define RT_PARTICLE_TYPE_SPHERE		1
#define RT_PARTICLE_TYPE_CYLINDER	2
#define RT_PARTICLE_TYPE_CONE		3

struct part_specific {
	vect_t	part_V;
};

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
rt_part_prep( stp, rec, rtip )
struct soltab		*stp;
union record		*rec;
struct rt_i		*rtip;
{
	register struct part_specific *part;
	struct part_internal	pi;
	int			i;

	if( rec == (union record *)0 )  {
		rec = db_getmrec( rtip->rti_dbip, stp->st_dp );
		i = rt_part_import( &pi, rec, stp->st_pathmat );
		rt_free( (char *)rec, "part record" );
	} else {
		i = rt_part_import( &pi, rec, stp->st_pathmat );
	}
	if( i < 0 )  {
		rt_log("rt_part_setup(%s): db import failure\n", stp->st_name);
		return(-1);		/* BAD */
	}

	return(-1);	/* unfinished */
}

/*
 *			R T _ P A R T _ P R I N T
 */
void
rt_part_print( stp )
register struct soltab *stp;
{
	register struct part_specific *part =
		(struct part_specific *)stp->st_specific;
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

	return(2);			/* HIT */
}

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;	

/*
 *			R T _ P A R T _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_part_vshot( stp, rp, segp, n, resp)
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct resource         *resp; /* pointer to a list of free segs */
{
	rt_vstub( stp, rp, segp, n, resp );
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
	register struct part_specific *part =
		(struct part_specific *)stp->st_specific;

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
	register struct part_specific *part =
		(struct part_specific *)stp->st_specific;
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
rt_part_plot( rp, mat, vhead, dp, abs_tol, rel_tol, norm_tol )
union record	*rp;
mat_t		mat;
struct vlhead	*vhead;
struct directory *dp;
double		abs_tol;
double		rel_tol;
double		norm_tol;
{
	struct part_internal	pi;
	point_t		tail;
	point_t		sphere_rim[16];
	point_t		vhemi[13];
	point_t		hhemi[13];
	vect_t		a, b, c;		/* defines coord sys of part */
	vect_t		as, bs, hs;		/* scaled by radius */
	vect_t		Hunit;
	register int	i;

	if( rt_part_import( &pi, rp, mat ) < 0 )  {
		rt_log("rt_part_plot(%s): db import failure\n", dp->d_namep);
		return(-1);
	}

	if( pi.part_type == RT_PARTICLE_TYPE_SPHERE )  {
		/* For the sphere, 3 rings of 16 points */
		VSET( a, pi.part_vrad, 0, 0 );
		VSET( b, 0, pi.part_vrad, 0 );
		VSET( c, 0, 0, pi.part_vrad );

		ell_16pts( sphere_rim, pi.part_V, a, b );
		ADD_VL( vhead, sphere_rim[15], 0 );
		for( i=0; i<16; i++ )  {
			ADD_VL( vhead, sphere_rim[i], 1 );
		}

		ell_16pts( sphere_rim, pi.part_V, b, c );
		ADD_VL( vhead, sphere_rim[15], 0 );
		for( i=0; i<16; i++ )  {
			ADD_VL( vhead, sphere_rim[i], 1 );
		}

		ell_16pts( sphere_rim, pi.part_V, a, c );
		ADD_VL( vhead, sphere_rim[15], 0 );
		for( i=0; i<16; i++ )  {
			ADD_VL( vhead, sphere_rim[i], 1 );
		}
		return(0);		/* OK */
	}

	VMOVE( Hunit, pi.part_H );
	VUNITIZE( Hunit );
	vec_perp( a, Hunit );
	VUNITIZE(a);
	VCROSS( b, Hunit, a );
	VUNITIZE(b);

	VSCALE( as, a, pi.part_vrad );
	VSCALE( bs, b, pi.part_vrad );
	VSCALE( hs, Hunit, -pi.part_vrad );
	rt_part_hemisphere( vhemi, pi.part_V, as, bs, hs );

	VADD2( tail, pi.part_V, pi.part_H );
	VSCALE( as, a, pi.part_hrad );
	VSCALE( bs, b, pi.part_hrad );
	VSCALE( hs, Hunit, pi.part_hrad );
	rt_part_hemisphere( hhemi, tail, as, bs, hs );

	/* Draw V end hemisphere */
	ADD_VL( vhead, vhemi[0], 0 );
	for( i=7; i >= 0; i-- )  {
		ADD_VL( vhead, vhemi[i], 1 );
	}
	ADD_VL( vhead, vhemi[8], 1 );
	ADD_VL( vhead, vhemi[12], 1 );
	ADD_VL( vhead, vhemi[10], 1 );
	ADD_VL( vhead, vhemi[4], 1 );
	ADD_VL( vhead, vhemi[2], 0 );
	ADD_VL( vhead, vhemi[9], 1 );
	ADD_VL( vhead, vhemi[12], 1 );
	ADD_VL( vhead, vhemi[11], 1 );
	ADD_VL( vhead, vhemi[6], 1 );

	/* Draw H end hemisphere */
	ADD_VL( vhead, hhemi[0], 0 );
	for( i=7; i >= 0; i-- )  {
		ADD_VL( vhead, hhemi[i], 1 );
	}
	ADD_VL( vhead, hhemi[8], 1 );
	ADD_VL( vhead, hhemi[12], 1 );
	ADD_VL( vhead, hhemi[10], 1 );
	ADD_VL( vhead, hhemi[4], 1 );
	ADD_VL( vhead, hhemi[2], 0 );
	ADD_VL( vhead, hhemi[9], 1 );
	ADD_VL( vhead, hhemi[12], 1 );
	ADD_VL( vhead, hhemi[11], 1 );
	ADD_VL( vhead, hhemi[6], 1 );

	/* Draw 4 connecting lines */
	ADD_VL( vhead, vhemi[0], 0 );
	ADD_VL( vhead, hhemi[0], 1 );
	ADD_VL( vhead, vhemi[2], 0 );
	ADD_VL( vhead, hhemi[2], 1 );
	ADD_VL( vhead, vhemi[4], 0 );
	ADD_VL( vhead, hhemi[4], 1 );
	ADD_VL( vhead, vhemi[6], 0 );
	ADD_VL( vhead, hhemi[6], 1 );

	return(0);
}

/*
 *			R T _ P A R T _ T E S S
 */
int
rt_part_tess( r, m, rp, mat, dp, abs_tol, rel_tol, norm_tol )
struct nmgregion	**r;
struct model		*m;
union record		*rp;
mat_t			mat;
struct directory	*dp;
double			abs_tol;
double			rel_tol;
double			norm_tol;
{
	return(-1);
}

/*
 *			R T _ P A R T _ I M P O R T
 */
int
rt_part_import( part, rp, mat )
struct part_internal	*part;
union record		*rp;
register mat_t		mat;
{
	point_t		v;
	vect_t		h;
	double		vrad;
	double		hrad;
	fastf_t		maxrad, minrad;

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

	/* Apply modeling transformations */
	MAT4X3PNT( part->part_V, mat, v );
	MAT4X3VEC( part->part_H, mat, h );
	if( (part->part_vrad = vrad / mat[15]) < 0 )
		return(-2);
	if( (part->part_hrad = hrad / mat[15]) < 0 )
		return(-3);

	if( part->part_vrad > part->part_hrad )  {
		maxrad = part->part_vrad;
		minrad = part->part_hrad;
	} else {
		maxrad = part->part_hrad;
		minrad = part->part_vrad;
	}
	if( maxrad <= 0 )
		return(-4);

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
