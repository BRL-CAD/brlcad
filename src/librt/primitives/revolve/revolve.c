/*                           R E V O L V E . C
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
/** @addtogroup primitives */
/** @{ */
/** @file revolve.c
 *
 * Intersect a ray with an 'revolve' primitive object.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"

/* local interface header */
#include "./revolve.h"


#define START_FACE	1
#define END_FACE	2
#define LINE_SEG	3
#define CARC_SEG	4
#define NURB_SEG	5
#define BEZIER_SEG	6

#define MAX_HITS	64

/**
 * R T _ R E V O L V E _ P R E P
 *
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid REVOLVE, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 *	0	REVOLVE is OK
 *	!0	Error in description
 *
 * Implicit return -
 *	A struct revolve_specific is created, and it's address is stored
 *	in stp->st_specific for use by revolve_shot().
 */
int
rt_revolve_prep( struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip )
{
    struct rt_revolve_internal		*rip;
    register struct revolve_specific	*rev;
    const struct bn_tol		*tol = &rtip->rti_tol;

    RT_CK_DB_INTERNAL(ip);
    rip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);

    stp->st_id = ID_REVOLVE;
    stp->st_meth = &rt_functab[ID_REVOLVE];

    BU_GETSTRUCT( rev, revolve_specific );
    stp->st_specific = (genptr_t)rev;

    VMOVE( rev->v3d, rip->v3d );
    VMOVE( rev->zUnit, rip->axis3d );
    VMOVE( rev->xUnit, rip->r );
    VCROSS( rev->yUnit, rev->zUnit, rev->xUnit );

    VUNITIZE( rev->xUnit );
    VUNITIZE( rev->yUnit );
    VUNITIZE( rev->zUnit );

    rev->ang = rip->ang;
    rev->sketch_name = rip->sketch_name;
    rev->sk = rip->sk;

/* calculate end plane */


/* FIXME: bounding volume */
    VMOVE( stp->st_center, rev->v3d );
    stp->st_aradius = 100.0;
    stp->st_bradius = 100.0;

    /* cheat, make bounding RPP by enclosing bounding sphere (copied from g_ehy.c) */
    stp->st_min[X] = stp->st_center[X] - stp->st_bradius;
    stp->st_max[X] = stp->st_center[X] + stp->st_bradius;
    stp->st_min[Y] = stp->st_center[Y] - stp->st_bradius;
    stp->st_max[Y] = stp->st_center[Y] + stp->st_bradius;
    stp->st_min[Z] = stp->st_center[Z] - stp->st_bradius;
    stp->st_max[Z] = stp->st_center[Z] + stp->st_bradius;

    bu_log("rt_revolve_prep\n");

}

/**
 * R T _ R E V O L V E _ P R I N T
 */
void
rt_revolve_print( const struct soltab *stp )
{
    register const struct revolve_specific *revolve =
	(struct revolve_specific *)stp->st_specific;
}

/**
 * R T _ R E V O L V E _ S H O T
 *
 * Intersect a ray with a revolve.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 *	0	MISS
 *	>0	HIT
 */
int
rt_revolve_shot( struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead )
{
    register struct revolve_specific *rev =
	(struct revolve_specific *)stp->st_specific;
    register struct seg *segp;
    const struct bn_tol	*tol = &ap->a_rt_i->rti_tol;

    register struct hit *hitp;
    struct hit		*hits[MAX_HITS], hit[MAX_HITS];

    int 	i, j, nseg, nhits, in, out;

    fastf_t	k, m;
    point_t	dp, pr, xlated;
    vect_t	vr, ua, ur, norm, temp;
    fastf_t	h, aa, bb;

    nhits = 0;

    for ( i=0; i<MAX_HITS; i++ ) hits[i] = &hit[i];

    vr[X] = VDOT( rev->xUnit, rp->r_dir );
    vr[Y] = VDOT( rev->yUnit, rp->r_dir );
    vr[Z] = VDOT( rev->zUnit, rp->r_dir ); 

    VSUB2( xlated, rp->r_pt, rev->v3d );
    pr[X] = VDOT( rev->xUnit, xlated );
    pr[Y] = VDOT( rev->yUnit, xlated );
    pr[Z] = VDOT( rev->zUnit, xlated );

    VMOVE( ur, vr );
    VUNITIZE( ur );

/* find two plane intersections if ang != 2pi */
    /* assume ang == 2pi for now */

/* calculate hyperbola parameters */
    VREVERSE( dp, pr);

    VSET( norm, ur[X], ur[Y], 0 );

    k = VDOT( dp, norm ) / VDOT( ur, norm );

    h = pr[Z] + k*vr[Z];

    aa = sqrt( (pr[X] + k*vr[X])*(pr[X] + k*vr[X]) + (pr[Y] + k*vr[Y])*(pr[Y] + k*vr[Y]) );
    bb = sqrt( aa*aa * ( 1.0/(1 - ur[Z]*ur[Z]) - 1.0 ) );

/* 
	[ (x*x) / aa^2 ] - [ (y-h)^2 / bb^2 ] = 1

	x = aa cosh( t - k );
	y = h + bb sinh( t - k );
*/

/* find hyperbola intersection with each sketch segment */
    nseg = rev->sk->skt_curve.seg_count;
    for( i=0; i<nseg; i++ ) {
	long *lng = (long *)rev->sk->skt_curve.segments[i];
	struct line_seg *lsg;

	vect_t	dir;
	point_t	pt, pt2, hit1, hit2;
	fastf_t a, b, c, disc, k1, k2, t1, t2, x, y;
	fastf_t xmin, xmax, ymin, ymax;

	switch ( *lng ) {
	    case CURVE_LSEG_MAGIC:
		lsg = (struct line_seg *)lng;
		VMOVE( pt, rev->sk->verts[lsg->start] );
		VMOVE( pt2, rev->sk->verts[lsg->end] );
		VSUB2( dir, pt2, pt );
		m = dir[Y] / dir[X];

		a = dir[X]*dir[X]/(aa*aa) - dir[Y]*dir[Y]/(bb*bb);
		b = 2*(dir[X]*pt[X]/(aa*aa) - dir[Y]*(pt[Y]-h)/(bb*bb));
		c = pt[X]*pt[X]/(aa*aa) - (pt[Y]-h)*(pt[Y]-h)/(bb*bb) - 1;
		disc = b*b - ( 4.0 * a * c );
		if ( !NEAR_ZERO( a, RT_PCOEF_TOL ) ) {
		    if ( disc > 0 ) {
			disc = sqrt( disc );
			t1 =  (-b + disc) / (2.0 * a);
			t2 =  (-b - disc) / (2.0 * a);
			k1 = (pt[Y]-pr[Z] + t1*dir[Y])/vr[Z];
			k2 = (pt[Y]-pr[Z] + t2*dir[Y])/vr[Z];

			if ( t1 > 0 && t1 < 1 ) {
			    if ( nhits >= MAX_HITS ) return -1; /* too many hits */
			    hitp = hits[nhits++];
			    VJOIN1( hitp->hit_vpriv, pr, k1, vr );
			    hitp->hit_vpriv[Z] = -1.0/m;
			    hitp->hit_magic = RT_HIT_MAGIC;
			    hitp->hit_dist = k1;
			    hitp->hit_surfno = LINE_SEG;
			}
			if ( t2 > 0 && t2 < 1 ) {
			    if ( nhits >= MAX_HITS ) return -1; /* too many hits */
			    hitp = hits[nhits++];
			    VJOIN1( hitp->hit_vpriv, pr, k2, vr );
			    hitp->hit_vpriv[Z] = -1.0/m;
			    hitp->hit_magic = RT_HIT_MAGIC;
			    hitp->hit_dist = k2;
			    hitp->hit_surfno = LINE_SEG;
			}
		    }
		} else if ( !NEAR_ZERO( b, RT_PCOEF_TOL ) ) {
		    t1 = -c / b;
		    k1 = (pt[Y]-pr[Z] + t1*dir[Y])/vr[Z];
		    if ( t1 > 0 && t1 < 1 ) {
			if ( nhits >= MAX_HITS ) return -1; /* too many hits */
			hitp = hits[nhits++];
			VJOIN1( hitp->hit_vpriv, pr, k1, vr );
			hitp->hit_vpriv[Z] = -1.0/m;
			hitp->hit_magic = RT_HIT_MAGIC;
			hitp->hit_dist = k1;
			hitp->hit_surfno = LINE_SEG;
		    }
		}
#if 0
		if ( pt[X] > pt2[X] ) {
		    xmax = pt[X];
		    xmin = pt2[X];
		} else {
		    xmax = pt2[X];
		    xmin = pt[X];
		}
		if ( pt[Y] > pt2[Y] ) {
		    ymax = pt[Y];
		    ymin = pt2[Y];
		} else {
		    ymax = pt2[Y];
		    ymin = pt[Y];
		}

		a = 0.5 * (dir[X]*bb - dir[Y]*aa);
		b = (dir[Y]*pt[X]) - dir[X]*(pt[Y] - h);
		c = -0.5 * (dir[X]*bb + dir[Y]*aa);
		disc = b*b - ( 4.0 * a * c );

		if ( !NEAR_ZERO( a, RT_PCOEF_TOL ) ) {
		    if ( disc > 0 ) {
			disc = sqrt( disc );
			t1 = log( fabs( (-b + disc) / (2.0 * a) ) );
			t2 = log( fabs( (-b - disc) / (2.0 * a) ) );
			k1 = (h + bb*sinh(t1) - pr[Z]) / vr[Z];
			k2 = (h + bb*sinh(t2) - pr[Z]) / vr[Z];
			x = aa*cosh( t1 );
			y = h + bb*sinh( t1 );
			if ( x > xmin && x < xmax && y > ymin && y < ymax ) {
			    if ( nhits >= MAX_HITS ) return -1; /* too many hits */
			    VJOIN1( temp, pr, k1, vr );
			    if ( !NEAR_ZERO( x*x - temp[X]*temp[X] - temp[Y]*temp[Y], RT_LEN_TOL ) ||
					!NEAR_ZERO( y - temp[Z], RT_LEN_TOL ) ) {
			        bu_log( "2D point: \t\t%5.2f\t%5.2f\n3D point: \t%5.2f\t%5.2f\t%5.2f\n",
					x, y, temp[X], temp[Y], temp[Z] );
			    }
			    hitp = hits[nhits++];
			    VJOIN1( hitp->hit_vpriv, pr, k1, vr );
			    hitp->hit_magic = RT_HIT_MAGIC;
			    hitp->hit_dist = t1;
			    hitp->hit_surfno = LINE_SEG;
			}
			x = aa*cosh( t2 );
			y = h + bb*sinh( t2 );
			if ( x > xmin && x < xmax && y > ymin && y < ymax ) {
			    if ( nhits >= MAX_HITS ) return -1; /* too many hits */
			    VJOIN1( temp, pr, k2, vr );
			    if ( !NEAR_ZERO( x*x - temp[X]*temp[X] - temp[Y]*temp[Y], RT_LEN_TOL ) ||
					!NEAR_ZERO( y - temp[Z], RT_LEN_TOL ) ) {
			        bu_log( "2D point: \t\t%5.2f\t%5.2f\n3D point: \t%5.2f\t%5.2f\t%5.2f\n",
					x, y, temp[X], temp[Y], temp[Z] );
			    }
			    hitp = hits[nhits++];
			    VJOIN1( hitp->hit_vpriv, pr, k2, vr );
			    hitp->hit_magic = RT_HIT_MAGIC;
			    hitp->hit_dist = k2;
			    hitp->hit_surfno = LINE_SEG;
			}
		    }
		} 

else if ( !NEAR_ZERO( b, RT_PCOEF_TOL ) ) {
		    t1 = log( -c / b );
		    k1 = (h + bb*sinh(t1) - pr[Z]) / vr[Z];
		    x = aa*cosh( t1 );
		    y = h + bb*sinh( t1 );
		    if ( x > xmin && x < xmax && y > ymin && y < ymax ) {
			if ( nhits >= MAX_HITS ) return -1; /* too many hits */
			VJOIN1( temp, pr, k1, vr );
			if ( !NEAR_ZERO( x*x - temp[X]*temp[X] - temp[Y]*temp[Y], RT_LEN_TOL ) ||
				!NEAR_ZERO( y - temp[Z], RT_LEN_TOL ) ) {
			    bu_log( "2D point: \t\t%5.2f\t%5.2f\n3D point: \t%5.2f\t%5.2f\t%5.2f\n",
				x, y, temp[X], temp[Y], temp[Z] );
			}
			hitp = hits[nhits++];
			VJOIN1( hitp->hit_vpriv, pr, k1, vr );
			hitp->hit_magic = RT_HIT_MAGIC;
			hitp->hit_dist = k1;
			hitp->hit_surfno = LINE_SEG;
		    }
		}
#endif
		break;
	}

    }

    if ( nhits%2 != 0 ) {
	bu_log( "odd number of hits: %d\n", nhits );
	return -1;
    }

#if 0
    if ( nhits > 1 ) {
	hitp = hits[0];
	for ( i=0; i<nhits; i++) {
	    if ( hits[i]->hit_dist < hitp->hit_dist ) {
		hitp = hits[i];
	    }
	}
	bu_log("radius:\t%5.2f", 
		sqrt( hitp->hit_vpriv[X]*hitp->hit_vpriv[X] +
			hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y] ) );
	/*bu_log( "hits: %d\n", nhits );*/
    }
#endif

/* sort hitpoints (an arbitrary number of hits depending on sketch) */
    /* horribly inefficient sort for now... */
    for ( i=0; i<nhits; i+=2 ) {
	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	in = out = -1;
	for ( j=0; j<nhits; j++ ) {
	    if ( hits[j] == NULL ) continue;
	    if ( in == -1 ) {
		in = j;
		continue;
	    }
	    /* store shortest dist as 'in', second shortest as 'out' */
	    if ( hits[j]->hit_dist <= hits[in]->hit_dist ) {
		out = in;
		in = j;
	    } else if ( out == -1 || hits[j]->hit_dist <= hits[out]->hit_dist ) {
		out = j;
	    }
	}
	segp->seg_in = *hits[in];
	hits[in] = NULL;
	segp->seg_out = *hits[out];
	hits[out] = NULL;
	BU_LIST_INSERT( &(seghead->l), &(segp->l) );
    }

    return nhits;

/* the EXAMPLE_NEW_SEGMENT block shows how one might add a new result
 * if the ray did hit the primitive.  the segment values would need to
 * be adjusted accordingly to match real values instead of -1.
 */
#if 0
    /* allocate a segment */
    RT_GET_SEG(segp, ap->a_resource);
    segp->seg_stp = stp; /* stash a pointer to the primitive */

    segp->seg_in.hit_dist = -1; /* REVOLVE set to real distance to entry point */
    segp->seg_out.hit_dist = -1; /* REVOLVE set to real distance to exit point */
    segp->seg_in.hit_surfno = -1; /* REVOLVE set to a non-negative ID for entry surface */
    segp->seg_out.hit_surfno = -1;  /* REVOLVE set to a non-negative ID for exit surface */

    /* add segment to list of those encountered for this primitive */
    BU_LIST_INSERT( &(seghead->l), &(segp->l) );

    return(2); /* num surface intersections == in + out == 2 */
#endif

}

#define RT_REVOLVE_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL


/**
 * R T _ R E V O L V E _ V S H O T
 *
 * Vectorized version.
 */
void
rt_revolve_vshot(struct soltab *stp[],	/* An array of solid pointers */
	     struct xray *rp[],		/* An array of ray pointers */
	     struct seg segp[],		/* array of segs (results returned) */
	     int n,			/* Number of ray/object pairs */
	     struct application *ap)
{
    rt_vstub( stp, rp, segp, n, ap );
}

/**
 * R T _ R E V O L V E _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_revolve_norm( struct hit *hitp, struct soltab *stp, struct xray *rp )
{
    register struct revolve_specific *rev =
	(struct revolve_specific *)stp->st_specific;
    vect_t	n, nT;

	/* currently only does normal component in x-y plane */
/*    VJOIN1( n, rp->r_pt, hitp->hit_dist, rp->r_dir );
    VSUB2( hitp->hit_normal, n, rev->v3d );
    VUNITIZE( hitp->hit_normal ); 
*/
 /*   bu_log("\t%5.2f\n", 
	sqrt( hitp->hit_vpriv[X]*hitp->hit_vpriv[X] +
		hitp->hit_vpriv[Y]*hitp->hit_vpriv[Y] ) );
*/
    VSET( n, hitp->hit_vpriv[X], hitp->hit_vpriv[Y], 0 );
    n[Z] = MAGNITUDE( n ) * hitp->hit_vpriv[Z];

    nT[X] = ( rev->xUnit[X] * n[X] )
	  + ( rev->yUnit[X] * n[Y] )
	  + ( rev->zUnit[X] * n[Z] );
    nT[Y] = ( rev->xUnit[Y] * n[X] )
	  + ( rev->yUnit[Y] * n[Y] )
	  + ( rev->zUnit[Y] * n[Z] );
    nT[Z] = ( rev->xUnit[Z] * n[X] )
	  + ( rev->yUnit[Z] * n[Y] )
	  + ( rev->zUnit[Z] * n[Z] );
    VUNITIZE( nT );
    VMOVE( hitp->hit_normal, nT );

/*    VREVERSE( hitp->hit_normal, rp->r_dir );
*/
}

/**
 * R T _ R E V O L V E _ C U R V E
 *
 * Return the curvature of the revolve.
 */
void
rt_revolve_curve( struct curvature *cvp, struct hit *hitp, struct soltab *stp )
{
    register struct revolve_specific *revolve =
	(struct revolve_specific *)stp->st_specific;

    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/**
 * R T _ R E V O L V E _ U V
 *
 * For a hit on the surface of an revolve, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.

 * u = azimuth,  v = elevation
 */
void
rt_revolve_uv( struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp )
{
    register struct revolve_specific *revolve =
	(struct revolve_specific *)stp->st_specific;
}

/**
 * R T _ R E V O L V E _ F R E E
 */
void
rt_revolve_free( struct soltab *stp )
{
    register struct revolve_specific *revolve =
	(struct revolve_specific *)stp->st_specific;

    bu_free( (char *)revolve, "revolve_specific" );
}

/**
 * R T _ R E V O L V E _ C L A S S
 */
int
rt_revolve_class( const struct soltab *stp, const vect_t min, const vect_t max, const struct bn_tol *tol )
{
    return 0;
}

/**
 * R T _ R E V O L V E _ P L O T
 */
int
rt_revolve_plot( struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol )
{
    struct rt_revolve_internal	*revolve_ip;

    RT_CK_DB_INTERNAL(ip);
    revolve_ip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(revolve_ip);

    return(0);
}

/**
 * R T _ R E V O L V E _ T E S S
 *
 * Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_revolve_tess( struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol )
{
    struct rt_revolve_internal	*revolve_ip;

    RT_CK_DB_INTERNAL(ip);
    revolve_ip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(revolve_ip);

    return(-1);
}


/**
 * R T _ R E V O L V E _ I M P O R T 5
 *
 * Import an REVOLVE from the database format to the internal format.
 * Note that the data read will be in network order.  This means
 * Big-Endian integers and IEEE doubles for floating point.
 *
 * Apply modeling transformations as well.
 */
int
rt_revolve_import5( struct rt_db_internal  *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp, const int minor_type )
{
    struct rt_revolve_internal	*rip;
    fastf_t			vv[ELEMENTS_PER_VECT*3 + 1];

    char			*sketch_name;
    unsigned char		*ptr;
    struct directory		*dp;
    struct rt_db_internal	tmp_ip;

    RT_CK_DB_INTERNAL(ip)
    BU_CK_EXTERNAL( ep );

    /* set up the internal structure */
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_REVOLVE;
    ip->idb_meth = &rt_functab[ID_REVOLVE];
    ip->idb_ptr = bu_malloc( sizeof(struct rt_revolve_internal), "rt_revolve_internal");
    rip = (struct rt_revolve_internal *)ip->idb_ptr;
    rip->magic = RT_REVOLVE_INTERNAL_MAGIC;

    /* Convert the data in ep->ext_buf into internal format.  Note the
     * conversion from network data (Big Endian ints, IEEE double
     * floating point) to host local data representations.
     */

    ptr = (unsigned char *)ep->ext_buf;
    sketch_name = (char *)ptr + (ELEMENTS_PER_VECT*3 + 1)*SIZEOF_NETWORK_DOUBLE;
    if ( !dbip )
	rip->sk = (struct rt_sketch_internal *)NULL;
    else if ( (dp=db_lookup( dbip, sketch_name, LOOKUP_NOISY)) == DIR_NULL )
    {
	bu_log( "rt_revolve_import: ERROR: Cannot find sketch (%s) for extrusion\n",
		sketch_name );
	rip->sk = (struct rt_sketch_internal *)NULL;
    }
    else
    {
	if ( rt_db_get_internal( &tmp_ip, dp, dbip, bn_mat_identity, resp ) != ID_SKETCH )
	{
	    bu_log( "rt_revolve_import: ERROR: Cannot import sketch (%s) for extrusion\n",
		    sketch_name );
	    bu_free( ip->idb_ptr, "extrusion" );
	    return( -1 );
	}
	else
	    rip->sk = (struct rt_sketch_internal *)tmp_ip.idb_ptr;
    }

    ntohd( (unsigned char *)&vv, (unsigned char *)ep->ext_buf, ELEMENTS_PER_VECT*3 + 1 );

    /* Apply the modeling transformation */
    if (mat == NULL) mat = bn_mat_identity;
    MAT4X3PNT( rip->v3d, mat, &vv[0*3] );
    MAT4X3PNT( rip->axis3d, mat, &vv[1*3] );
    MAT4X3PNT( rip->r, mat, &vv[2*3] );
    rip->ang = vv[9];
    rip->sketch_name = bu_strdup( (unsigned char *)ep->ext_buf + (ELEMENTS_PER_VECT*3 + 1)*SIZEOF_NETWORK_DOUBLE );

    return(0);			/* OK */
}

/**
 * R T _ R E V O L V E _ E X P O R T 5
 *
 * Export an REVOLVE from internal form to external format.  Note that
 * this means converting all integers to Big-Endian format and
 * floating point data to IEEE double.
 *
 * Apply the transformation to mm units as well.
 */
int
rt_revolve_export5( struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip )
{
    struct rt_revolve_internal	*rip;
    fastf_t			vec[ELEMENTS_PER_VECT*3 + 1];
    unsigned char		*ptr;

    RT_CK_DB_INTERNAL(ip);
    if ( ip->idb_type != ID_REVOLVE )  return(-1);
    rip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * (ELEMENTS_PER_VECT*3 + 1) + strlen(rip->sketch_name) + 1;
    ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "revolve external");

    ptr = (unsigned char *)ep->ext_buf;

    /* Since libwdb users may want to operate in units other than mm,
     * we offer the opportunity to scale the solid (to get it into mm)
     * on the way out.
     */
    VSCALE( &vec[0*3], rip->v3d, local2mm );
    VSCALE( &vec[1*3], rip->axis3d, local2mm );
    VSCALE( &vec[2*3], rip->r, local2mm );
    vec[9] = rip->ang;

    htond( ptr, (unsigned char *)vec, ELEMENTS_PER_VECT*3 + 1 );
    ptr += (ELEMENTS_PER_VECT*3 + 1) * SIZEOF_NETWORK_DOUBLE;

    bu_strlcpy( (char *)ptr, rip->sketch_name, strlen(rip->sketch_name)+1 );

    return 0;
}

/**
 * R T _ R E V O L V E _ D E S C R I B E
 *
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_revolve_describe( struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local )
{
    register struct rt_revolve_internal	*rip =
	(struct rt_revolve_internal *)ip->idb_ptr;
    char	buf[256];

    RT_REVOLVE_CK_MAGIC(rip);
    bu_vls_strcat( str, "truncated general revolve (REVOLVE)\n");

    sprintf(buf, "\tV (%g, %g, %g)\n",
	    INTCLAMP(rip->v3d[X] * mm2local),
	    INTCLAMP(rip->v3d[Y] * mm2local),
	    INTCLAMP(rip->v3d[Z] * mm2local) );
    bu_vls_strcat( str, buf );

    sprintf(buf, "\tAxis (%g, %g, %g)\n",
	    INTCLAMP(rip->axis3d[X] * mm2local),
	    INTCLAMP(rip->axis3d[Y] * mm2local),
	    INTCLAMP(rip->axis3d[Z] * mm2local) );
    bu_vls_strcat( str, buf );

    sprintf(buf, "\tR (%g, %g, %g)\n",
	    INTCLAMP(rip->r[X] * mm2local),
	    INTCLAMP(rip->r[Y] * mm2local),
	    INTCLAMP(rip->r[Z] * mm2local) );
    bu_vls_strcat( str, buf );

    sprintf(buf, "\tAngle=%g\n", INTCLAMP(rip->ang * mm2local));
    bu_vls_strcat( str, buf );

    snprintf( buf, 256, "\tsketch name: %s\n", rip->sketch_name );
    bu_vls_strcat( str, buf );

    return(0);
}

/**
 * R T _ R E V O L V E _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_revolve_ifree( struct rt_db_internal *ip )
{
    register struct rt_revolve_internal	*revolve_ip;

    RT_CK_DB_INTERNAL(ip);
    revolve_ip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(revolve_ip);
    revolve_ip->magic = 0;			/* sanity */

    bu_free( (char *)revolve_ip, "revolve ifree" );
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

/**
 * R T _ R E V O L V E _ X F O R M
 *
 * Create transformed version of internal form.  Free *ip if
 * requested.  Implement this if it's faster than doing an
 * export/import cycle.
 */
int
rt_revolve_xform( struct rt_db_internal *op, const mat_t mat, struct rt_db_internal *ip, int free )
{
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
