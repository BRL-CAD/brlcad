/*                         G _ H Y P . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2008 United States Government as represented by
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
/** @addtogroup g_  */
/** @{ */
/** @file g_hpy.c
 *
 *	Intersect a ray with an elliptical hyperboloid of one sheet.
 *
 * Adding a new solid type:
 *	Design disk record
 *
 *	define rt_hyp_internal --- parameters for solid
 *	define hyp_specific --- raytracing form, possibly w/precomuted terms
 *	define rt_hyp_parse --- struct bu_structparse for "db get", "db adjust", ...
 *
 *	code import/export/describe/print/ifree/plot/prep/shot/curve/uv/tess
 *
 *	edit db.h add solidrec s_type define
 *	edit rtgeom.h to add rt_hyp_internal
 *	edit table.c:
 *		RT_DECLARE_INTERFACE()
 *		struct rt_functab entry
 *		rt_id_solid()
 *	edit raytrace.h to make ID_HYP, increment ID_MAXIMUM
 *	edit db_scan.c to add the new solid to db_scan()
 *	edit Makefile.am to add g_hyp.c to compile
 *
 *	Then:
 *	go to src/libwdb and create mk_hyp() routine
 *	go to src/conv and edit g2asc.c and asc2g.c to support the new solid
 *	go to src/librt and edit tcl.c to add the new solid to
 *		rt_solid_type_lookup[]
 *		also add the interface table and to rt_id_solid() in table.c
 *	go to src/mged and create the edit support
 *  
 *  Hyperboloid of one sheet:
 *  
 *  	[ (x * x) / (r1 * r1) ] + [ (y * y) / (r2 * r2) ] - [ (z*z) * (c*c) / (a*a) ] = 1
 *  
 *  	r1:	semi-major axis, along Au
 *  	r2:	semi-minor axis, along Au x H
 *  	c:	slope of asymptotic cone in the Au-H plane
 *  
 *  Authors - Timothy Van Ruitenbeek
 *
 */
/** @} */

#include "common.h"

/* system headers */
#include <stdio.h>
#include <math.h>

/* interface headers */
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"


/* ray tracing form of solid, including precomputed terms */
struct hyp_specific {
    point_t	hyp_V;		/* vector to hyp origin */
    vect_t	hyp_Hunit;	/* unit H vector */
    vect_t	hyp_Aunit;	/* unit vector along semi-major axis */
    vect_t	hyp_Bunit;	/* unit vector, A x H, semi-minor axis */
    mat_t	hyp_SoR;	/* Scale(Rot(vect)) */
    mat_t	hyp_invRoS;	/* invRot(Scale(vect)) */
    fastf_t	hyp_Hmag;	/* scaled height of hyperboloid */
};

const struct bu_structparse rt_hyp_parse[] = {
    { "%f", 3, "V",   bu_offsetof(struct rt_hyp_internal, hyp_V[X]),  BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "H",   bu_offsetof(struct rt_hyp_internal, hyp_H[X]),  BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "A",   bu_offsetof(struct rt_hyp_internal, hyp_Au[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "r_1", bu_offsetof(struct rt_hyp_internal, hyp_r1),    BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "r_2", bu_offsetof(struct rt_hyp_internal, hyp_r2),    BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "c",   bu_offsetof(struct rt_hyp_internal, hyp_c),     BU_STRUCTPARSE_FUNC_NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL }
};

/**
 *  			R T _ H Y P _ P R E P
 *
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid HYP, and if so, precompute various
 *  terms of the formula.
 *
 *  Returns -
 *  	0	HYP is OK
 *  	!0	Error in description
 *
 *  Implicit return -
 *  	A struct hyp_specific is created, and it's address is stored in
 *  	stp->st_specific for use by hyp_shot().
 */
int
rt_hyp_prep( struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip )
{
    struct rt_hyp_internal		*hyp_ip;
    register struct hyp_specific	*hyp;
#ifndef NO_MAGIC_CHECKING
    const struct bn_tol		*tol = &rtip->rti_tol;
#endif

/*  other vars here:  */
    vect_t	unitH;	/* unit vector along axis of revolution */
    vect_t	unitA;	/* unit vector along semi-major axis of elliptical cross section */
    vect_t	unitB;	/* unit vector along semi-minor axis of elliptical cross section */
    mat_t	R;	/* rotation matrix */
    mat_t	Rinv;	/* inverse rotation matrix */
    mat_t	S;	/* scale matrix ( c = 1, |a| = 1, |b| = 1 ) */
    fastf_t	a, b, c, sqH;


#ifndef NO_MAGIC_CHECKING
    RT_CK_DB_INTERNAL(ip);
    BN_CK_TOL(tol);
#endif
    hyp_ip = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(hyp_ip);

    /* TODO: check that this is a valid hyperboloid (assume it is, for now)  */

    /* set soltab ID */
    stp->st_id = ID_HYP;
    stp->st_meth = &rt_functab[ID_HYP];

    /* calculate scaled height to use for top/bottom intersection planes */
    hyp->hyp_Hmag = MAGNITUDE( hyp_ip->hyp_H ) / hyp_ip->hyp_c;

    /* setup unit vectors for hyp_specific */
    VMOVE( hyp->hyp_V, hyp_ip->hyp_V );

    VMOVE( unitH, hyp_ip->hyp_H );
    VUNITIZE( unitH );
    VMOVE( hyp->hyp_Hunit, unitH );

    VMOVE( unitB, hyp_ip->hyp_Au );
    VMOVE( hyp->hyp_Aunit, hyp_ip->hyp_Au );

    VCROSS( unitB, unitA, unitH );
    VMOVE( hyp->hyp_Bunit, unitB );

    /* calculate transformation matrix scale(rotate(vect)) */

    MAT_IDN( R );
    VMOVE( &R[0], hyp->hyp_Bunit );
    VMOVE( &R[4], hyp->hyp_Aunit );
    VMOVE( &R[8], hyp->hyp_Hunit );
    bn_mat_trn( Rinv, R );

    a = hyp_ip->hyp_r1;
    b = hyp_ip->hyp_r2;
    c = hyp_ip->hyp_c;
    sqH = MAGSQ( hyp_ip->hyp_H );

    MAT_IDN( S );

    S[0] = 1.0 / b;
    S[5] = 1.0 / a;
    S[10] = 1.0 / c;

    bn_mat_mul( hyp->hyp_SoR, S, R );
    bn_mat_mul( hyp->hyp_invRoS, Rinv, S );

    /* calculate bounding sphere */
    VMOVE( stp->st_center, hyp->hyp_V );
    stp->st_aradius = sqrt((c*c)*sqH + (a*a) + sqH );
    stp->st_bradius = stp->st_aradius;

    /* cheat, make bounding RPP by enclosing bounding sphere (copied from g_ehy.c) */
    stp->st_min[X] = stp->st_center[X] - stp->st_bradius;
    stp->st_max[X] = stp->st_center[X] + stp->st_bradius;
    stp->st_min[Y] = stp->st_center[Y] - stp->st_bradius;
    stp->st_max[Y] = stp->st_center[Y] + stp->st_bradius;
    stp->st_min[Z] = stp->st_center[Z] - stp->st_bradius;
    stp->st_max[Z] = stp->st_center[Z] + stp->st_bradius;

    return(0);			/* OK */

}

/**
 *			R T _ H Y P _ P R I N T
 */
void
rt_hyp_print( const struct soltab *stp )
{
    register const struct hyp_specific *hyp =
	(struct hyp_specific *)stp->st_specific;

    VPRINT( "V", hyp->hyp_V );
    VPRINT( "Hunit", hyp->hyp_Hunit );
    VPRINT( "Aunit", hyp->hyp_Aunit );
    VPRINT( "Bunit", hyp->hyp_Bunit );
    fprintf( stderr, "h = %g\n", hyp->hyp_Hmag );
    bn_mat_print( "S o R", hyp->hyp_SoR );
    bn_mat_print( "invR o S", hyp->hyp_invRoS );

}

/* hit_surfno is set to one of these */
#define	HYP_NORM_BODY	(1)		/* compute normal */
#define	HYP_NORM_TOP	(2)		/* copy hyp_Hunit */
#define HYP_NORM_BOTTOM	(3)		/* copy -hyp_Huint */

/**
 *  			R T _ H Y P _ S H O T
 *
 *  Intersect a ray with a hyp.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_hyp_shot( struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead )
{
    register int numHits = 0;

    register struct hyp_specific *hyp =
	(struct hyp_specific *)stp->st_specific;
    register struct seg *segp;
    const struct bn_tol	*tol = &ap->a_rt_i->rti_tol;

    struct hit	hits[5];	/* 4 potential hits (top, bottom, 2 sides) */
    register struct hit *hitp;	/* pointer to hitpoint */

    vect_t	dp;
    vect_t	pp;
    fastf_t	k1, k2;
    vect_t	xlated;

    fastf_t	a, b, c;
    fastf_t	disc;
    fastf_t	hitX, hitY;

    hitp = &hits[0];


    MAT4X3VEC( dp, hyp->hyp_SoR, rp->r_dir );
    VSUB2( xlated, rp->r_pt, hyp->hyp_V );
    MAT4X3VEC( pp, hyp->hyp_SoR, xlated );

    /* find roots to quadratic (hitpoints) */
    a = dp[X]*dp[X] + dp[Y]*dp[Y] - dp[Z]*dp[Z];
    b = 2.0 * ( pp[X]*dp[X] + pp[Y]*dp[Y] - pp[Z]*dp[Z] );
    c = pp[X]*pp[X] + pp[Y]*pp[Y] - pp[Z]*pp[Z] - 1.0;

    disc = b*b - ( 4.0 * a * c );
    if ( !NEAR_ZERO( a, RT_PCOEF_TOL ) && disc > 0 ) {
	disc = sqrt(disc);

	k1 = (-b + disc) / (2.0 * a);
	k2 = (-b - disc) / (2.0 * a);

	VJOIN1( hitp->hit_vpriv, pp, k1, dp );
	if ( hitp->hit_vpriv[Z] >= -hyp->hyp_Hmag
	    && hitp->hit_vpriv[Z] <= hyp->hyp_Hmag ) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = HYP_NORM_BODY;
	    hitp++;
	    numHits++;
	}

	VJOIN1( hitp->hit_vpriv, pp, k2, dp );
	if ( hitp->hit_vpriv[Z] >= -hyp->hyp_Hmag
	    && hitp->hit_vpriv[Z] <= hyp->hyp_Hmag ) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = HYP_NORM_BODY;
	    hitp++;
	    numHits++;
	}

    } else if ( !NEAR_ZERO( b, RT_PCOEF_TOL ) ) {
	k1 = -c / b;
	VJOIN1( hitp->hit_vpriv, pp, k1, dp );
	if ( hitp->hit_vpriv[Z] >= -hyp->hyp_Hmag
	    && hitp->hit_vpriv[Z] <= hyp->hyp_Hmag ) {
	    hitp->hit_magic = RT_HIT_MAGIC;
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = HYP_NORM_BODY;
	    hitp++;
	    numHits++;
	}
    }

    /* check top & bottom plates */
    k1 = (hyp->hyp_Hmag - pp[Z]) / dp[Z];
    k2 = (-hyp->hyp_Hmag - pp[Z]) / dp[Z];

    VJOIN1( hitp->hit_vpriv, pp, k1, dp );
    hitX = hitp->hit_vpriv[X];
    hitY = hitp->hit_vpriv[Y];
    /* check if hitpoint is on the top surface */
    if ( (hitX*hitX + hitY*hitY) < (hyp->hyp_Hmag * hyp->hyp_Hmag + 1.0) ) {
	hitp->hit_magic = RT_HIT_MAGIC;
	hitp->hit_dist = k1;
	hitp->hit_surfno = HYP_NORM_BODY;
	hitp++;
	numHits++;
    }

    VJOIN1( hitp->hit_vpriv, pp, k2, dp );
    hitX = hitp->hit_vpriv[X];
    hitY = hitp->hit_vpriv[Y];
    /* check if hitpoint is on the bottom surface */
    if ( (hitX*hitX + hitY*hitY) < (hyp->hyp_Hmag * hyp->hyp_Hmag + 1.0) ) {
	hitp->hit_magic = RT_HIT_MAGIC;
	hitp->hit_dist = k2;
	hitp->hit_surfno = HYP_NORM_BODY;
	hitp++;
	numHits++;
    }

    /* bu_log("numHits: %d\n", numHits); */

    if ( hitp == &hits[0] ) {
	return(0);	/* MISS */
    }

    if ( hitp == &hits[2] ) {	/* 2 hits */
	if ( hits[0].hit_dist < hits[1].hit_dist )  {
	    /* entry is [0], exit is [1] */
	    register struct seg *segp;

	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;
	    segp->seg_in = hits[0];	/* struct copy */
	    segp->seg_out = hits[1];	/* struct copy */
	    BU_LIST_INSERT( &(seghead->l), &(segp->l) );
	} else {
	    /* entry is [1], exit is [0] */
	    register struct seg *segp;

	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;
	    segp->seg_in = hits[1];	/* struct copy */
	    segp->seg_out = hits[0];	/* struct copy */
	    BU_LIST_INSERT( &(seghead->l), &(segp->l) );
	}
	return(2);			/* HIT */
    } else {	/* 4 hits */
	/* merge sort list to find segments */
	struct hit sorted[4];
	struct hit *temp;
	int i = 0, j = 2, k = 0;
	register struct seg *segp;

	if ( hits[0].hit_dist > hits[1].hit_dist ) {
	    *temp = hits[0];
	    hits[0] = hits[1];
	    hits[1] = *temp;
	}
	if ( hits[2].hit_dist > hits[3].hit_dist ) {
	    *temp = hits[2];
	    hits[2] = hits[3];
	    hits[3] = *temp;
	}

	while ( i < 2 && j < 4 ) {
	    if ( hits[i].hit_dist < hits[j].hit_dist )
		sorted[k++] = hits[i++];
	    else
		sorted[k++] = hits[j++];
	}
	while ( i < 2 ) sorted[k++] = hits[i++];
	while ( j < 4 ) sorted[k++] = hits[j++];

	/* hit segments are now (0,1) and (2,3) */
	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in = sorted[0];	/* struct copy */
	segp->seg_out = sorted[1];	/* struct copy */
	BU_LIST_INSERT( &(seghead->l), &(segp->l) );

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in = sorted[2];	/* struct copy */
	segp->seg_out = sorted[3];	/* struct copy */
	BU_LIST_INSERT( &(seghead->l), &(segp->l) );

	return(4);
    }
}

#define RT_HYP_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/**
 *			R T _ H Y P _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_hyp_vshot(struct soltab *stp[],	/* An array of solid pointers */
	     struct xray *rp[],		/* An array of ray pointers */
	     struct seg segp[],		/* array of segs (results returned) */
	     int n,			/* Number of ray/object pairs */
	     struct application *ap)
{
    rt_vstub( stp, rp, segp, n, ap );
}

/**
 *  			R T _ H Y P _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_hyp_norm( struct hit *hitp, struct soltab *stp, struct xray *rp )
{
    register struct hyp_specific *hyp =
	(struct hyp_specific *)stp->st_specific;

    fastf_t	dzdx, dzdy, x, y, z;
    /* normal from basic hyperboloid and transformed normal */
    vect_t	n, nT;

    VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
    switch ( hitp->hit_surfno ) {
	case HYP_NORM_TOP:
	    VMOVE( hitp->hit_normal, hyp->hyp_Hunit );
	    break;
	case HYP_NORM_BOTTOM:
	    VREVERSE( hitp->hit_normal, hyp->hyp_Hunit );
	    break;
	case HYP_NORM_BODY:
	    /* normal vector is VUNITIZE( -dz/dx, -dz/dy, +-1 ) */
	    /* z = +- sqrt( x^2 + y^2 -1) */
	    x = hitp->hit_vpriv[X];
	    y = hitp->hit_vpriv[Y];
	    z = hitp->hit_vpriv[Z];
	    if ( NEAR_ZERO(z, SMALL_FASTF) ) {
		/* near z==0, the norm is in the x-y plane */
		VSET( n, x, y, 0 );
	    } else {
		dzdx = x / z;
		dzdy = y / z;
		if ( z > 0 ) {
		    VSET( n, dzdx, dzdy, -1.0);
		} else {
		    VSET( n, -dzdx, -dzdy, 1.0);
		}
	    }
	    MAT4X3VEC( nT, hyp->hyp_invRoS, n);
	    VUNITIZE( nT );
	    VMOVE( hitp->hit_normal, nT );
	    break;
	default:
	    bu_log("rt_hyp_norm: surfno=%d bad\n", hitp->hit_surfno);
	    break;
    }
}

/**
 *			R T _ H Y P _ C U R V E
 *
 *  Return the curvature of the hyp.
 */
void
rt_hyp_curve( struct curvature *cvp, struct hit *hitp, struct soltab *stp )
{
    register struct hyp_specific *hyp =
	(struct hyp_specific *)stp->st_specific;

    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/**
 *  			R T _ H Y P _ U V
 *
 *  For a hit on the surface of an hyp, return the (u, v) coordinates
 *  of the hit point, 0 <= u, v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_hyp_uv( struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp )
{
    register struct hyp_specific *hyp =
	(struct hyp_specific *)stp->st_specific;

    fastf_t	x, y, rSq;

    /* u = (angle from semi-major axis on basic hyperboloid) / (2*pi) */
    uvp->uv_u = (atan2(hitp->hit_vpriv[X], hitp->hit_vpriv[Y]) + M_PI) * 0.5 * M_1_PI;

    /* (0,0.25) = bottom, (0.25,0.75) = side, (0.75,1.0) = top */
    switch( hitp->hit_surfno ) {
	case HYP_NORM_BODY:
	    /* v = (z + Hmag) / (2*Hmag) */
	    uvp->uv_v = 0.25 + (hitp->hit_vpriv[Z] + hyp->hyp_Hmag) / (4.0 * hyp->hyp_Hmag);
	    break;
	case HYP_NORM_TOP:
	    x = hitp->hit_vpriv[X];
	    y = hitp->hit_vpriv[Y];
	    rSq = 1.0 + (hyp->hyp_Hmag * hyp->hyp_Hmag);
	    uvp->uv_v = 1.0 - 0.25 * sqrt( (x*x + y*y) / rSq );
	    break;
	case HYP_NORM_BOTTOM:
	    x = hitp->hit_vpriv[X];
	    y = hitp->hit_vpriv[Y];
	    rSq = 1.0 + (hyp->hyp_Hmag * hyp->hyp_Hmag);
	    uvp->uv_v = 0.25 * sqrt( (x*x + y*y) / rSq );
	    break;
    }
    /* copied from g_ehy.c */
    uvp->uv_du = uvp->uv_dv = 0;
}

/**
 *		R T _ H Y P _ F R E E
 */
void
rt_hyp_free( struct soltab *stp )
{
    register struct hyp_specific *hyp =
	(struct hyp_specific *)stp->st_specific;

    bu_free( (char *)hyp, "hyp_specific" );
}

/**
 *			R T _ H Y P _ C L A S S
 */
int
rt_hyp_class( const struct soltab *stp, const vect_t min, const vect_t max, const struct bn_tol *tol )
{
    return(0);
}
/**
 *			R T _ H Y P _ P L O T
 */
int
rt_hyp_plot( struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol )
{
    register int		i,j;	/* loop indices */
    struct rt_hyp_internal	*hyp_ip;
    vect_t 	majorAxis[8],		/* vector offsets along major axis */
		minorAxis[8],		/* vector offsets along minor axis */
		heightAxis[7],		/* vector offsets for layers */
		Bunit;		/* unit vector along semi-minor axis */
    vect_t	ell[16];	/* stores 16 points to draw ellipses */
    vect_t	ribs[16][7];	/* assume 7 layers for now */
    fastf_t 	scale;		/* used to calculate semi-major/minor axes for top/bottom */
    fastf_t	cos22_5 = 0.9238795325112867385,
		cos67_5 = 0.3826834323650898373;

    RT_CK_DB_INTERNAL(ip);
    hyp_ip = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(hyp_ip);

    VCROSS( Bunit, hyp_ip->hyp_H, hyp_ip->hyp_Au );
    VUNITIZE( Bunit );

    VMOVE( heightAxis[0], hyp_ip->hyp_H );
    VSCALE( heightAxis[1], heightAxis[0], 0.5 );
    VSCALE( heightAxis[2], heightAxis[0], 0.25 );
    VSETALL( heightAxis[3], 0 );
    VREVERSE( heightAxis[4], heightAxis[2] );
    VREVERSE( heightAxis[5], heightAxis[1] );
    VREVERSE( heightAxis[6], heightAxis[0] );

    for ( i = 0; i < 7; i++ ) {
	/* determine Z height depending on i */
	scale = sqrt(MAGSQ( heightAxis[i] )*(hyp_ip->hyp_c * hyp_ip->hyp_c)/(hyp_ip->hyp_r1 * hyp_ip->hyp_r1) + 1 );

	/* calculate vectors for offset */
	VSCALE( majorAxis[0], hyp_ip->hyp_Au, hyp_ip->hyp_r1 * scale );
	VSCALE( majorAxis[1], majorAxis[0], cos22_5 );
	VSCALE( majorAxis[2], majorAxis[0], M_SQRT1_2 );
	VSCALE( majorAxis[3], majorAxis[0], cos67_5 );
	VREVERSE( majorAxis[4], majorAxis[3] );
	VREVERSE( majorAxis[5], majorAxis[2] );
	VREVERSE( majorAxis[6], majorAxis[1] );
	VREVERSE( majorAxis[7], majorAxis[0] );

	VSCALE( minorAxis[0], Bunit, hyp_ip->hyp_r2 * scale );
	VSCALE( minorAxis[1], minorAxis[0], cos22_5 );
	VSCALE( minorAxis[2], minorAxis[0], M_SQRT1_2 );
	VSCALE( minorAxis[3], minorAxis[0], cos67_5 );
	VREVERSE( minorAxis[4], minorAxis[3] );
	VREVERSE( minorAxis[5], minorAxis[2] );
	VREVERSE( minorAxis[6], minorAxis[1] );
	VREVERSE( minorAxis[7], minorAxis[0] );

	/* calculate ellipse */
	VADD3( ell[ 0], hyp_ip->hyp_V, heightAxis[i], majorAxis[0] );
	VADD4( ell[ 1], hyp_ip->hyp_V, heightAxis[i], majorAxis[1], minorAxis[3] );
	VADD4( ell[ 2], hyp_ip->hyp_V, heightAxis[i], majorAxis[2], minorAxis[2] );
	VADD4( ell[ 3], hyp_ip->hyp_V, heightAxis[i], majorAxis[3], minorAxis[1] );
	VADD3( ell[ 4], hyp_ip->hyp_V, heightAxis[i], minorAxis[0] );
	VADD4( ell[ 5], hyp_ip->hyp_V, heightAxis[i], majorAxis[4], minorAxis[1] );
	VADD4( ell[ 6], hyp_ip->hyp_V, heightAxis[i], majorAxis[5], minorAxis[2] );
	VADD4( ell[ 7], hyp_ip->hyp_V, heightAxis[i], majorAxis[6], minorAxis[3] );
	VADD3( ell[ 8], hyp_ip->hyp_V, heightAxis[i], majorAxis[7] );
	VADD4( ell[ 9], hyp_ip->hyp_V, heightAxis[i], majorAxis[6], minorAxis[4] );
	VADD4( ell[10], hyp_ip->hyp_V, heightAxis[i], majorAxis[5], minorAxis[5] );
	VADD4( ell[11], hyp_ip->hyp_V, heightAxis[i], majorAxis[4], minorAxis[6] );
	VADD3( ell[12], hyp_ip->hyp_V, heightAxis[i], minorAxis[7] );
	VADD4( ell[13], hyp_ip->hyp_V, heightAxis[i], majorAxis[3], minorAxis[6] );
	VADD4( ell[14], hyp_ip->hyp_V, heightAxis[i], majorAxis[2], minorAxis[5] );
	VADD4( ell[15], hyp_ip->hyp_V, heightAxis[i], majorAxis[1], minorAxis[4] );

	/* draw ellipse */
	RT_ADD_VLIST( vhead, ell[15], BN_VLIST_LINE_MOVE );
	for ( j=0; j<16; j++ )  {
	    RT_ADD_VLIST( vhead, ell[j], BN_VLIST_LINE_DRAW );
	}

	/* add ellipse's points to ribs */
	for ( j=0; j<16; j++ ) {
	    VMOVE( ribs[j][i], ell[j] );
	}
    }

    /* draw ribs */
    for ( i=0; i<16; i++ )  {
	RT_ADD_VLIST( vhead, ribs[i][0], BN_VLIST_LINE_MOVE );
	for ( j=1; j<7; j++ )  {
	    RT_ADD_VLIST( vhead, ribs[i][j], BN_VLIST_LINE_DRAW );
	}

    }

    return 0;
}

/**
 *			R T _ H Y P _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_hyp_tess( struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol )
{
    struct rt_hyp_internal	*hyp_ip;

    RT_CK_DB_INTERNAL(ip);
    hyp_ip = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(hyp_ip);

    return(-1);
}


/**
 *			R T _ H Y P _ I M P O R T 5
 *
 *  Import an HYP from the database format to the internal format.
 *  Note that the data read will be in network order.  This means
 *  Big-Endian integers and IEEE doubles for floating point.
 *
 *  Apply modeling transformations as well.
 *
 */
int
rt_hyp_import5( struct rt_db_internal  *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip )
{
    struct rt_hyp_internal	*hyp_ip;
    fastf_t			vec[ELEMENTS_PER_VECT*4];

    RT_CK_DB_INTERNAL(ip)
	BU_CK_EXTERNAL( ep );

    BU_ASSERT_LONG( ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 3 * 4 );

    /* set up the internal structure */
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_HYP;
    ip->idb_meth = &rt_functab[ID_HYP];
    ip->idb_ptr = bu_malloc( sizeof(struct rt_hyp_internal), "rt_hyp_internal");
    hyp_ip = (struct rt_hyp_internal *)ip->idb_ptr;
    hyp_ip->hyp_magic = RT_HYP_INTERNAL_MAGIC;

    /* Convert the data in ep->ext_buf into internal format.
     * Note the conversion from network data
     * (Big Endian ints, IEEE double floating point) to host local data
     * representations.
     */
    ntohd( (unsigned char *)&vec, (const unsigned char *)ep->ext_buf, ELEMENTS_PER_VECT*4 );

    /* Apply the modeling transformation */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT( hyp_ip->hyp_V, mat, &vec[0*3] );
    MAT4X3PNT( hyp_ip->hyp_H, mat, &vec[1*3] );
    MAT4X3PNT( hyp_ip->hyp_Au, mat, &vec[2*3] );
    VUNITIZE( hyp_ip->hyp_Au );

    hyp_ip->hyp_r1 = vec[ 9] / mat[15];
    hyp_ip->hyp_r2 = vec[10] / mat[15];
    hyp_ip->hyp_c  = vec[11] / mat[15];

    return(0);			/* OK */
}

/**
 *			R T _ H Y P _ E X P O R T 5
 *
 *  Export an HYP from internal form to external format.
 *  Note that this means converting all integers to Big-Endian format
 *  and floating point data to IEEE double.
 *
 *  Apply the transformation to mm units as well.
 */
int
rt_hyp_export5( struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip )
{
    struct rt_hyp_internal	*hyp_ip;
    fastf_t			vec[ELEMENTS_PER_VECT * 4];

    RT_CK_DB_INTERNAL(ip);
    if ( ip->idb_type != ID_HYP )  return(-1);
    hyp_ip = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(hyp_ip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT * 4;
    ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "hyp external");


    /* Since libwdb users may want to operate in units other
     * than mm, we offer the opportunity to scale the solid
     * (to get it into mm) on the way out.
     */
    VSCALE( &vec[0*3], hyp_ip->hyp_V, local2mm );
    VSCALE( &vec[1*3], hyp_ip->hyp_H, local2mm );
    VMOVE( &vec[2*3], hyp_ip->hyp_Au );
    vec[ 9] = hyp_ip->hyp_r1 * local2mm;
    vec[10] = hyp_ip->hyp_r2 * local2mm;
    vec[11] = hyp_ip->hyp_c * local2mm;

    /* Convert from internal (host) to database (network) format */
    htond( ep->ext_buf, (unsigned char *)vec, ELEMENTS_PER_VECT*4 );

    return 0;
}

/**
 *			R T _ H Y P _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_hyp_describe( struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local )
{
    register struct rt_hyp_internal	*hyp_ip =
	(struct rt_hyp_internal *)ip->idb_ptr;
    char	buf[256];

    RT_HYP_CK_MAGIC(hyp_ip);
    bu_vls_strcat( str, "truncated general hyp (HYP)\n");

    sprintf(buf, "\tV (%g, %g, %g)\n",
	    INTCLAMP(hyp_ip->hyp_V[X] * mm2local),
	    INTCLAMP(hyp_ip->hyp_V[Y] * mm2local),
	    INTCLAMP(hyp_ip->hyp_V[Z] * mm2local) );
    bu_vls_strcat( str, buf );

    sprintf(buf, "\tH (%g, %g, %g) mag=%g\n",
	    INTCLAMP(hyp_ip->hyp_H[X] * mm2local),
	    INTCLAMP(hyp_ip->hyp_H[Y] * mm2local),
	    INTCLAMP(hyp_ip->hyp_H[Z] * mm2local),
	    INTCLAMP(MAGNITUDE(hyp_ip->hyp_H) * mm2local) );
    bu_vls_strcat( str, buf );

    sprintf(buf, "\tA=%g\n", INTCLAMP(hyp_ip->hyp_r1 * mm2local));
    bu_vls_strcat( str, buf );

    sprintf(buf, "\tB=%g\n", INTCLAMP(hyp_ip->hyp_r2 * mm2local));
    bu_vls_strcat( str, buf );

    sprintf(buf, "\tc=%g\n", INTCLAMP(hyp_ip->hyp_c * mm2local));
    bu_vls_strcat( str, buf );


    return(0);
}

/**
 *			R T _ H Y P _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_hyp_ifree( struct rt_db_internal *ip )
{
    register struct rt_hyp_internal	*hyp_ip;

    RT_CK_DB_INTERNAL(ip);
    hyp_ip = (struct rt_hyp_internal *)ip->idb_ptr;
    RT_HYP_CK_MAGIC(hyp_ip);
    hyp_ip->hyp_magic = 0;			/* sanity */

    bu_free( (char *)hyp_ip, "hyp ifree" );
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

/**
 *			R T _ H Y P _ X F O R M
 *
 *  Create transformed version of internal form.  Free *ip if requested.
 *  Implement this if it's faster than doing an export/import cycle.
 */

int
rt_hyp_xform( struct rt_db_internal *op, const mat_t mat, struct rt_db_internal *ip, int free )
{
    return(0);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
