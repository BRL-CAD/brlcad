/*                      G _ S K E T C H . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2007 United States Government as represented by
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
/** @file g_sketch.c
 *
 *	Provide support for 2D sketches.
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/** @} */

#ifndef lint
static const char RCSsketch[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <math.h>
#include <ctype.h>

#include "tcl.h"
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "./debug.h"


fastf_t rt_cnurb_par_edge(const struct edge_g_cnurb *crv, fastf_t epsilon);
extern void get_indices( genptr_t seg, int *start, int *end );	/* from g_extrude.c */

int
rt_check_curve(struct curve *crv, struct rt_sketch_internal *skt, int noisey)
{
    int i, j;
    int ret=0;

    for( i=0 ; i<crv->seg_count ; i++ )
	{
	    struct line_seg *lsg;
	    struct carc_seg *csg;
	    struct nurb_seg *nsg;
	    struct bezier_seg *bsg;
	    long *lng;

	    lng = (long *)crv->segments[i];

	    switch( *lng )
		{
		    case CURVE_LSEG_MAGIC:
			lsg = (struct line_seg *)lng;
			if( lsg->start >= skt->vert_count ||
			    lsg->end >= skt->vert_count )
			    ret++;
			break;
		    case CURVE_CARC_MAGIC:
			csg = (struct carc_seg *)lng;
			if( csg->start >= skt->vert_count ||
			    csg->end >= skt->vert_count )
			    ret++;
			break;
		    case CURVE_NURB_MAGIC:
			nsg = (struct nurb_seg *)lng;
			for( j=0 ; j<nsg->c_size ; j++ )
			    {
				if( nsg->ctl_points[j] >= skt->vert_count )
				    {
					ret++;
					break;
				    }
			    }
			break;
		    case CURVE_BEZIER_MAGIC:
			bsg = (struct bezier_seg *)lng;
			for( j=0 ; j<=bsg->degree ; j++ ) {
			    if( bsg->ctl_points[j] >= skt->vert_count ) {
				ret++;
				break;
			    }
			}
			break;
		    default:
			ret++;
			if( noisey )
			    bu_log( "Unrecognized segment type in sketch\n");
			break;
		}
	}
    if( ret && noisey )
	bu_log( "sketch references non-existent vertices!!!\n" );
    return( ret );
}


/**
 *  			R T _ S K E T C H _ P R E P
 *
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid SKETCH, and if so, precompute various
 *  terms of the formula.
 *
 *  Returns -
 *  	0	SKETCH is OK
 *  	!0	Error in description
 *
 *  Implicit return -
 *  	A struct sketch_specific is created, and it's address is stored in
 *  	stp->st_specific for use by sketch_shot().
 */
int
rt_sketch_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    stp->st_specific = (genptr_t)NULL;
    return( 0 );
}


/**
 *			R T _ S K E T C H _ P R I N T
 */
void
rt_sketch_print(register const struct soltab *stp)
{
}

/**
 *  			R T _ S K E T C H _ S H O T
 *
 *  Intersect a ray with a sketch.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_sketch_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    return(0);			/* MISS */
}

#define RT_SKETCH_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL


/**
 *			R T _ S K E T C H _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_sketch_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
     /* An array of solid pointers */
     /* An array of ray pointers */
     /* array of segs (results returned) */
     /* Number of ray/object pairs */

{
    rt_vstub( stp, rp, segp, n, ap );
}


/**
 *  			R T _ S K E T C H _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_sketch_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{

    VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
}


/**
 *			R T _ S K E T C H _ C U R V E
 *
 *  Return the curvature of the sketch.
 */
void
rt_sketch_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}


/**
 *  			R T _ S K E T C H _ U V
 *
 *  For a hit on the surface of an sketch, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_sketch_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
}


/**
 *		R T _ S K E T C H _ F R E E
 */
void
rt_sketch_free(register struct soltab *stp)
{
}


/**
 *			R T _ S K E T C H _ C L A S S
 */
int
rt_sketch_class(void)
{
    return(0);
}

int
seg_to_vlist(struct bu_list *vhead, const struct rt_tess_tol *ttol, fastf_t *V, fastf_t *u_vec, fastf_t *v_vec, struct rt_sketch_internal *sketch_ip, genptr_t seg)
{
    int		ret=0;
    int		i;
    long		*lng;
    struct line_seg *lsg;
    struct carc_seg *csg;
    struct nurb_seg *nsg;
    struct bezier_seg *bsg;
    fastf_t delta;
    point_t center, start_pt;
    fastf_t pt[4];
    vect_t semi_a, semi_b;
    fastf_t radius;
    vect_t norm;

    lng = (long *)seg;
    switch( *lng )
	{
	    case CURVE_LSEG_MAGIC:
		lsg = (struct line_seg *)lng;
		if( lsg->start >= sketch_ip->vert_count ||
		    lsg->end >= sketch_ip->vert_count )
		    {
			ret++;
			break;
		    }
		VJOIN2( pt, V, sketch_ip->verts[lsg->start][0], u_vec, sketch_ip->verts[lsg->start][1], v_vec);
		RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE )
		    VJOIN2( pt, V, sketch_ip->verts[lsg->end][0], u_vec, sketch_ip->verts[lsg->end][1], v_vec);
		RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
		break;
	    case CURVE_CARC_MAGIC:
		{
		    point2d_t mid_pt, start2d, end2d, center2d, s2m, dir;
		    fastf_t s2m_len_sq, len_sq, tmp_len, cross_z;
		    fastf_t start_ang, end_ang, tot_ang, cosdel, sindel;
		    fastf_t oldu, oldv, newu, newv;
		    int nsegs;

		    csg = (struct carc_seg *)lng;
		    if( csg->start >= sketch_ip->vert_count ||
			csg->end >= sketch_ip->vert_count )
			{
			    ret++;
			    break;
			}

		    delta = M_PI/4.0;
		    if( csg->radius <= 0.0 )
			{
			    VJOIN2( center, V, sketch_ip->verts[csg->end][0], u_vec, sketch_ip->verts[csg->end][1], v_vec);
			    VJOIN2( pt, V, sketch_ip->verts[csg->start][0], u_vec, sketch_ip->verts[csg->start][1], v_vec);
			    VSUB2( semi_a, pt, center );
			    VCROSS( norm, u_vec, v_vec );
			    VCROSS( semi_b, norm, semi_a );
			    VUNITIZE( semi_b );
			    radius = MAGNITUDE( semi_a );
			    VSCALE( semi_b, semi_b, radius );
			}
		    else if( csg->radius < SMALL_FASTF )
			{
			    bu_log( "Radius too small in sketch!\n" );
			    break;
			}
		    else
			radius = csg->radius;

		    if( ttol->abs > 0.0 )
			{
			    fastf_t tmp_delta, ratio;

			    ratio = ttol->abs / radius;
			    if( ratio < 1.0 )
				{
				    tmp_delta = 2.0 * acos( 1.0 - ratio);
				    if( tmp_delta < delta )
					delta = tmp_delta;
				}
			}
		    if( ttol->rel > 0.0 && ttol->rel < 1.0 )
			{
			    fastf_t tmp_delta;

			    tmp_delta = 2.0 * acos( 1.0 - ttol->rel );
			    if( tmp_delta < delta )
				delta = tmp_delta;
			}
		    if( ttol->norm > 0.0 )
			{
			    fastf_t norm;

			    norm = ttol->norm * M_PI / 180.0;
			    if( norm < delta )
				delta = norm;
			}
		    if( csg->radius <= 0.0 )
			{
			    /* this is a full circle */
			    nsegs = ceil( 2.0 * M_PI / delta );
			    delta = 2.0 * M_PI / (double)nsegs;
			    cosdel = cos( delta );
			    sindel = sin( delta );
			    oldu = 1.0;
			    oldv = 0.0;
			    VJOIN2( start_pt, center, oldu, semi_a, oldv, semi_b );
			    RT_ADD_VLIST( vhead, start_pt, BN_VLIST_LINE_MOVE );
			    for( i=1 ; i<nsegs ; i++ )
				{
				    newu = oldu * cosdel - oldv * sindel;
				    newv = oldu * sindel + oldv * cosdel;
				    VJOIN2( pt, center, newu, semi_a, newv, semi_b );
				    RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
				    oldu = newu;
				    oldv = newv;
				}
			    RT_ADD_VLIST( vhead, start_pt, BN_VLIST_LINE_DRAW );
			    break;
			}

		    /* this is an arc (not a full circle) */
		    V2MOVE( start2d, sketch_ip->verts[csg->start] );
		    V2MOVE( end2d, sketch_ip->verts[csg->end] );
		    mid_pt[0] = (start2d[0] + end2d[0]) * 0.5;
		    mid_pt[1] = (start2d[1] + end2d[1]) * 0.5;
		    V2SUB2( s2m, mid_pt, start2d )
			dir[0] = -s2m[1];
		    dir[1] = s2m[0];
		    s2m_len_sq =  s2m[0]*s2m[0] + s2m[1]*s2m[1];
		    if( s2m_len_sq < SMALL_FASTF )
			{
			    bu_log( "start and end points are too close together in circular arc of sketch\n" );
			    break;
			}
		    len_sq = radius*radius - s2m_len_sq;
		    if( len_sq < 0.0 )
			{
			    bu_log( "Impossible radius for specified start and end points in circular arc\n");
			    break;
			}
		    tmp_len = sqrt( dir[0]*dir[0] + dir[1]*dir[1] );
		    dir[0] = dir[0] / tmp_len;
		    dir[1] = dir[1] / tmp_len;
		    tmp_len = sqrt( len_sq );
		    V2JOIN1( center2d, mid_pt, tmp_len, dir )

			/* check center location */
			cross_z = ( end2d[X] - start2d[X] )*( center2d[Y] - start2d[Y] ) -
			( end2d[Y] - start2d[Y] )*( center2d[X] - start2d[X] );
		    if( !(cross_z > 0.0 && csg->center_is_left) )
			V2JOIN1( center2d, mid_pt, -tmp_len, dir );
		    start_ang = atan2( start2d[Y]-center2d[Y], start2d[X]-center2d[X] );
		    end_ang = atan2( end2d[Y]-center2d[Y], end2d[X]-center2d[X] );
		    if( csg->orientation ) /* clock-wise */
			{
			    while( end_ang > start_ang )
				end_ang -= 2.0 * M_PI;
			}
		    else /* counter-clock-wise */
			{
			    while( end_ang < start_ang )
				end_ang += 2.0 * M_PI;
			}
		    tot_ang = end_ang - start_ang;
		    nsegs = ceil( tot_ang / delta );
		    if( nsegs < 0 )
			nsegs = -nsegs;
		    if( nsegs < 3 )
			nsegs = 3;
		    delta = tot_ang / nsegs;
		    cosdel = cos( delta );
		    sindel = sin( delta );
		    VJOIN2( center, V, center2d[0], u_vec, center2d[1], v_vec );
		    VJOIN2( start_pt, V, start2d[0], u_vec, start2d[1], v_vec );
		    oldu = (start2d[0] - center2d[0]);
		    oldv = (start2d[1] - center2d[1]);
		    RT_ADD_VLIST( vhead, start_pt, BN_VLIST_LINE_MOVE );
		    for( i=0 ; i<nsegs ; i++ )
			{
			    newu = oldu * cosdel - oldv * sindel;
			    newv = oldu * sindel + oldv * cosdel;
			    VJOIN2( pt, center, newu, u_vec, newv, v_vec );
			    RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
			    oldu = newu;
			    oldv = newv;
			}
		    break;
		}
	    case CURVE_NURB_MAGIC:
		{
		    struct edge_g_cnurb eg;
		    int coords;
		    fastf_t inv_weight;
		    int num_intervals;
		    fastf_t param_delta, epsilon;

		    nsg = (struct nurb_seg *)lng;
		    for( i=0 ; i<nsg->c_size ; i++ )
			{
			    if( nsg->ctl_points[i] >= sketch_ip->vert_count )
				{
				    ret++;
				    break;
				}
			}
		    if( nsg->order < 3 )
			{
			    /* just straight lines */
			    VJOIN2( start_pt, V, sketch_ip->verts[nsg->ctl_points[0]][0], u_vec, sketch_ip->verts[nsg->ctl_points[0]][1], v_vec );
			    if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) )
				{
				    inv_weight = 1.0/nsg->weights[0];
				    VSCALE( start_pt, start_pt, inv_weight );
				}
			    RT_ADD_VLIST( vhead, start_pt, BN_VLIST_LINE_MOVE );
			    for( i=1 ; i<nsg->c_size ; i++ )
				{
				    VJOIN2( pt, V, sketch_ip->verts[nsg->ctl_points[i]][0], u_vec, sketch_ip->verts[nsg->ctl_points[i]][1], v_vec );
				    if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) )
					{
					    inv_weight = 1.0/nsg->weights[i];
					    VSCALE( pt, pt, inv_weight );
					}
				    RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
				}
			    break;
			}
		    eg.l.magic = NMG_EDGE_G_CNURB_MAGIC;
		    eg.order = nsg->order;
		    eg.k.k_size = nsg->k.k_size;
		    eg.k.knots = nsg->k.knots;
		    eg.c_size = nsg->c_size;
		    coords = 3 + RT_NURB_IS_PT_RATIONAL( nsg->pt_type );
		    eg.pt_type = RT_NURB_MAKE_PT_TYPE( coords, 2, RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) );
		    eg.ctl_points = (fastf_t *)bu_malloc( nsg->c_size * coords * sizeof( fastf_t ), "eg.ctl_points" );
		    if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) )
			{
			    for( i=0 ; i<nsg->c_size ; i++ )
				{
				    VJOIN2( &eg.ctl_points[i*coords], V, sketch_ip->verts[nsg->ctl_points[i]][0], u_vec, sketch_ip->verts[nsg->ctl_points[i]][1], v_vec );
				    eg.ctl_points[(i+1)*coords - 1] = nsg->weights[i];
				}
			}
		    else
			{
			    for( i=0 ; i<nsg->c_size ; i++ )
				VJOIN2( &eg.ctl_points[i*coords], V, sketch_ip->verts[nsg->ctl_points[i]][0], u_vec, sketch_ip->verts[nsg->ctl_points[i]][1], v_vec );
			}
		    epsilon = MAX_FASTF;
		    if( ttol->abs > 0.0 && ttol->abs < epsilon )
			epsilon = ttol->abs;
		    if( ttol->rel > 0.0 )
			{
			    point2d_t min_pt, max_pt, tmp_pt;
			    point2d_t diff;
			    fastf_t tmp_epsilon;

			    min_pt[0] = MAX_FASTF;
			    min_pt[1] = MAX_FASTF;
			    max_pt[0] = -MAX_FASTF;
			    max_pt[1] = -MAX_FASTF;

			    for( i=0 ; i<nsg->c_size ; i++ )
				{
				    V2MOVE( tmp_pt, sketch_ip->verts[nsg->ctl_points[i]] );
				    if( tmp_pt[0] > max_pt[0] )
					max_pt[0] = tmp_pt[0];
				    if( tmp_pt[1] > max_pt[1] )
					max_pt[1] = tmp_pt[1];
				    if( tmp_pt[0] < min_pt[0] )
					min_pt[0] = tmp_pt[0];
				    if( tmp_pt[1] < min_pt[1] )
					min_pt[1] = tmp_pt[1];
				}

			    V2SUB2( diff, max_pt, min_pt )
				tmp_epsilon = ttol->rel * sqrt( MAG2SQ( diff ) );
			    if( tmp_epsilon < epsilon )
				epsilon = tmp_epsilon;

			}
		    param_delta = rt_cnurb_par_edge( &eg, epsilon );
		    num_intervals = ceil( (nsg->k.knots[nsg->k.k_size-1] - nsg->k.knots[0])/param_delta );
		    if( num_intervals < 3 )
			num_intervals = 3;
		    if( num_intervals > 500 )
			{
			    bu_log( "num_intervals was %d, clamped to 500\n", num_intervals );
			    num_intervals = 500;
			}
		    param_delta = (nsg->k.knots[nsg->k.k_size-1] - nsg->k.knots[0])/(double)num_intervals;
		    for( i=0 ; i<=num_intervals ; i++ )
			{
			    fastf_t t;
			    int j;

			    t = nsg->k.knots[0] + i*param_delta;
			    rt_nurb_c_eval( &eg, t, pt );
			    if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) )
				{
				    for( j=0 ; j<coords-1 ; j++ )
					pt[j] /= pt[coords-1];
				}
			    if( i == 0 )
				RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE )
				    else
					RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
			}
		    bu_free( (char *)eg.ctl_points, "eg.ctl_points" );
		    break;
		}
	    case CURVE_BEZIER_MAGIC: {
		struct bezier_2d_list *bezier_hd, *bz;
		fastf_t epsilon;

		bsg = (struct bezier_seg *)lng;

		for( i=0 ; i<=bsg->degree ; i++ ) {
		    if( bsg->ctl_points[i] >= sketch_ip->vert_count ) {
			ret++;
			break;
		    }
		}

		if( bsg->degree < 1 ) {
		    bu_log( "g_sketch: ERROR: Bezier curve with illegal degree (%d)\n",
			    bsg->degree );
		    ret++;
		    break;
		}

		if( bsg->degree == 1 ) {
		    /* straight line */
		    VJOIN2( start_pt, V, sketch_ip->verts[bsg->ctl_points[0]][0],
			    u_vec, sketch_ip->verts[bsg->ctl_points[0]][1], v_vec );
		    RT_ADD_VLIST( vhead, start_pt, BN_VLIST_LINE_MOVE );
		    for( i=1 ; i<=bsg->degree ; i++ ) {
			VJOIN2( pt, V, sketch_ip->verts[bsg->ctl_points[i]][0],
				u_vec, sketch_ip->verts[bsg->ctl_points[i]][1], v_vec );
			RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
		    }
		    break;
		}

		/* use tolerance to determine coarseness of plot */
		epsilon = MAX_FASTF;
		if( ttol->abs > 0.0 && ttol->abs < epsilon )
		    epsilon = ttol->abs;
		if( ttol->rel > 0.0 )
		    {
			point2d_t min_pt, max_pt, tmp_pt;
			point2d_t diff;
			fastf_t tmp_epsilon;

			min_pt[0] = MAX_FASTF;
			min_pt[1] = MAX_FASTF;
			max_pt[0] = -MAX_FASTF;
			max_pt[1] = -MAX_FASTF;

			for( i=0 ; i<=bsg->degree ; i++ )
			    {
				V2MOVE( tmp_pt, sketch_ip->verts[bsg->ctl_points[i]] );
				if( tmp_pt[0] > max_pt[0] )
				    max_pt[0] = tmp_pt[0];
				if( tmp_pt[1] > max_pt[1] )
				    max_pt[1] = tmp_pt[1];
				if( tmp_pt[0] < min_pt[0] )
				    min_pt[0] = tmp_pt[0];
				if( tmp_pt[1] < min_pt[1] )
				    min_pt[1] = tmp_pt[1];
			    }

			V2SUB2( diff, max_pt, min_pt )
			    tmp_epsilon = ttol->rel * sqrt( MAG2SQ( diff ) );
			if( tmp_epsilon < epsilon )
			    epsilon = tmp_epsilon;

		    }


		/* Create an initial bezier_2d_list */
		bezier_hd = (struct bezier_2d_list *)bu_malloc( sizeof( struct bezier_2d_list ),
								"g_sketch.c: bezier_hd" );
		BU_LIST_INIT( &bezier_hd->l );
		bezier_hd->ctl = (point2d_t *)bu_calloc( bsg->degree + 1, sizeof( point2d_t ),
							 "g_sketch.c: bezier_hd->ctl" );
		for( i=0 ; i<=bsg->degree ; i++ ) {
		    V2MOVE( bezier_hd->ctl[i], sketch_ip->verts[bsg->ctl_points[i]] );
		}

		/* now do subdivision as necessary */
		bezier_hd = subdivide_bezier( bezier_hd, bsg->degree, epsilon, 0 );

		/* plot the results */
		bz = BU_LIST_FIRST( bezier_2d_list, &bezier_hd->l );
		VJOIN2( pt, V, bz->ctl[0][0], u_vec, bz->ctl[0][1], v_vec);
		RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE );

		while( BU_LIST_WHILE( bz, bezier_2d_list, &(bezier_hd->l) ) ) {
		    BU_LIST_DEQUEUE( &bz->l );
		    for( i=1 ; i<=bsg->degree ; i++ ) {
			VJOIN2( pt, V, bz->ctl[i][0], u_vec,
				bz->ctl[i][1], v_vec);
			RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
		    }
		    bu_free( (char *)bz->ctl, "g_sketch.c: bz->ctl" );
		    bu_free( (char *)bz, "g_sketch.c: bz" );
		}
		bu_free( (char *)bezier_hd, "g_sketch.c: bezier_hd" );
		break;
	    }
	    default:
		bu_log( "seg_to_vlist: ERROR: unrecognized segment type!!!!\n" );
		break;
	}

    return( ret );
}


/**
 *			C U R V E _ T O _ V L I S T
 */
int
curve_to_vlist(struct bu_list *vhead, const struct rt_tess_tol *ttol, fastf_t *V, fastf_t *u_vec, fastf_t *v_vec, struct rt_sketch_internal *sketch_ip, struct curve *crv)
{
    int seg_no;
    int ret=0;

    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at start of curve_to_vlist():\n" );
	    bu_mem_barriercheck();
	}

    for( seg_no=0 ; seg_no < crv->seg_count ; seg_no++ )
	{
	    ret += seg_to_vlist( vhead, ttol, V, u_vec, v_vec, sketch_ip, crv->segments[seg_no] );
	}

    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at end of curve_to_vlist():\n" );
	    bu_mem_barriercheck();
	}

    return( ret );
}


/**
 *			R T _ S K E T C H _ P L O T
 */
int
rt_sketch_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    LOCAL struct rt_sketch_internal	*sketch_ip;
    int				ret;
    int				myret=0;

    RT_CK_DB_INTERNAL(ip);
    sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
    RT_SKETCH_CK_MAGIC(sketch_ip);

    if( (ret=curve_to_vlist( vhead, ttol, sketch_ip->V, sketch_ip->u_vec, sketch_ip->v_vec, sketch_ip, &sketch_ip->skt_curve )) )
	{
	    myret--;
	    bu_log( "WARNING: Errors in sketch (%d segments reference non-existent vertices)\n",
		    ret );
	}

    return( myret );
}


/**
 *			R T _ S K E T C H _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_sketch_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    return(-1);
}


/**
 *			R T _ S K E T C H _ I M P O R T
 *
 *  Import an SKETCH from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_sketch_import(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
    LOCAL struct rt_sketch_internal	*sketch_ip;
    union record			*rp;
    vect_t				v;
    int				seg_no;
    unsigned char			*ptr;
    struct curve			*crv;
    int				i;

    BU_CK_EXTERNAL( ep );
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if( rp->u_id != DBID_SKETCH )  {
	bu_log("rt_sketch_import: defective record\n");
	return(-1);
    }

    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at start of sketch_import():\n" );
	    bu_mem_barriercheck();
	}

    RT_CK_DB_INTERNAL( ip );
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_SKETCH;
    ip->idb_meth = &rt_functab[ID_SKETCH];
    ip->idb_ptr = bu_calloc( 1, sizeof(struct rt_sketch_internal), "rt_sketch_internal");
    sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
    sketch_ip->magic = RT_SKETCH_INTERNAL_MAGIC;

    ntohd( (unsigned char *)v, rp->skt.skt_V, 3 );
    MAT4X3PNT( sketch_ip->V, mat, v );
    ntohd( (unsigned char *)v, rp->skt.skt_uvec, 3 );
    MAT4X3VEC( sketch_ip->u_vec, mat, v );
    ntohd( (unsigned char *)v, rp->skt.skt_vvec, 3 );
    MAT4X3VEC( sketch_ip->v_vec, mat, v );
    sketch_ip->vert_count = bu_glong( rp->skt.skt_vert_count );
    sketch_ip->skt_curve.seg_count = bu_glong( rp->skt.skt_seg_count );

    ptr = (unsigned char *)rp;
    ptr += sizeof( struct sketch_rec );
    if( sketch_ip->vert_count )
	sketch_ip->verts = (point2d_t *)bu_calloc( sketch_ip->vert_count, sizeof( point2d_t ), "sketch_ip->vert" );
    ntohd( (unsigned char *)sketch_ip->verts, ptr, sketch_ip->vert_count*2 );
    ptr += 16 * sketch_ip->vert_count;

    if( sketch_ip->skt_curve.seg_count )
	sketch_ip->skt_curve.segments = (genptr_t *)bu_calloc( sketch_ip->skt_curve.seg_count, sizeof( genptr_t ), "segs" );
    else
	sketch_ip->skt_curve.segments = (genptr_t *)NULL;
    for( seg_no=0 ; seg_no < sketch_ip->skt_curve.seg_count ; seg_no++ )
	{
	    long magic;
	    struct line_seg *lsg;
	    struct carc_seg *csg;
	    struct nurb_seg *nsg;
	    struct bezier_seg *bsg;
	    int i;

	    magic = bu_glong( ptr );
	    ptr += 4;
	    switch( magic )
		{
		    case CURVE_LSEG_MAGIC:
			lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "lsg" );
			lsg->magic = magic;
			lsg->start = bu_glong( ptr );
			ptr += 4;
			lsg->end = bu_glong( ptr );
			ptr += 4;
			sketch_ip->skt_curve.segments[seg_no] = (genptr_t)lsg;
			break;
		    case CURVE_CARC_MAGIC:
			csg = (struct carc_seg *)bu_malloc( sizeof( struct carc_seg ), "csg" );
			csg->magic = magic;
			csg->start = bu_glong( ptr );
			ptr += 4;
			csg->end = bu_glong( ptr );
			ptr += 4;
			csg->orientation = bu_glong( ptr );
			ptr += 4;
			csg->center_is_left = bu_glong( ptr );
			ptr += 4;
			ntohd( (unsigned char *)&csg->radius, ptr, 1 );
			ptr += 8;
			sketch_ip->skt_curve.segments[seg_no] = (genptr_t)csg;
			break;
		    case CURVE_NURB_MAGIC:
			nsg = (struct nurb_seg *)bu_malloc( sizeof( struct nurb_seg ), "nsg" );
			nsg->magic = magic;
			nsg->order = bu_glong( ptr );
			ptr += 4;
			nsg->pt_type = bu_glong( ptr );
			ptr += 4;
			nsg->k.k_size = bu_glong( ptr );
			ptr += 4;
			nsg->k.knots = (fastf_t *)bu_malloc( nsg->k.k_size * sizeof( fastf_t ), "nsg->k.knots" );
			ntohd( (unsigned char *)nsg->k.knots, ptr, nsg->k.k_size );
			ptr += 8 * nsg->k.k_size;
			nsg->c_size = bu_glong( ptr );
			ptr += 4;
			nsg->ctl_points = (int *)bu_malloc( nsg->c_size * sizeof( int ), "nsg->ctl_points" );
			for( i=0 ; i<nsg->c_size ; i++ )
			    {
				nsg->ctl_points[i] = bu_glong( ptr );
				ptr += 4;
			    }
			if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) )
			    {
				nsg->weights = (fastf_t *)bu_malloc( nsg->c_size * sizeof( fastf_t ), "nsg->weights" );
				ntohd( (unsigned char *)nsg->weights, ptr, nsg->c_size );
				ptr += 8 * nsg->c_size;
			    }
			else
			    nsg->weights = (fastf_t *)NULL;
			sketch_ip->skt_curve.segments[seg_no] = (genptr_t)nsg;
			break;
		    case CURVE_BEZIER_MAGIC:
			bsg = (struct bezier_seg *)bu_malloc( sizeof( struct bezier_seg ), "bsg" );
			bsg->magic = magic;
			bsg->degree = bu_glong( ptr );
			ptr += 4;
			bsg->ctl_points = (int *)bu_calloc( bsg->degree + 1, sizeof( int ), "bsg->ctl_points" );
			for( i=0 ; i<=bsg->degree ; i++ ) {
			    bsg->ctl_points[i] = bu_glong( ptr );
			    ptr += 4;
			}
			sketch_ip->skt_curve.segments[seg_no] = (genptr_t)bsg;
			break;
		    default:
			bu_bomb( "rt_sketch_import: ERROR: unrecognized segment type!!!\n" );
			break;
		}
	}

    crv = &sketch_ip->skt_curve;

    crv->reverse = (int *)bu_calloc( crv->seg_count, sizeof(int), "crv->reverse" );
    for( i=0 ; i<crv->seg_count ; i++ )
	{
	    crv->reverse[i] = bu_glong( ptr );
	    ptr += 4;
	}

    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at end of sketch_import():\n" );
	    bu_mem_barriercheck();
	}

    return(0);			/* OK */
}


/**
 *			R T _ S K E T C H _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_sketch_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_sketch_internal	*sketch_ip;
    union record		*rec;
    int	i, seg_no, nbytes=0, ngran;
    vect_t tmp_vec;
    unsigned char *ptr;

    RT_CK_DB_INTERNAL(ip);
    if( ip->idb_type != ID_SKETCH )  return(-1);
    sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
    RT_SKETCH_CK_MAGIC(sketch_ip);

    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at start of sketch_export():\n" );
	    bu_mem_barriercheck();
	}

    BU_CK_EXTERNAL(ep);

    nbytes = sizeof(union record);		/* base record */
    nbytes += sketch_ip->vert_count*(8*2);	/* vertex list */

    for( seg_no=0 ; seg_no < sketch_ip->skt_curve.seg_count ; seg_no++ )
	{
	    long *lng;
	    struct nurb_seg *nseg;
	    struct bezier_seg *bseg;

	    lng = (long *)sketch_ip->skt_curve.segments[seg_no];
	    switch( *lng )
		{
		    case CURVE_LSEG_MAGIC:
			nbytes += 12;
			break;
		    case CURVE_CARC_MAGIC:
			nbytes += 28;
			break;
		    case CURVE_NURB_MAGIC:
			nseg = (struct nurb_seg *)lng;
			nbytes += 16 + sizeof( struct knot_vector) + nseg->k.k_size * 8 + nseg->c_size * 4;
			if( RT_NURB_IS_PT_RATIONAL( nseg->pt_type ) )
			    nbytes += nseg->c_size * 8;	/* weights */
			break;
		    case CURVE_BEZIER_MAGIC:
			bseg = (struct bezier_seg *)lng;
			nbytes += 8 + (bseg->degree + 1) * 4;
			break;
		    default:
			bu_log( "rt_sketch_export: unsupported segement type (x%x)\n", *lng );
			bu_bomb( "rt_sketch_export: unsupported segement type\n" );
		}
	}

    ngran = ceil((double)(nbytes + sizeof(union record)) / sizeof(union record));
    ep->ext_nbytes = ngran * sizeof(union record);
    ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "sketch external");

    rec = (union record *)ep->ext_buf;

    rec->skt.skt_id = DBID_SKETCH;

    /* convert from local editing units to mm and export
     * to database record format
     */
    VSCALE( tmp_vec, sketch_ip->V, local2mm );
    htond( rec->skt.skt_V, (unsigned char *)tmp_vec, 3 );

    /* uvec and uvec are unit vectors, do not convert to/from mm */
    htond( rec->skt.skt_uvec, (unsigned char *)sketch_ip->u_vec, 3 );
    htond( rec->skt.skt_vvec, (unsigned char *)sketch_ip->v_vec, 3 );

    (void)bu_plong( rec->skt.skt_vert_count, sketch_ip->vert_count );
    (void)bu_plong( rec->skt.skt_seg_count, sketch_ip->skt_curve.seg_count );
    (void)bu_plong( rec->skt.skt_count, ngran-1 );

    ptr = (unsigned char *)rec;
    ptr += sizeof( struct sketch_rec );
    /* convert 2D points to mm */
    for( i=0 ; i<sketch_ip->vert_count ; i++ )
	{
	    point2d_t pt2d;

	    V2SCALE( pt2d, sketch_ip->verts[i], local2mm )
		htond( ptr, (const unsigned char *)pt2d, 2 );
	    ptr += 16;
	}

    for( seg_no=0 ; seg_no < sketch_ip->skt_curve.seg_count ; seg_no++ )
	{
	    struct line_seg *lseg;
	    struct carc_seg *cseg;
	    struct nurb_seg *nseg;
	    struct bezier_seg *bseg;
	    long *lng;
	    fastf_t tmp_fastf;

	    /* write segment type ID, and segement parameters */
	    lng = (long *)sketch_ip->skt_curve.segments[seg_no];
	    switch (*lng )
		{
		    case CURVE_LSEG_MAGIC:
			lseg = (struct line_seg *)lng;
			(void)bu_plong( ptr, CURVE_LSEG_MAGIC );
			ptr += 4;
			(void)bu_plong( ptr, lseg->start );
			ptr += 4;
			(void)bu_plong( ptr, lseg->end );
			ptr += 4;
			break;
		    case CURVE_CARC_MAGIC:
			cseg = (struct carc_seg *)lng;
			(void)bu_plong( ptr, CURVE_CARC_MAGIC );
			ptr += 4;
			(void)bu_plong( ptr, cseg->start );
			ptr += 4;
			(void)bu_plong( ptr, cseg->end );
			ptr += 4;
			(void) bu_plong( ptr, cseg->orientation );
			ptr += 4;
			(void) bu_plong( ptr, cseg->center_is_left );
			ptr += 4;
			tmp_fastf = cseg->radius * local2mm;
			htond( ptr, (unsigned char *)&tmp_fastf, 1 );
			ptr += 8;
			break;
		    case CURVE_NURB_MAGIC:
			nseg = (struct nurb_seg *)lng;
			(void)bu_plong( ptr, CURVE_NURB_MAGIC );
			ptr += 4;
			(void)bu_plong( ptr, nseg->order );
			ptr += 4;
			(void)bu_plong( ptr, nseg->pt_type );
			ptr += 4;
			(void)bu_plong( ptr, nseg->k.k_size );
			ptr += 4;
			htond( ptr, (const unsigned char *)nseg->k.knots, nseg->k.k_size );
			ptr += nseg->k.k_size * 8;
			(void)bu_plong( ptr, nseg->c_size );
			ptr += 4;
			for( i=0 ; i<nseg->c_size ; i++ )
			    {
				(void)bu_plong( ptr, nseg->ctl_points[i] );
				ptr +=  4;
			    }
			if( RT_NURB_IS_PT_RATIONAL( nseg->pt_type ) )
			    {
				htond( ptr, (const unsigned char *)nseg->weights, nseg->c_size );
				ptr += 8 * nseg->c_size;
			    }
			break;
		    case CURVE_BEZIER_MAGIC:
			bseg = (struct bezier_seg *)lng;
			(void)bu_plong( ptr, CURVE_BEZIER_MAGIC );
			ptr += 4;
			(void)bu_plong( ptr, bseg->degree );
			ptr += 4;
			for( i=0 ; i<=bseg->degree ; i++ ) {
			    (void)bu_plong( ptr, bseg->ctl_points[i] );
			    ptr += 4;
			}
			break;
		    default:
			bu_bomb( "rt_sketch_export: ERROR: unrecognized curve type!!!!\n" );
			break;

		}
	}

    for( seg_no=0 ; seg_no < sketch_ip->skt_curve.seg_count ; seg_no++ )
	{
	    (void)bu_plong( ptr, sketch_ip->skt_curve.reverse[seg_no] );
	    ptr += 4;
	}
    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at end of sketch_export():\n" );
	    bu_mem_barriercheck();
	}

    return(0);
}


/**
 *			R T _ S K E T C H _ I M P O R T 5
 *
 *  Import an SKETCH from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_sketch_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
    LOCAL struct rt_sketch_internal	*sketch_ip;
    vect_t				v;
    int				seg_no;
    unsigned char			*ptr;
    struct curve			*crv;
    int				i;

    BU_CK_EXTERNAL( ep );

    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at start of sketch_import5():\n" );
	    bu_mem_barriercheck();
	}

    RT_CK_DB_INTERNAL( ip );
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_SKETCH;
    ip->idb_meth = &rt_functab[ID_SKETCH];
    ip->idb_ptr = bu_calloc( 1, sizeof(struct rt_sketch_internal), "rt_sketch_internal");
    sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
    sketch_ip->magic = RT_SKETCH_INTERNAL_MAGIC;

    ptr = ep->ext_buf;
    ntohd( (unsigned char *)v, ptr, 3 );
    MAT4X3PNT( sketch_ip->V, mat, v );
    ptr += SIZEOF_NETWORK_DOUBLE * 3;
    ntohd( (unsigned char *)v, ptr, 3 );
    MAT4X3VEC( sketch_ip->u_vec, mat, v );
    ptr += SIZEOF_NETWORK_DOUBLE * 3;
    ntohd( (unsigned char *)v, ptr, 3 );
    MAT4X3VEC( sketch_ip->v_vec, mat, v );
    ptr += SIZEOF_NETWORK_DOUBLE * 3;
    sketch_ip->vert_count = bu_glong( ptr );
    ptr += SIZEOF_NETWORK_LONG;
    sketch_ip->skt_curve.seg_count = bu_glong( ptr );
    ptr += SIZEOF_NETWORK_LONG;

    if( sketch_ip->vert_count )
	sketch_ip->verts = (point2d_t *)bu_calloc( sketch_ip->vert_count, sizeof( point2d_t ), "sketch_ip->vert" );
    ntohd( (unsigned char *)sketch_ip->verts, ptr, sketch_ip->vert_count*2 );
    ptr += SIZEOF_NETWORK_DOUBLE * 2 * sketch_ip->vert_count;

    if( sketch_ip->skt_curve.seg_count )
	sketch_ip->skt_curve.segments = (genptr_t *)bu_calloc( sketch_ip->skt_curve.seg_count, sizeof( genptr_t ), "segs" );
    else
	sketch_ip->skt_curve.segments = (genptr_t *)NULL;
    for( seg_no=0 ; seg_no < sketch_ip->skt_curve.seg_count ; seg_no++ )
	{
	    long magic;
	    struct line_seg *lsg;
	    struct carc_seg *csg;
	    struct nurb_seg *nsg;
	    struct bezier_seg *bsg;
	    int i;

	    magic = bu_glong( ptr );
	    ptr += SIZEOF_NETWORK_LONG;
	    switch( magic )
		{
		    case CURVE_LSEG_MAGIC:
			lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "lsg" );
			lsg->magic = magic;
			lsg->start = bu_glong( ptr );
			ptr += SIZEOF_NETWORK_LONG;
			lsg->end = bu_glong( ptr );
			ptr += SIZEOF_NETWORK_LONG;
			sketch_ip->skt_curve.segments[seg_no] = (genptr_t)lsg;
			break;
		    case CURVE_CARC_MAGIC:
			csg = (struct carc_seg *)bu_malloc( sizeof( struct carc_seg ), "csg" );
			csg->magic = magic;
			csg->start = bu_glong( ptr );
			ptr += SIZEOF_NETWORK_LONG;
			csg->end = bu_glong( ptr );
			ptr += SIZEOF_NETWORK_LONG;
			csg->orientation = bu_glong( ptr );
			ptr += SIZEOF_NETWORK_LONG;
			csg->center_is_left = bu_glong( ptr );
			ptr += SIZEOF_NETWORK_LONG;
			ntohd( (unsigned char *)&csg->radius, ptr, 1 );
			ptr += SIZEOF_NETWORK_DOUBLE;
			sketch_ip->skt_curve.segments[seg_no] = (genptr_t)csg;
			break;
		    case CURVE_NURB_MAGIC:
			nsg = (struct nurb_seg *)bu_malloc( sizeof( struct nurb_seg ), "nsg" );
			nsg->magic = magic;
			nsg->order = bu_glong( ptr );
			ptr += SIZEOF_NETWORK_LONG;
			nsg->pt_type = bu_glong( ptr );
			ptr += SIZEOF_NETWORK_LONG;
			nsg->k.k_size = bu_glong( ptr );
			ptr += SIZEOF_NETWORK_LONG;
			nsg->k.knots = (fastf_t *)bu_malloc( nsg->k.k_size * sizeof( fastf_t ), "nsg->k.knots" );
			ntohd( (unsigned char *)nsg->k.knots, ptr, nsg->k.k_size );
			ptr += SIZEOF_NETWORK_DOUBLE * nsg->k.k_size;
			nsg->c_size = bu_glong( ptr );
			ptr += SIZEOF_NETWORK_LONG;
			nsg->ctl_points = (int *)bu_malloc( nsg->c_size * sizeof( int ), "nsg->ctl_points" );
			for( i=0 ; i<nsg->c_size ; i++ )
			    {
				nsg->ctl_points[i] = bu_glong( ptr );
				ptr += SIZEOF_NETWORK_LONG;
			    }
			if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) )
			    {
				nsg->weights = (fastf_t *)bu_malloc( nsg->c_size * sizeof( fastf_t ), "nsg->weights" );
				ntohd( (unsigned char *)nsg->weights, ptr, nsg->c_size );
				ptr += SIZEOF_NETWORK_DOUBLE * nsg->c_size;
			    }
			else
			    nsg->weights = (fastf_t *)NULL;
			sketch_ip->skt_curve.segments[seg_no] = (genptr_t)nsg;
			break;
		    case CURVE_BEZIER_MAGIC:
			bsg = (struct bezier_seg *)bu_malloc( sizeof( struct bezier_seg ), "bsg" );
			bsg->magic = magic;
			bsg->degree = bu_glong( ptr );
			ptr += SIZEOF_NETWORK_LONG;
			bsg->ctl_points = (int *)bu_calloc( bsg->degree+1, sizeof( int ), "bsg->ctl_points" );
			for( i=0 ; i<=bsg->degree ; i++ ) {
			    bsg->ctl_points[i] = bu_glong( ptr );
			    ptr += SIZEOF_NETWORK_LONG;
			}
			sketch_ip->skt_curve.segments[seg_no] = (genptr_t)bsg;
			break;
		    default:
			bu_bomb( "rt_sketch_import: ERROR: unrecognized segment type!!!\n" );
			break;
		}
	}

    crv = &sketch_ip->skt_curve;

    if (crv->seg_count) {
	crv->reverse = (int *)bu_calloc( crv->seg_count, sizeof(int), "crv->reverse" );
    }

    for( i=0 ; i<crv->seg_count ; i++ )
	{
	    crv->reverse[i] = bu_glong( ptr );
	    ptr += SIZEOF_NETWORK_LONG;
	}

    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at end of sketch_import5():\n" );
	    bu_mem_barriercheck();
	}

    return(0);			/* OK */
}


/**
 *			R T _ S K E T C H _ E X P O R T 5
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_sketch_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_sketch_internal	*sketch_ip;
    unsigned char			*cp;
    int				seg_no;
    int				i;
    vect_t				tmp_vec;

    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at start of sketch_export5():\n" );
	    bu_mem_barriercheck();
	}

    RT_CK_DB_INTERNAL(ip);
    if( ip->idb_type != ID_SKETCH )  return(-1);
    sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
    RT_SKETCH_CK_MAGIC(sketch_ip);

    BU_CK_EXTERNAL(ep);

    /* tally up size of buffer needed */
    ep->ext_nbytes = 3 * (3 * SIZEOF_NETWORK_DOUBLE)	/* V, u_vec, v_vec */
	+ 2 * SIZEOF_NETWORK_LONG		/* vert_count and seg_count */
	+ 2 * sketch_ip->vert_count * SIZEOF_NETWORK_DOUBLE	/* 2D-vertices */
	+ sketch_ip->skt_curve.seg_count * SIZEOF_NETWORK_LONG;	/* reverse flags */

    for( seg_no=0 ; seg_no < sketch_ip->skt_curve.seg_count ; seg_no++ )
	{
	    long *lng;
	    struct nurb_seg *nseg;
	    struct bezier_seg *bseg;

	    lng = (long *)sketch_ip->skt_curve.segments[seg_no];
	    switch( *lng )
		{
		    case CURVE_LSEG_MAGIC:
			ep->ext_nbytes += 3 * SIZEOF_NETWORK_LONG;
			break;
		    case CURVE_CARC_MAGIC:
			ep->ext_nbytes += 5 * SIZEOF_NETWORK_LONG + SIZEOF_NETWORK_DOUBLE;
			break;
		    case CURVE_NURB_MAGIC:
			nseg = (struct nurb_seg *)lng;
			ep->ext_nbytes += 3 * SIZEOF_NETWORK_LONG; /* order, pt_type, c_size */
			ep->ext_nbytes +=  SIZEOF_NETWORK_LONG + nseg->k.k_size * SIZEOF_NETWORK_DOUBLE;	/* knot vector */
			ep->ext_nbytes += nseg->c_size * SIZEOF_NETWORK_LONG; /* control point indices */
			if( RT_NURB_IS_PT_RATIONAL( nseg->pt_type ) )
			    ep->ext_nbytes += nseg->c_size * SIZEOF_NETWORK_DOUBLE;	/* weights */
			break;
		    case CURVE_BEZIER_MAGIC:
			bseg = (struct bezier_seg *)lng;
			ep->ext_nbytes += (bseg->degree + 3) * SIZEOF_NETWORK_LONG;
			break;
		    default:
			bu_log( "rt_sketch_export: unsupported segement type (x%x)\n", *lng );
			bu_bomb( "rt_sketch_export: unsupported segement type\n" );
		}
	}
    ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes, "sketch external");

    cp = (unsigned char *)ep->ext_buf;

    /* scale and export */
    VSCALE( tmp_vec, sketch_ip->V, local2mm );
    htond( cp, (unsigned char *)tmp_vec, 3 );
    cp += 3 * SIZEOF_NETWORK_DOUBLE;

    /* uvec and uvec are unit vectors, do not convert to/from mm */
    htond( cp, (unsigned char *)sketch_ip->u_vec, 3 );
    cp += 3 * SIZEOF_NETWORK_DOUBLE;
    htond( cp, (unsigned char *)sketch_ip->v_vec, 3 );
    cp += 3 * SIZEOF_NETWORK_DOUBLE;

    (void)bu_plong( cp, sketch_ip->vert_count );
    cp += SIZEOF_NETWORK_LONG;
    (void)bu_plong( cp, sketch_ip->skt_curve.seg_count );
    cp += SIZEOF_NETWORK_LONG;

    /* convert 2D points to mm */
    for( i=0 ; i<sketch_ip->vert_count ; i++ )
	{
	    point2d_t pt2d;

	    V2SCALE( pt2d, sketch_ip->verts[i], local2mm )
		htond( cp, (const unsigned char *)pt2d, 2 );
	    cp += 2 * SIZEOF_NETWORK_DOUBLE;
	}

    for( seg_no=0 ; seg_no < sketch_ip->skt_curve.seg_count ; seg_no++ )
	{
	    struct line_seg *lseg;
	    struct carc_seg *cseg;
	    struct nurb_seg *nseg;
	    struct bezier_seg *bseg;
	    long *lng;
	    fastf_t tmp_fastf;

	    /* write segment type ID, and segement parameters */
	    lng = (long *)sketch_ip->skt_curve.segments[seg_no];
	    switch (*lng )
		{
		    case CURVE_LSEG_MAGIC:
			lseg = (struct line_seg *)lng;
			(void)bu_plong( cp, CURVE_LSEG_MAGIC );
			cp += SIZEOF_NETWORK_LONG;
			(void)bu_plong( cp, lseg->start );
			cp += SIZEOF_NETWORK_LONG;
			(void)bu_plong( cp, lseg->end );
			cp += SIZEOF_NETWORK_LONG;
			break;
		    case CURVE_CARC_MAGIC:
			cseg = (struct carc_seg *)lng;
			(void)bu_plong( cp, CURVE_CARC_MAGIC );
			cp += SIZEOF_NETWORK_LONG;
			(void)bu_plong( cp, cseg->start );
			cp += SIZEOF_NETWORK_LONG;
			(void)bu_plong( cp, cseg->end );
			cp += SIZEOF_NETWORK_LONG;
			(void) bu_plong( cp, cseg->orientation );
			cp += SIZEOF_NETWORK_LONG;
			(void) bu_plong( cp, cseg->center_is_left );
			cp += SIZEOF_NETWORK_LONG;
			tmp_fastf = cseg->radius * local2mm;
			htond( cp, (unsigned char *)&tmp_fastf, 1 );
			cp += SIZEOF_NETWORK_DOUBLE;
			break;
		    case CURVE_NURB_MAGIC:
			nseg = (struct nurb_seg *)lng;
			(void)bu_plong( cp, CURVE_NURB_MAGIC );
			cp += SIZEOF_NETWORK_LONG;
			(void)bu_plong( cp, nseg->order );
			cp += SIZEOF_NETWORK_LONG;
			(void)bu_plong( cp, nseg->pt_type );
			cp += SIZEOF_NETWORK_LONG;
			(void)bu_plong( cp, nseg->k.k_size );
			cp += SIZEOF_NETWORK_LONG;
			htond( cp, (const unsigned char *)nseg->k.knots, nseg->k.k_size );
			cp += nseg->k.k_size * SIZEOF_NETWORK_DOUBLE;
			(void)bu_plong( cp, nseg->c_size );
			cp += SIZEOF_NETWORK_LONG;
			for( i=0 ; i<nseg->c_size ; i++ )
			    {
				(void)bu_plong( cp, nseg->ctl_points[i] );
				cp += SIZEOF_NETWORK_LONG;
			    }
			if( RT_NURB_IS_PT_RATIONAL( nseg->pt_type ) )
			    {
				htond( cp, (const unsigned char *)nseg->weights, nseg->c_size );
				cp += SIZEOF_NETWORK_DOUBLE * nseg->c_size;
			    }
			break;
		    case CURVE_BEZIER_MAGIC:
			bseg = (struct bezier_seg *)lng;
			(void)bu_plong( cp, CURVE_BEZIER_MAGIC );
			cp += SIZEOF_NETWORK_LONG;
			(void)bu_plong( cp, bseg->degree );
			cp += SIZEOF_NETWORK_LONG;
			for( i=0 ; i<=bseg->degree ; i++ ) {
			    (void)bu_plong( cp, bseg->ctl_points[i] );
			    cp += SIZEOF_NETWORK_LONG;
			}
			break;
		    default:
			bu_bomb( "rt_sketch_export: ERROR: unrecognized curve type!!!!\n" );
			break;

		}
	}

    for( seg_no=0 ; seg_no < sketch_ip->skt_curve.seg_count ; seg_no++ )
	{
	    (void)bu_plong( cp, sketch_ip->skt_curve.reverse[seg_no] );
	    cp += SIZEOF_NETWORK_LONG;
	}

    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at end of sketch_export5():\n" );
	    bu_mem_barriercheck();
	}

    return(0);
}


/**
 *			R T _ S K E T C H _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_sketch_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    register struct rt_sketch_internal	*sketch_ip =
	(struct rt_sketch_internal *)ip->idb_ptr;
    int i;
    int seg_no;
    char	buf[256];
    point_t V;
    vect_t u, v;

    RT_SKETCH_CK_MAGIC(sketch_ip);
    bu_vls_strcat( str, "2D sketch (SKETCH)\n");

    VSCALE( V, sketch_ip->V, mm2local );
    VSCALE( u, sketch_ip->u_vec, mm2local );
    VSCALE( v, sketch_ip->v_vec, mm2local );

    sprintf(buf, "\tV = (%g %g %g),  A = (%g %g %g), B = (%g %g %g)\n\t%d vertices\n",
	    V3INTCLAMPARGS( V ),
	    V3INTCLAMPARGS( u ),
	    V3INTCLAMPARGS( v ),
	    sketch_ip->vert_count );
    bu_vls_strcat( str, buf );

    if( !verbose )
	return( 0 );

    if( sketch_ip->vert_count )
	{
	    bu_vls_strcat( str, "\tVertices:\n\t" );
	    for( i=0 ; i<sketch_ip->vert_count ; i++ )
		{
		    sprintf( buf, " %d-(%g %g)", i, V2INTCLAMPARGS( sketch_ip->verts[i] ) );
		    bu_vls_strcat( str, buf );
		    if( i && (i+1)%3 == 0 )
			bu_vls_strcat( str, "\n\t" );
		}
	}
    bu_vls_strcat( str, "\n" );

    sprintf(buf, "\n\tCurve:\n" );
    bu_vls_strcat( str, buf );
    for( seg_no=0 ; seg_no < sketch_ip->skt_curve.seg_count ; seg_no++ )
	{
	    struct line_seg *lsg;
	    struct carc_seg *csg;
	    struct nurb_seg *nsg;
	    struct bezier_seg *bsg;

	    lsg = (struct line_seg *)sketch_ip->skt_curve.segments[seg_no];
	    switch( lsg->magic )
		{
		    case CURVE_LSEG_MAGIC:
			lsg = (struct line_seg *)sketch_ip->skt_curve.segments[seg_no];
			if( lsg->start >= sketch_ip->vert_count ||
			    lsg->end >= sketch_ip->vert_count )
			    {
				if( sketch_ip->skt_curve.reverse[seg_no] )
				    sprintf( buf, "\t\tLine segment from vertex #%d to #%d\n",
					     lsg->end, lsg->start );
				else
				    sprintf( buf, "\t\tLine segment from vertex #%d to #%d\n",
					     lsg->start, lsg->end );
			    }
			else
			    {
				if( sketch_ip->skt_curve.reverse[seg_no] )
				    sprintf( buf, "\t\tLine segment (%g %g) <-> (%g %g)\n",
					     V2INTCLAMPARGS( sketch_ip->verts[lsg->end] ),
					     V2INTCLAMPARGS( sketch_ip->verts[lsg->start] ) );
				else
				    sprintf( buf, "\t\tLine segment (%g %g) <-> (%g %g)\n",
					     V2INTCLAMPARGS( sketch_ip->verts[lsg->start] ),
					     V2INTCLAMPARGS( sketch_ip->verts[lsg->end] ) );
			    }
			bu_vls_strcat( str, buf );
			break;
		    case CURVE_CARC_MAGIC:
			csg = (struct carc_seg *)sketch_ip->skt_curve.segments[seg_no];
			if( csg->radius < 0.0 )
			    {
				bu_vls_strcat( str, "\t\tFull Circle:\n" );

				if( csg->end >= sketch_ip->vert_count ||
				    csg->start >= sketch_ip->vert_count )
				    {
					sprintf( buf, "\t\tcenter at vertex #%d\n",
						 csg->end );
					bu_vls_strcat( str, buf );
					sprintf( buf, "\t\tpoint on circle at vertex #%d\n",
						 csg->start );
				    }
				else
				    {
					sprintf( buf, "\t\t\tcenter: (%g %g)\n",
						 V2INTCLAMPARGS( sketch_ip->verts[csg->end] ) );
					bu_vls_strcat( str, buf );
					sprintf( buf, "\t\t\tpoint on circle: (%g %g)\n",
						 V2INTCLAMPARGS( sketch_ip->verts[csg->start] ) );
				    }
				bu_vls_strcat( str, buf );
			    }
			else
			    {
				bu_vls_strcat( str, "\t\tCircular Arc:\n" );

				if( csg->end >= sketch_ip->vert_count ||
				    csg->start >= sketch_ip->vert_count )
				    {
					sprintf( buf, "\t\t\tstart at vertex #%d\n",
						 csg->start );
					bu_vls_strcat( str, buf );
					sprintf( buf, "\t\t\tend at vertex #%d\n",
						 csg->end );
					bu_vls_strcat( str, buf );
				    }
				else
				    {
					sprintf( buf, "\t\t\tstart: (%g, %g)\n",
						 V2INTCLAMPARGS( sketch_ip->verts[csg->start] ) );
					bu_vls_strcat( str, buf );
					sprintf( buf, "\t\t\tend: (%g, %g)\n",
						 V2INTCLAMPARGS( sketch_ip->verts[csg->end] ) );
					bu_vls_strcat( str, buf );
				    }
				sprintf( buf, "\t\t\tradius: %g\n", csg->radius*mm2local );
				bu_vls_strcat( str, buf );
				if( csg->orientation )
				    bu_vls_strcat( str, "\t\t\tcurve is clock-wise\n" );
				else
				    bu_vls_strcat( str, "\t\t\tcurve is counter-clock-wise\n" );
				if( csg->center_is_left )
				    bu_vls_strcat( str, "\t\t\tcenter of curvature is left of the line from start point to end point\n" );
				else
				    bu_vls_strcat( str, "\t\t\tcenter of curvature is right of the line from start point to end point\n" );
				if( sketch_ip->skt_curve.reverse[seg_no] )
				    bu_vls_strcat( str, "\t\t\tarc is reversed\n" );
			    }
			break;
		    case CURVE_NURB_MAGIC:
			nsg = (struct nurb_seg *)sketch_ip->skt_curve.segments[seg_no];
			bu_vls_strcat( str, "\t\tNURB Curve:\n" );
			if( RT_NURB_IS_PT_RATIONAL( nsg->pt_type ) )
			    {
				sprintf( buf, "\t\t\tCurve is rational\n" );
				bu_vls_strcat( str, buf );
			    }
			sprintf( buf, "\t\t\torder = %d, number of control points = %d\n",
				 nsg->order, nsg->c_size );
			bu_vls_strcat( str, buf );
			if( nsg->ctl_points[0] >= sketch_ip->vert_count ||
			    nsg->ctl_points[nsg->c_size-1] >= sketch_ip->vert_count )
			    {
				if( sketch_ip->skt_curve.reverse[seg_no] )
				    sprintf( buf, "\t\t\tstarts at vertex #%d\n\t\t\tends at vertex #%d\n",
					     nsg->ctl_points[nsg->c_size-1],
					     nsg->ctl_points[0] );
				else
				    sprintf( buf, "\t\t\tstarts at vertex #%d\n\t\t\tends at vertex #%d\n",
					     nsg->ctl_points[0],
					     nsg->ctl_points[nsg->c_size-1] );
			    }
			else
			    {
				if( sketch_ip->skt_curve.reverse[seg_no] )
				    sprintf( buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
					     V2INTCLAMPARGS( sketch_ip->verts[nsg->ctl_points[nsg->c_size-1]] ),
					     V2INTCLAMPARGS( sketch_ip->verts[nsg->ctl_points[0]] ) );
				else
				    sprintf( buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
					     V2INTCLAMPARGS( sketch_ip->verts[nsg->ctl_points[0]] ),
					     V2INTCLAMPARGS( sketch_ip->verts[nsg->ctl_points[nsg->c_size-1]] ) );
			    }
			bu_vls_strcat( str, buf );
			sprintf( buf, "\t\t\tknot values are %g to %g\n",
				 INTCLAMP(nsg->k.knots[0]), INTCLAMP(nsg->k.knots[nsg->k.k_size-1]) );
			bu_vls_strcat( str, buf );
			break;
		    case CURVE_BEZIER_MAGIC:
			bsg = (struct bezier_seg *)sketch_ip->skt_curve.segments[seg_no];
			bu_vls_strcat( str, "\t\tBezier segment:\n" );
			sprintf( buf, "\t\t\tdegree = %d\n", bsg->degree );
			bu_vls_strcat( str, buf );
			if( bsg->ctl_points[0] >= sketch_ip->vert_count ||
			    bsg->ctl_points[bsg->degree] >= sketch_ip->vert_count ) {
			    if( sketch_ip->skt_curve.reverse[seg_no] ){
				sprintf( buf, "\t\t\tstarts at vertex #%d\n\t\t\tends at vertex #%d\n",
					 bsg->ctl_points[bsg->degree],
					 bsg->ctl_points[0] );
			    } else {
				sprintf( buf, "\t\t\tstarts at vertex #%d\n\t\t\tends at vertex #%d\n",
					 bsg->ctl_points[0],
					 bsg->ctl_points[bsg->degree] );
			    }
			} else {
			    if( sketch_ip->skt_curve.reverse[seg_no] )
				sprintf( buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
					 V2INTCLAMPARGS( sketch_ip->verts[bsg->ctl_points[bsg->degree]]),
					 V2INTCLAMPARGS( sketch_ip->verts[bsg->ctl_points[0]] ) );
			    else
				sprintf( buf, "\t\t\tstarts at (%g %g)\n\t\t\tends at (%g %g)\n",
					 V2INTCLAMPARGS( sketch_ip->verts[bsg->ctl_points[0]] ),
					 V2INTCLAMPARGS( sketch_ip->verts[bsg->ctl_points[bsg->degree]]));
			}
			bu_vls_strcat( str, buf );
			break;
		    default:
			bu_bomb( "rt_sketch_describe: ERROR: unrecognized segment type\n" );
		}
	}

    return(0);
}

void
rt_curve_free(struct curve *crv)
{
    int i;

    if( crv->seg_count )
	bu_free( (char *)crv->reverse, "crv->reverse" );
    for( i=0 ; i<crv->seg_count ; i++ )
	{
	    long *lng;
	    struct nurb_seg *nsg;
	    struct bezier_seg *bsg;

	    lng = (long *)crv->segments[i];
	    switch( *lng )
		{
		    case CURVE_NURB_MAGIC:
			nsg = (struct nurb_seg *)lng;
			bu_free( (char *)nsg->ctl_points, "nsg->ctl_points" );
			if( nsg->weights )
			    bu_free( ( char *)nsg->weights, "nsg->weights" );
			bu_free( ( char *)nsg->k.knots, "nsg->k.knots" );
			bu_free( (char *)lng, "curve segment" );
			break;
		    case CURVE_BEZIER_MAGIC:
			bsg = (struct bezier_seg *)lng;
			bu_free( (char *)bsg->ctl_points, "bsg->ctl_points" );
			bu_free( (char *)lng, "curve segment" );
			break;
		    case CURVE_LSEG_MAGIC:
		    case CURVE_CARC_MAGIC:
			bu_free( (char *)lng, "curve segment" );
			break;
		    default:
			bu_log( "ERROR: rt_curve_free: unrecognized curve segments type!!!!\n");
			break;
		}
	}

    if( crv->seg_count > 0 )
	bu_free( (char *)crv->segments, "crv->segments" );

    crv->seg_count = 0;
    crv->reverse = (int *)NULL;
    crv->segments = (genptr_t)NULL;
}


/**
 *			R T _ S K E T C H _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_sketch_ifree( struct rt_db_internal	*ip )
{
    register struct rt_sketch_internal	*sketch_ip;
    struct curve				*crv;

    RT_CK_DB_INTERNAL(ip);
    sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
    RT_SKETCH_CK_MAGIC(sketch_ip);
    sketch_ip->magic = 0;			/* sanity */

    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at start of sketch_ifree():\n" );
	    bu_mem_barriercheck();
	}

    if( sketch_ip->verts )
	bu_free( (char *)sketch_ip->verts, "sketch_ip->verts" );

    crv = &sketch_ip->skt_curve;

    rt_curve_free( crv );

    bu_free( (char *)sketch_ip, "sketch ifree" );
    ip->idb_ptr = GENPTR_NULL;	/* sanity */

    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at end of sketch_ifree():\n" );
	    bu_mem_barriercheck();
	}

}

void
rt_copy_curve(struct curve *crv_out, const struct curve *crv_in)
{
    int i, j;

    crv_out->seg_count = crv_in->seg_count;
    crv_out->reverse = (int *)bu_calloc( crv_out->seg_count, sizeof( int ), "crv->reverse" );
    crv_out->segments = (genptr_t *)bu_calloc( crv_out->seg_count, sizeof( genptr_t ), "crv->segments" );
    for( j=0 ; j<crv_out->seg_count ; j++ )
	{
	    long *lng;
	    struct line_seg *lsg_out, *lsg_in;
	    struct carc_seg *csg_out, *csg_in;
	    struct nurb_seg *nsg_out, *nsg_in;
	    struct bezier_seg *bsg_out, *bsg_in;

	    crv_out->reverse[j] = crv_in->reverse[j];
	    lng = (long *)crv_in->segments[j];
	    switch( *lng )
		{
		    case CURVE_LSEG_MAGIC:
			lsg_in = (struct line_seg *)lng;
			lsg_out = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "line_seg" );
			crv_out->segments[j] = (genptr_t)lsg_out;
			*lsg_out = *lsg_in;
			break;
		    case CURVE_CARC_MAGIC:
			csg_in = (struct carc_seg *)lng;
			csg_out = (struct carc_seg *)bu_malloc( sizeof( struct carc_seg ), "carc_seg" );
			crv_out->segments[j] = (genptr_t)csg_out;
			*csg_out = *csg_in;
			break;
		    case CURVE_NURB_MAGIC:
			nsg_in = (struct nurb_seg *)lng;
			nsg_out = (struct nurb_seg *)bu_malloc( sizeof( struct nurb_seg ), "nurb_seg" );
			crv_out->segments[j] = (genptr_t)nsg_out;
			*nsg_out = *nsg_in;
			nsg_out->ctl_points = (int *)bu_calloc( nsg_in->c_size, sizeof( int ), "nsg_out->ctl_points" );
			for( i=0 ; i<nsg_out->c_size ; i++ )
			    nsg_out->ctl_points[i] = nsg_in->ctl_points[i];
			if( RT_NURB_IS_PT_RATIONAL( nsg_in->pt_type ) )
			    {
				nsg_out->weights = (fastf_t *)bu_malloc( nsg_out->c_size * sizeof( fastf_t ), "nsg_out->weights" );
				for( i=0 ; i<nsg_out->c_size ; i++ )
				    nsg_out->weights[i] = nsg_in->weights[i];
			    }
			else
			    nsg_out->weights = (fastf_t *)NULL;
			nsg_out->k.knots = bu_malloc( nsg_in->k.k_size * sizeof( fastf_t ), "nsg_out->k.knots" );
			for( i=0 ; i<nsg_in->k.k_size ; i++ )
			    nsg_out->k.knots[i] = nsg_in->k.knots[i];
			break;
		    case CURVE_BEZIER_MAGIC:
			bsg_in = (struct bezier_seg *)lng;
			bsg_out = (struct bezier_seg *)bu_malloc( sizeof( struct bezier_seg ), "bezier_seg" );
			crv_out->segments[j] = (genptr_t)bsg_out;
			*bsg_out = *bsg_in;
			bsg_out->ctl_points = (int *)bu_calloc( bsg_out->degree + 1,
								sizeof( int ), "bsg_out->ctl_points" );
			for( i=0 ; i<=bsg_out->degree ; i++ ) {
			    bsg_out->ctl_points[i] = bsg_in->ctl_points[i];
			}
			break;
		    default:
			bu_bomb( "rt_copy_sketch: ERROR: unrecognized segment type!!!!\n" );
		}
	}

}


struct rt_sketch_internal *
rt_copy_sketch(const struct rt_sketch_internal *sketch_ip)
{
    struct rt_sketch_internal *out;
    int i;
    struct curve *crv_out;

    RT_SKETCH_CK_MAGIC( sketch_ip );

    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at start of rt_copy_sketch():\n" );
	    bu_mem_barriercheck();
	}

    out = (struct rt_sketch_internal *) bu_malloc( sizeof( struct rt_sketch_internal ), "rt_sketch_internal" );
    *out = *sketch_ip;	/* struct copy */

    out->verts = (point2d_t *)bu_calloc( out->vert_count, sizeof( point2d_t ), "out->verts" );
    for( i=0 ; i<out->vert_count ; i++ )
	V2MOVE( out->verts[i], sketch_ip->verts[i] );

    crv_out = &out->skt_curve;
    rt_copy_curve( crv_out, &sketch_ip->skt_curve );

    if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
	    bu_log( "Barrier check at end of rt_copy_sketch():\n" );
	    bu_mem_barriercheck();
	}

    return( out );
}


int
curve_to_tcl_list(struct bu_vls *vls, struct curve *crv)
{
    int i,j;

    bu_vls_printf( vls, " SL {" );
    for( j=0 ; j<crv->seg_count ; j++ )
	{
	    switch( (*(long *)crv->segments[j]) )
		{
		    case CURVE_LSEG_MAGIC:
			{
			    struct line_seg *lsg = (struct line_seg *)crv->segments[j];
			    bu_vls_printf( vls, " { line S %d E %d }", lsg->start, lsg->end );
			}
			break;
		    case CURVE_CARC_MAGIC:
			{
			    struct carc_seg *csg = (struct carc_seg *)crv->segments[j];
			    bu_vls_printf( vls, " { carc S %d E %d R %.25g L %d O %d }",
					   csg->start, csg->end, csg->radius,
					   csg->center_is_left, csg->orientation );
			}
			break;
		    case CURVE_BEZIER_MAGIC:
			{
			    struct bezier_seg *bsg = (struct bezier_seg *)crv->segments[j];
			    bu_vls_printf( vls, " { bezier D %d P {", bsg->degree );
			    for( i=0 ; i<=bsg->degree ; i++ )
				bu_vls_printf( vls, " %d", bsg->ctl_points[i] );
			    bu_vls_printf( vls, " } }" );
			}
			break;
		    case CURVE_NURB_MAGIC:
			{
			    int k;
			    struct nurb_seg *nsg = (struct nurb_seg *)crv->segments[j];
			    bu_vls_printf( vls, " { nurb O %d T %d K {",
					   nsg->order, nsg->pt_type );
			    for( k=0 ; k<nsg->k.k_size ; k++ )
				bu_vls_printf( vls, " %.25g", nsg->k.knots[k] );
			    bu_vls_strcat( vls, "} P {" );
			    for( k=0 ; k<nsg->c_size ; k++ )
				bu_vls_printf( vls, " %d", nsg->ctl_points[k] );
			    if( nsg->weights )
				{
				    bu_vls_strcat( vls, "} W {" );
				    for( k=0 ; k<nsg->c_size ; k++ )
					bu_vls_printf( vls, " %.25g", nsg->weights[k] );
				}
			    bu_vls_strcat( vls, "} }" );
			}
			break;
		}
	}
    bu_vls_strcat( vls, " }" );	/* end of segment list */

    return( 0 );
}


int rt_sketch_tclform( const struct rt_functab *ftp, Tcl_Interp *interp)
{
    RT_CK_FUNCTAB(ftp);

    Tcl_AppendResult( interp,
		      "V {%f %f %f} A {%f %f %f} B {%f %f %f} VL { {%f %f} {%f %f} ...} SL { { segment_data } { segment_data} }" );

    return TCL_OK;
}


int
rt_sketch_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr)
{
    register struct rt_sketch_internal *skt=(struct rt_sketch_internal *)intern->idb_ptr;
    Tcl_DString     ds;
    struct bu_vls   vls;
    int i;
    struct curve	*crv;

    RT_SKETCH_CK_MAGIC( skt );

    Tcl_DStringInit( &ds );
    bu_vls_init( &vls );

    if( attr == (char *)NULL )
	{
	    bu_vls_strcpy( &vls, "sketch" );
	    bu_vls_printf( &vls, " V {%.25g %.25g %.25g}", V3ARGS( skt->V ) );
	    bu_vls_printf( &vls, " A {%.25g %.25g %.25g}", V3ARGS( skt->u_vec ) );
	    bu_vls_printf( &vls, " B {%.25g %.25g %.25g}", V3ARGS( skt->v_vec ) );
	    bu_vls_strcat( &vls, " VL {" );
	    for( i=0 ; i<skt->vert_count ; i++ )
		bu_vls_printf( &vls, " {%.25g %.25g}", V2ARGS( skt->verts[i] ) );
	    bu_vls_strcat( &vls, " }" );

	    crv = &skt->skt_curve;
	    if( curve_to_tcl_list( &vls, crv ) )
		{
		    bu_vls_free( &vls );
		    return( TCL_ERROR );
		}
	}
    else if( !strcmp( attr, "V" ) )
	bu_vls_printf( &vls, "%.25g %.25g %.25g", V3ARGS( skt->V ) );
    else if( !strcmp( attr, "A" ) )
	bu_vls_printf( &vls, "%.25g %.25g %.25g", V3ARGS( skt->u_vec ) );
    else if( !strcmp( attr, "B" ) )
	bu_vls_printf( &vls, "%.25g %.25g %.25g", V3ARGS( skt->v_vec ) );
    else if( !strcmp( attr, "VL" ) )
	{
	    for( i=0 ; i<skt->vert_count ; i++ )
		bu_vls_printf( &vls, " {%.25g %.25g}", V2ARGS( skt->verts[i] ) );
	}
    else if( !strcmp( attr, "SL" ) )
	{
	    crv = &skt->skt_curve;
	    if( curve_to_tcl_list( &vls, crv ) )
		{
		    bu_vls_free( &vls );
		    return( TCL_ERROR );
		}
	}
    else if( *attr == 'V' )
	{
	    i = atoi( (attr+1) );
	    if( i < 0 || i >= skt->vert_count )
		{
		    Tcl_SetResult( interp, "ERROR: Illegal vertex number\n", TCL_STATIC );
		    bu_vls_free( &vls );
		    return( TCL_ERROR );
		}

	    bu_vls_printf( &vls, "%.25g %.25g", V2ARGS( skt->verts[i] ) );
	}
    else	/* unrecognized attribute */
	{
	    Tcl_SetResult( interp, "ERROR: Unknown attribute, choices are V, A, B, VL, SL, or V#\n", TCL_STATIC );
	    bu_vls_free( &vls );
	    return( TCL_ERROR );
	}

    Tcl_DStringAppend( &ds, bu_vls_addr( &vls ), -1 );
    Tcl_DStringResult( interp, &ds );
    Tcl_DStringFree( &ds );
    bu_vls_free( &vls );
    return( TCL_OK );
}


int
get_tcl_curve(Tcl_Interp *interp, struct curve *crv, Tcl_Obj *seg_list)
{
    int seg_count;
    int ret, j;

    /* get number of segments */
    seg_count = 0;
    if( (ret=Tcl_ListObjLength( interp, seg_list, &seg_count ) ))
	{
	    return( ret );
	}

    if( seg_count )
	{
	    crv->seg_count = seg_count;
	    crv->reverse = (int *)bu_calloc( seg_count, sizeof( int ), "crv->reverse" );
	    crv->segments = (genptr_t *)bu_calloc( seg_count, sizeof( genptr_t ), "crv->segments" );
	}

    /* loop through all the segments */
    for( j=0 ; j<seg_count ; j++ )
	{
	    Tcl_Obj *seg, *seg_type, *seg_elem, *seg_val;
	    char *type, *elem;
	    int k, seg_len;


	    /* get the next segment */
	    if( (ret=Tcl_ListObjIndex( interp, seg_list, j, &seg ) ))
		{
		    return( ret );
		}

	    if( (ret=Tcl_ListObjLength( interp, seg, &seg_len )) )
		return( ret );

	    /* get the segment type */
	    if( (ret=Tcl_ListObjIndex( interp, seg, 0, &seg_type ) ))
		return( ret );
	    type = Tcl_GetString( seg_type );

	    if( !strcmp( type, "line" ) )
		{
		    struct line_seg *lsg;

		    lsg = (struct line_seg *)bu_calloc( 1, sizeof( struct line_seg ), "lsg" );
		    for( k=1 ; k<seg_len ; k += 2 )
			{
			    if( (ret=Tcl_ListObjIndex( interp, seg, k, &seg_elem ) ))
				return( ret );

			    if( (ret=Tcl_ListObjIndex( interp, seg, k+1, &seg_val ) ))
				return( ret );

			    elem = Tcl_GetString( seg_elem );
			    switch( *elem )
				{
				    case 'S':
					Tcl_GetIntFromObj( interp, seg_val, &lsg->start );
					break;
				    case 'E':
					Tcl_GetIntFromObj( interp, seg_val, &lsg->end );
					break;
				}
			}
		    lsg->magic = CURVE_LSEG_MAGIC;
		    crv->segments[j] = (genptr_t)lsg;
		}
	    else if( !strcmp( type, "bezier" ) )
		{
		    struct bezier_seg *bsg;
		    int num_points;

		    bsg = (struct bezier_seg *)bu_calloc( 1, sizeof( struct bezier_seg ), "bsg" );
		    for( k=1 ; k<seg_len ; k+= 2 ) {

			if( (ret=Tcl_ListObjIndex( interp, seg, k, &seg_elem ) ))
			    return( ret );

			if( (ret=Tcl_ListObjIndex( interp, seg, k+1, &seg_val ) ))
			    return( ret );

			elem = Tcl_GetString( seg_elem );
			switch( *elem )
			    {
				case 'D': /* degree */
				    Tcl_GetIntFromObj( interp, seg_val,
						       &bsg->degree );
				    break;
				case 'P': /* list of control points */
				    num_points = 0;
				    (void)tcl_obj_to_int_array( interp,
								seg_val, &bsg->ctl_points,
								&num_points );

				    if( num_points != bsg->degree + 1 ) {
					Tcl_SetResult( interp, "ERROR: degree and number of control points disagree for a Bezier segment\n", TCL_STATIC );
					return( TCL_ERROR );
				    }
			    }
		    }
		    bsg->magic = CURVE_BEZIER_MAGIC;
		    crv->segments[j] = (genptr_t)bsg;
		}
	    else if( !strcmp( type, "carc" ) )
		{
		    struct carc_seg *csg;
		    double tmp;

		    csg = (struct carc_seg *)bu_calloc( 1, sizeof( struct carc_seg ), "csg" );
		    for( k=1 ; k<seg_len ; k += 2 )
			{
			    if( (ret=Tcl_ListObjIndex( interp, seg, k, &seg_elem ) ))
				return( ret );

			    if( (ret=Tcl_ListObjIndex( interp, seg, k+1, &seg_val ) ))
				return( ret );

			    elem = Tcl_GetString( seg_elem );
			    switch( *elem )
				{
				    case 'S':
					Tcl_GetIntFromObj( interp, seg_val, &csg->start );
					break;
				    case 'E':
					Tcl_GetIntFromObj( interp, seg_val, &csg->end );
					break;
				    case 'R':
					Tcl_GetDoubleFromObj( interp, seg_val, &tmp );
					csg->radius = tmp;
					break;
				    case 'L' :
					Tcl_GetBooleanFromObj( interp, seg_val, &csg->center_is_left );
					break;
				    case 'O':
					Tcl_GetBooleanFromObj( interp, seg_val, &csg->orientation );
					break;
				}
			}
		    csg->magic = CURVE_CARC_MAGIC;
		    crv->segments[j] = (genptr_t)csg;
		}
	    else if( !strcmp( type, "nurb" ) )
		{
		    struct nurb_seg *nsg;

		    nsg = (struct nurb_seg *)bu_calloc( 1, sizeof( struct nurb_seg ), "nsg" );
		    for( k=1 ; k<seg_len ; k += 2 )
			{
			    if( (ret=Tcl_ListObjIndex( interp, seg, k, &seg_elem ) ))
				return( ret );

			    if( (ret=Tcl_ListObjIndex( interp, seg, k+1, &seg_val ) ))
				return( ret );

			    elem = Tcl_GetString( seg_elem );
			    switch( *elem )
				{
				    case 'O':
					Tcl_GetIntFromObj( interp, seg_val, &nsg->order );
					break;
				    case 'T':
					Tcl_GetIntFromObj( interp, seg_val, &nsg->pt_type );
					break;
				    case 'K':
					tcl_obj_to_fastf_array( interp, seg_val, &nsg->k.knots, &nsg->k.k_size );
					break;
				    case 'P' :
					tcl_obj_to_int_array( interp, seg_val, &nsg->ctl_points, &nsg->c_size );
					break;
				    case 'W':
					tcl_obj_to_fastf_array( interp, seg_val, &nsg->weights, &nsg->c_size );
					break;
				}
			}
		    nsg->magic = CURVE_NURB_MAGIC;
		    crv->segments[j] = (genptr_t)nsg;
		}
	    else
		{
		    Tcl_ResetResult( interp );
		    Tcl_AppendResult( interp, "ERROR: Unrecognized segment type: ",
				      Tcl_GetString( seg ), (char *)NULL);
		    return( TCL_ERROR );
		}
	}

    return( TCL_OK );
}


int
rt_sketch_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv)
{
    struct rt_sketch_internal *skt;
    int ret, array_len;
    fastf_t *new;

    RT_CK_DB_INTERNAL( intern );
    skt = (struct rt_sketch_internal *)intern->idb_ptr;
    RT_SKETCH_CK_MAGIC( skt );

    while( argc >= 2 )
	{
	    if( !strcmp( argv[0], "V" ) )
		{
		    new = skt->V;
		    array_len = 3;
		    if( tcl_list_to_fastf_array( interp, argv[1], &new, &array_len) !=
			array_len ) {
			Tcl_SetResult( interp,
				       "ERROR: Incorrect number of coordinates for vertex\n",
				       TCL_STATIC );
			return( TCL_ERROR );
		    }
		}
	    else if( !strcmp( argv[0], "A" ) )
		{
		    new = skt->u_vec;
		    array_len = 3;
		    if( tcl_list_to_fastf_array( interp, argv[1], &new, &array_len) !=
			array_len ) {
			Tcl_SetResult( interp,
				       "ERROR: Incorrect number of coordinates for vertex\n",
				       TCL_STATIC );
			return( TCL_ERROR );
		    }
		}
	    else if( !strcmp( argv[0], "B" ) )
		{
		    new = skt->v_vec;
		    array_len = 3;
		    if( tcl_list_to_fastf_array( interp, argv[1], &new, &array_len) !=
			array_len ) {
			Tcl_SetResult( interp,
				       "ERROR: Incorrect number of coordinates for vertex\n",
				       TCL_STATIC );
			return( TCL_ERROR );
		    }
		}
	    else if( !strcmp( argv[0], "VL" ) )
		{
		    fastf_t *new_verts=(fastf_t *)NULL;
		    int len;
		    char *ptr;

		    /* the vertex list is a list of lists (each element is a list of two coordinates)
		     * so eliminate all the '{' and '}' chars in the list
		     */
		    ptr = argv[1];
		    while( *ptr != '\0' )
			{
			    if( *ptr == '{' || *ptr == '}' )
				*ptr = ' ';
			    ptr++;
			}

		    len = 0;
		    (void)tcl_list_to_fastf_array( interp, argv[1], &new_verts, &len );
		    if( len%2 ) {
			Tcl_SetResult( interp,
				       "ERROR: Incorrect number of coordinates for vertices\n",
				       TCL_STATIC );
			return( TCL_ERROR );
		    }

		    if( skt->verts )
			bu_free( (char *)skt->verts, "verts" );
		    skt->verts = (point2d_t *)new_verts;
		    skt->vert_count = len / 2;
		}
	    else if( !strcmp( argv[0], "SL" ) )	/* the entire segment list */
		{
		    Tcl_Obj *tmp;
		    struct curve *crv;

		    /* create a Tcl object */
		    tmp = Tcl_NewStringObj( argv[1], -1 );

		    crv = &skt->skt_curve;
		    crv->seg_count = 0;
		    crv->reverse = (int *)NULL;
		    crv->segments = (genptr_t)NULL;

		    if( (ret=get_tcl_curve( interp, crv, tmp )) != TCL_OK )
			return( ret );
		}
	    else if( *argv[0] == 'V' && isdigit( *(argv[0]+1) )  )
		{
		    /* changing a specific vertex */
		    int vert_no;
		    fastf_t *new_vert;

		    vert_no = atoi( argv[0] + 1 );
		    new_vert = skt->verts[vert_no];
		    if( vert_no < 0 || vert_no > skt->vert_count )
			{
			    Tcl_SetResult( interp, "ERROR: Illegal vertex number\n",
					   TCL_STATIC );
			    return( TCL_ERROR );
			}
		    array_len = 2;
		    if(tcl_list_to_fastf_array( interp, argv[1], &new_vert, &array_len)
		       != array_len )
			{
			    Tcl_SetResult( interp,
					   "ERROR: Incorrect number of coordinates for vertex\n",
					   TCL_STATIC );
			    return( TCL_ERROR );
			}
		}

	    argc -= 2;
	    argv += 2;
	}

    return( TCL_OK );
}


void
rt_curve_reverse_segment( long *lng )
{
    struct line_seg *lsg;
    struct carc_seg *csg;
    struct bezier_seg *bsg;
    int tmp, i;

    switch( *lng ) {
	case CURVE_LSEG_MAGIC:
	    lsg = (struct line_seg *)lng;
	    tmp = lsg->start;
	    lsg->start = lsg->end;
	    lsg->end = tmp;
	    break;
	case CURVE_CARC_MAGIC:
	    csg = (struct carc_seg *)lng;
	    if( csg->radius < 0.0 ) {
		/* this is a full circle */
		csg->orientation = !csg->orientation; /* no real effect, but just for completeness */
	    } else {
		tmp = csg->start;
		csg->start = csg->end;
		csg->end = tmp;
		csg->center_is_left = !csg->center_is_left;
		csg->orientation = !csg->orientation;
	    }
	    break;
	case CURVE_BEZIER_MAGIC:
	    bsg = (struct bezier_seg *)lng;
	    for( i=0 ; i<bsg->degree/2 ; i++ ) {
		tmp = bsg->ctl_points[i];
		bsg->ctl_points[i] = bsg->ctl_points[bsg->degree-i];
		bsg->ctl_points[bsg->degree-i] = tmp;
	    }
	    break;
    }
}


void
rt_curve_order_segments( struct curve *crv )
{
    int i, j, k;
    int seg_count;
    int start1, end1, start2, end2, start3, end3;

    seg_count = crv->seg_count;
    if( seg_count < 2 ) {
	return;
    }

    for( j=1 ; j<seg_count ; j++ ) {
	i = j - 1;

	get_indices( crv->segments[i], &start1, &end1 );
	get_indices( crv->segments[j], &start2, &end2 );

	if( end1 != start2 ) {
	    int fixed=0;

	    for( k=j+1 ; k<seg_count ; k++ ) {
		get_indices( crv->segments[k], &start3, &end3 );
		if( start3 == end1 ) {
		    int tmp_reverse;
		    genptr_t tmp_seg;

		    /* exchange j and k segments */
		    tmp_seg = crv->segments[j];
		    crv->segments[j] = crv->segments[k];
		    crv->segments[k] = tmp_seg;

		    tmp_reverse = crv->reverse[j];
		    crv->reverse[j] = crv->reverse[k];
		    crv->reverse[k] = tmp_reverse;
		    fixed = 1;
		    break;
		}
	    }
	    if( !fixed ) {
		/* try reversing a segment */
		for( k=j ; k<seg_count ; k++ ) {
		    get_indices( crv->segments[k], &start3, &end3 );
		    if( end3 == end1 ) {
			int tmp_reverse;
			genptr_t tmp_seg;

			rt_curve_reverse_segment( crv->segments[k] );

			if( k != j ) {
			    /* exchange j and k segments */
			    tmp_seg = crv->segments[j];
			    crv->segments[j] = crv->segments[k];
			    crv->segments[k] = tmp_seg;

			    tmp_reverse = crv->reverse[j];
			    crv->reverse[j] = crv->reverse[k];
			    crv->reverse[k] = tmp_reverse;
			}
			fixed = 1;
			break;
		    }
		}
	    }
	}
    }
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
