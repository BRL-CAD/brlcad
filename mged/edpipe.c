/*
 *			E D P I P E . C
 *
 * Functions -
 *	split_pipeseg - split a pipe segment at a given point
 *	find_pipeseg_nearest_pt - find which segment of a pipe is nearest
 *			the ray from "pt" in the viewing direction (for segment selection in MGED)
 *
 *  Author -
 *	John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
	
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "wdb.h"

#include "./ged.h"
#include "./solid.h"
#include "./sedit.h"
#include "./dm.h"
#include "./menu.h"

extern struct rt_tol		mged_tol;	/* from ged.c */

void
pipe_seg_scale_od( ps, scale )
struct wdb_pipeseg *ps;
fastf_t scale;
{
	struct wdb_pipeseg *prev;
	struct wdb_pipeseg *next;
	int seg_count=0;
	int id_eq_od=0;

	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	/* make sure we can make this change */
	if( scale < 1.0 )
	{
		/* need to check that the new OD is not less than ID
		 * of any affected segment.
		 */
		prev = RT_LIST_PREV( wdb_pipeseg, &ps->l );
		while( prev->l.magic != RT_LIST_HEAD_MAGIC &&
			 prev->ps_type == WDB_PIPESEG_TYPE_BEND )
		{
			if( prev->ps_id > scale*prev->ps_od )
			{
				rt_log( "Cannot make OD smaller than ID\n" );
				return;
			}
			seg_count++;
			if( prev->ps_id == scale*prev->ps_od )
				id_eq_od++;
			prev = RT_LIST_PREV( wdb_pipeseg, &prev->l );
		}
		if( ps->ps_type == WDB_PIPESEG_TYPE_BEND )
		{
			next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
			while( next->ps_type == WDB_PIPESEG_TYPE_BEND )
			{
				if( next->ps_id > scale*prev->ps_od )
				{
					rt_log( "Cannot make OD smaller than ID\n" );
					return;
				}
				seg_count++;
				if( next->ps_id == scale*next->ps_od )
					id_eq_od++;
				next = RT_LIST_NEXT( wdb_pipeseg, &next->l );
			}
			if( next->ps_id > scale*prev->ps_od )
			{
				rt_log( "Cannot make OD smaller than ID\n" );
				return;
			}
			seg_count++;
			if( next->ps_id == scale*next->ps_od )
				id_eq_od++;
		}
		if( seg_count && id_eq_od == seg_count )
		{
			rt_log( "Cannot make zero wall thickness pipe\n" );
			return;
		}
	}

	ps->ps_od *= scale;
	prev = RT_LIST_PREV( wdb_pipeseg, &ps->l );
	while( prev->l.magic != RT_LIST_HEAD_MAGIC &&
		 prev->ps_type == WDB_PIPESEG_TYPE_BEND )
	{
		prev->ps_od *= scale;
		prev = RT_LIST_PREV( wdb_pipeseg, &prev->l );
	}

	if( ps->ps_type == WDB_PIPESEG_TYPE_BEND )
	{
		next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
		while( next->ps_type == WDB_PIPESEG_TYPE_BEND )
		{
			next->ps_od *= scale;
			next = RT_LIST_PNEXT_CIRC( wdb_pipeseg, &next->l );
		}
		next->ps_od *= scale;
	}
}

static void
break_bend( ps, n1, n2, angle )
struct wdb_pipeseg *ps;
vect_t n1,n2;
fastf_t angle;
{
	struct wdb_pipeseg *next;
	struct wdb_pipeseg *new_bend;
	vect_t to_end;
	point_t new_pt;
	fastf_t n1_coeff,n2_coeff;
	fastf_t bend_radius;

	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	if( ps->ps_type != WDB_PIPESEG_TYPE_BEND )
		rt_bomb( "break_bend: Called with a non-bend pipe segment\n" );

	next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
	VSUB2( to_end, next->ps_start, ps->ps_bendcenter );
	bend_radius = MAGNITUDE( to_end );

	n1_coeff = bend_radius*cos( angle );
	n2_coeff = bend_radius*sin( angle );
	VJOIN2( new_pt, ps->ps_bendcenter, n1_coeff, n1, n2_coeff, n2 );

	GETSTRUCT( new_bend, wdb_pipeseg );
	new_bend->ps_type = WDB_PIPESEG_TYPE_BEND;
	new_bend->l.magic = WDB_PIPESEG_MAGIC;
	new_bend->ps_od = ps->ps_od;
	new_bend->ps_id = ps->ps_id;

	VMOVE( new_bend->ps_bendcenter, ps->ps_bendcenter );
	VMOVE( new_bend->ps_start, new_pt );

	RT_LIST_APPEND( &ps->l, &new_bend->l );
}

static void
get_bend_start_line( ps, pt, dir )
CONST struct wdb_pipeseg *ps;
point_t pt;
vect_t dir;
{
	struct wdb_pipeseg *prev;
	struct wdb_pipeseg *next;

	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	if( ps->ps_type != WDB_PIPESEG_TYPE_BEND )
		rt_bomb( "get_bend_start_line called woth non-bend pipe segment\n" );

	VMOVE( pt, ps->ps_start );

	prev = RT_LIST_PREV( wdb_pipeseg, &ps->l );
	if( prev->l.magic != RT_LIST_HEAD_MAGIC )
	{
		if( prev->ps_type == WDB_PIPESEG_TYPE_LINEAR )
		{
			VSUB2( dir, pt, prev->ps_start );
			VUNITIZE( dir );
			return;
		}
	}

	next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
	{
		vect_t to_start;
		vect_t to_end;
		vect_t normal;

		VSUB2( to_start, ps->ps_start, ps->ps_bendcenter );
		VSUB2( to_end, next->ps_start, ps->ps_bendcenter );
		VCROSS( normal, to_start, to_end );
		VCROSS( dir, normal, to_start );
		VUNITIZE( dir );
		return;
	}

	rt_bomb( "get_bend_start_line: Cannot get a start line for pipe bend segment\n" );
}

static void
get_bend_end_line( ps, pt, dir )
CONST struct wdb_pipeseg *ps;
point_t pt;
vect_t dir;
{
	struct wdb_pipeseg *next;

	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	if( ps->ps_type != WDB_PIPESEG_TYPE_BEND )
		rt_bomb( "get_bend_end_line called woth non-bend pipe segment\n" );

	next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
	VMOVE( pt, next->ps_start );
	if( next->ps_type == WDB_PIPESEG_TYPE_LINEAR )
	{
		struct wdb_pipeseg *nnext;

		nnext = RT_LIST_NEXT( wdb_pipeseg, &next->l );
		VSUB2( dir, pt, nnext->ps_start );
		VUNITIZE( dir );
		return;
	}
	if( next->ps_type == WDB_PIPESEG_TYPE_BEND )
	{
		vect_t to_start;
		vect_t to_end;
		vect_t normal;

		VSUB2( to_start, ps->ps_start, ps->ps_bendcenter );
		VSUB2( to_end, next->ps_start, ps->ps_bendcenter );
		VCROSS( normal, to_start, to_end );
		VCROSS( dir, normal, to_end );
		VUNITIZE( dir );
		return;
	}

	rt_bomb( "get_bend_end_line: Cannot get a end line for pipe bend segment\n" );
}

static fastf_t
get_bend_radius( ps )
CONST struct wdb_pipeseg *ps;
{
	struct wdb_pipeseg *next;
	struct wdb_pipeseg *prev;
	fastf_t bend_radius=(-1.0);
	vect_t to_start;

	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	if( ps->ps_type == WDB_PIPESEG_TYPE_BEND )
	{
		VSUB2( to_start, ps->ps_start, ps->ps_bendcenter );
		bend_radius = MAGNITUDE( to_start );
		return( bend_radius );
	}

	next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
	prev = RT_LIST_PREV( wdb_pipeseg, &ps->l );

	while( next->ps_type != WDB_PIPESEG_TYPE_END || prev->l.magic != RT_LIST_HEAD_MAGIC )
	{
		if( next->ps_type != WDB_PIPESEG_TYPE_END )
		{
			if( next->ps_type == WDB_PIPESEG_TYPE_BEND )
			{
				VSUB2( to_start, next->ps_start, next->ps_bendcenter );
				bend_radius = MAGNITUDE( to_start );
				return( bend_radius );
			}
		}
		if( prev->l.magic != RT_LIST_HEAD_MAGIC )
		{
			if( prev->ps_type == WDB_PIPESEG_TYPE_BEND )
			{
				VSUB2( to_start, prev->ps_start, prev->ps_bendcenter );
				bend_radius = MAGNITUDE( to_start );
				return( bend_radius );
			}
		}
	}

	if( bend_radius < 0.0 )
		bend_radius = ps->ps_od;

	return( bend_radius );
}

void
split_pipeseg( pipe_hd, ps, pt )
struct rt_list *pipe_hd;
struct wdb_pipeseg *ps;
point_t pt;
{
	struct wdb_pipeseg *new_linear;
	struct wdb_pipeseg *new_bend;
	struct wdb_pipeseg *start_bend=(struct wdb_pipeseg *)NULL;
	struct wdb_pipeseg *end_bend=(struct wdb_pipeseg *)NULL;
	struct wdb_pipeseg *next;
	struct wdb_pipeseg *prev;
	vect_t new_dir1,new_dir2;
	vect_t v1,v2;
	vect_t n1,n2;
	vect_t normal;
	fastf_t bend_radius;
	fastf_t alpha;

	RT_CK_LIST_HEAD( pipe_hd );
	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	if( ps->ps_type != WDB_PIPESEG_TYPE_LINEAR )
	{
		rt_log( "Can only split linear pipe segments\n" );
		return;
	}

	bend_radius = get_bend_radius( ps );

	GETSTRUCT( new_linear, wdb_pipeseg );
	new_linear->ps_type = WDB_PIPESEG_TYPE_LINEAR;
	new_linear->l.magic = WDB_PIPESEG_MAGIC;
	new_linear->ps_od = ps->ps_od;
	new_linear->ps_id = ps->ps_id;

	GETSTRUCT( new_bend, wdb_pipeseg );
	new_bend->ps_type = WDB_PIPESEG_TYPE_BEND;
	new_bend->l.magic = WDB_PIPESEG_MAGIC;
	new_bend->ps_od = ps->ps_od;
	new_bend->ps_id = ps->ps_id;

	VSUB2( new_dir1, ps->ps_start, pt );
	VUNITIZE( new_dir1 );

	next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
	VSUB2( new_dir2, next->ps_start, pt );
	VUNITIZE( new_dir2 );

	prev = RT_LIST_PREV( wdb_pipeseg, &ps->l );
	if( RT_LIST_IS_HEAD( &prev->l, pipe_hd ) && next->ps_type == WDB_PIPESEG_TYPE_END )
	{
		/* this is the only segment in the current pipe */
		VCROSS( normal, new_dir1, new_dir2 );

		VCROSS( n1, normal, new_dir1 );
		VUNITIZE( n1 );

		VCROSS( n2, new_dir2, normal );
		VUNITIZE( n2 );

		alpha = bend_radius*(VDOT( n2, new_dir1 ) + VDOT( n1, new_dir2 ) )/
			(2.0 * (1.0 - VDOT( new_dir1, new_dir2 ) ) );
		VJOIN2( new_bend->ps_bendcenter, pt, alpha, new_dir1, bend_radius, n1 );
		VJOIN1( new_bend->ps_start, pt, alpha, new_dir1 );

		VJOIN1( new_linear->ps_start, pt, alpha, new_dir2 );

		RT_LIST_APPEND( &ps->l, &new_bend->l );
		RT_LIST_APPEND( &new_bend->l, &new_linear->l );

		return;
	}

	/* process starting end of "ps" */
	if( RT_LIST_NOT_HEAD( &prev->l, pipe_hd ) )
	{
		if( prev->ps_type == WDB_PIPESEG_TYPE_LINEAR )
		{
			vect_t dir;

			/* two consecutive linear sections, probably needs a bend inserted */
			VSUB2( dir, prev->ps_start, ps->ps_start );
			VUNITIZE( dir );

			if( !NEAR_ZERO( VDOT( dir, new_dir1 ) - 1.0, RT_DOT_TOL ) )
			{
				struct wdb_pipeseg *start_bend;
				point_t pt1;
				vect_t d1,d2;

				/* does need a bend */
				GETSTRUCT( start_bend, wdb_pipeseg );
				start_bend->ps_type = WDB_PIPESEG_TYPE_BEND;
				start_bend->l.magic = WDB_PIPESEG_MAGIC;
				start_bend->ps_od = ps->ps_od;
				start_bend->ps_id = ps->ps_id;

				VMOVE( pt1, ps->ps_start );
				VMOVE( d1, dir );
				VREVERSE( d2, new_dir1 );
				VCROSS( normal, d1, d2 );

				VCROSS( n1, normal, d1 );
				VUNITIZE( n1 );

				VCROSS( n2, d2, normal );
				VUNITIZE( n2 );

				alpha = bend_radius*(VDOT( n2, d1 ) + VDOT( n1, d2 ) )/
					(2.0 * (1.0 - VDOT( d1, d2 ) ) );

				VJOIN2( start_bend->ps_bendcenter, ps->ps_start, alpha, d1, bend_radius, n1 );
				VJOIN1( start_bend->ps_start, ps->ps_start, alpha, d1 );
				VJOIN1( ps->ps_start, ps->ps_start, alpha, d2 );

				RT_LIST_INSERT( &ps->l, &start_bend->l );
			}
		}
		else if( prev->ps_type == WDB_PIPESEG_TYPE_BEND )
		{
			point_t pt1;
			point_t pt2;
			vect_t d1;
			vect_t d2;
			vect_t d3;
			fastf_t local_bend_radius;
			fastf_t dist_to_center;
			fastf_t angle;
			mat_t mat;

			/* get bend radius for this bend */
			VSUB2( d1, prev->ps_bendcenter, prev->ps_start );
			local_bend_radius = MAGNITUDE( d1 );

			/* calculate new bend center */
			get_bend_start_line( prev, pt1, n2 );
			VSUB2( d2, pt, pt1 );
			VCROSS( normal, n2, d2 );
			VUNITIZE( normal );
			VCROSS( n1, normal, n2 );
			VUNITIZE( n1 );
			VJOIN1( prev->ps_bendcenter, prev->ps_start, local_bend_radius, n1 );

			/* calculate new start point for "ps" */
			VSUB2( d1, pt, prev->ps_bendcenter );
			angle = asin( local_bend_radius/MAGNITUDE( d1 ) );
			mat_arb_rot( mat, prev->ps_bendcenter, normal, angle );
			VCROSS( d2, d1, normal )
			MAT4X3VEC( d3, mat, d2 );
			VUNITIZE( d3 );
			VJOIN1( ps->ps_start, prev->ps_bendcenter, local_bend_radius, d3 );

			/* Make sure resulting bend is less than 180 degrees */
			angle = atan2( VDOT( d3, n2 ),	VDOT( d3, n1 ) );
			if( angle < 0.0 )
				angle += 2.0 * rt_pi;

			if( angle > rt_pi - RT_DOT_TOL )
				break_bend( prev, n1, n2, angle/2 );
		}
	}

	/* process end point of "ps" */
	if( next->ps_type != WDB_PIPESEG_TYPE_END )
	{
		if( next->ps_type == WDB_PIPESEG_TYPE_LINEAR )
		{
			vect_t dir;
			struct wdb_pipeseg *nnext;

			/* two consecutive linear sections, probably needs a bend inserted */
			nnext = RT_LIST_NEXT( wdb_pipeseg, &next->l );
			VSUB2( dir, nnext->ps_start, next->ps_start );
			VUNITIZE( dir );

			if( !NEAR_ZERO( VDOT( dir, new_dir2 ) - 1.0, RT_DOT_TOL ) )
			{
				point_t pt1;
				vect_t d1,d2;

				/* does need a bend */
				GETSTRUCT( end_bend, wdb_pipeseg );
				end_bend->ps_type = WDB_PIPESEG_TYPE_BEND;
				end_bend->l.magic = WDB_PIPESEG_MAGIC;
				end_bend->ps_od = ps->ps_od;
				end_bend->ps_id = ps->ps_id;

				VMOVE( pt1, nnext->ps_start );
				VMOVE( d1, dir );
				VREVERSE( d2, new_dir2 );
				VCROSS( normal, d1, d2 );

				VCROSS( n1, normal, d1 );
				VUNITIZE( n1 );

				VCROSS( n2, d2, normal );
				VUNITIZE( n2 );

				alpha = bend_radius*(VDOT( n2, d1 ) + VDOT( n1, d2 ) )/
					(2.0 * (1.0 - VDOT( d1, d2 ) ) );

				VJOIN2( end_bend->ps_bendcenter, next->ps_start, alpha, d1, bend_radius, n1 );
				VJOIN1( end_bend->ps_start, next->ps_start, alpha, d1 );
				VJOIN1( next->ps_start, next->ps_start, alpha, d2 );

				RT_LIST_APPEND( &ps->l, &end_bend->l );
			}
		}
		else if( next->ps_type == WDB_PIPESEG_TYPE_BEND )
		{
			struct wdb_pipeseg *nnext;
			mat_t mat;
			point_t pt1,pt2;
			vect_t d1,d2,d3;
			fastf_t local_bend_radius;
			fastf_t dist_to_center;
			fastf_t angle;

			/* get bend radius for this bend */
			VSUB2( d1, next->ps_bendcenter, next->ps_start );
			local_bend_radius = MAGNITUDE( d1 );

			/* calculate new bend center */
			get_bend_end_line( next, pt1, n2 );
			VSUB2( d2, pt, pt1 );
			VCROSS( normal, n2, d2 );
			VUNITIZE( normal );
			VCROSS( n1, normal, n2 );
			VUNITIZE( n1 );
			nnext = RT_LIST_NEXT( wdb_pipeseg, &next->l );
			VJOIN1( next->ps_bendcenter, nnext->ps_start, local_bend_radius, n1 );

			/* calculate new end point for "ps" (next->ps_start) */
			VSUB2( d1, pt, next->ps_bendcenter );
			VCROSS( d2, d1, normal );
			angle = asin( local_bend_radius/MAGNITUDE( d1 ) );
			mat_arb_rot( mat, next->ps_bendcenter, normal, angle );
			MAT4X3VEC( d3, mat, d2 );
			VUNITIZE( d3 );
			VJOIN1( next->ps_start, next->ps_bendcenter, local_bend_radius, d3 );

			/* Make sure resulting bend is less than 180 degrees */
			angle = atan2( VDOT( d3, n2 ), -VDOT( d3, n1 ) );
			if( angle < 0.0 )
				angle += 2.0 * rt_pi;

			if( angle > rt_pi - RT_DOT_TOL )
				break_bend( next, n1, n2, angle/2 );
		}
	}

	VSUB2( new_dir1, ps->ps_start, pt );
	VUNITIZE( new_dir1 );

	next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
	VSUB2( new_dir2, next->ps_start, pt );
	VUNITIZE( new_dir2 );

	VCROSS( normal, new_dir1, new_dir2 );

	VCROSS( n1, normal, new_dir1 );
	VUNITIZE( n1 );

	VCROSS( n2, new_dir2, normal );
	VUNITIZE( n2 );

	alpha = bend_radius*(VDOT( n2, new_dir1 ) + VDOT( n1, new_dir2 ) )/
		(2.0 * (1.0 - VDOT( new_dir1, new_dir2 ) ) );
	VJOIN2( new_bend->ps_bendcenter, pt, alpha, new_dir1, bend_radius, n1 );
	VJOIN1( new_bend->ps_start, pt, alpha, new_dir1 );

	VJOIN1( new_linear->ps_start, pt, alpha, new_dir2 );

	RT_LIST_APPEND( &ps->l, &new_bend->l );
	RT_LIST_APPEND( &new_bend->l, &new_linear->l );

	return;
}

struct wdb_pipeseg *
find_pipeseg_nearest_pt( pipe_hd, pt )
CONST struct rt_list *pipe_hd;
CONST point_t pt;
{
	struct wdb_pipeseg *ps;
	struct wdb_pipeseg *nearest=(struct wdb_pipeseg *)NULL;
	struct rt_tol tmp_tol;
	fastf_t min_dist = MAX_FASTF;
	vect_t dir,work;

	tmp_tol.magic = RT_TOL_MAGIC;
	tmp_tol.dist = 0.0;
	tmp_tol.dist_sq = tmp_tol.dist * tmp_tol.dist;
	tmp_tol.perp = 0.0;
	tmp_tol.para = 1 - tmp_tol.perp;

	/* get a direction vector in model space corresponding to z-direction in view */
	VSET( work, 0, 0, 1 )
	MAT4X3VEC( dir, view2model, work )
	VUNITIZE( dir );

	for( RT_LIST_FOR( ps, wdb_pipeseg, pipe_hd ) )
	{
		struct wdb_pipeseg *next;
		point_t pca;
		fastf_t dist[2];
		fastf_t dist_sq;
		int code;

		if( ps->ps_type == WDB_PIPESEG_TYPE_END )
			break;

		next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );

		if( ps->ps_type == WDB_PIPESEG_TYPE_LINEAR )
		{
			code = rt_dist_line3_lseg3( dist, pt, dir, ps->ps_start, next->ps_start, &tmp_tol );

			if( code == 0 )
				dist_sq = 0.0;
			else
			{
				point_t p1,p2;
				vect_t seg_vec;
				vect_t diff;

				VJOIN1( p1, pt, dist[0], dir )
				if( dist[1] < 0.0 )
					VMOVE( p2, ps->ps_start )
				else if( dist[1] > 1.0 )
					VMOVE( p2, next->ps_start )
				else
				{
					VSUB2( seg_vec, next->ps_start, ps->ps_start )
					VJOIN1( p2, ps->ps_start, dist[1], seg_vec )
				}
				VSUB2( diff, p1, p2 )
				dist_sq = MAGSQ( diff );
			}

			if( dist_sq < min_dist )
			{
				min_dist = dist_sq;
				nearest = ps;
			}
		}
		else if( ps->ps_type == WDB_PIPESEG_TYPE_BEND )
		{
			vect_t to_start;
			vect_t to_end;
			vect_t norm;
			vect_t v1,v2;
			fastf_t delta_angle;
			fastf_t cos_del,sin_del;
			fastf_t x,y,x_new,y_new;
			fastf_t radius;
			point_t pt1,pt2;
			int i;

			VSUB2( to_start, ps->ps_start, ps->ps_bendcenter );
			VSUB2( to_end, next->ps_start, ps->ps_bendcenter );
			VCROSS( norm, to_start, to_end );
			VMOVE( v1, to_start );
			VUNITIZE( v1 );
			VCROSS( v2, norm, v1 );
			VUNITIZE( v2 );
			delta_angle = atan2( VDOT( to_end, v2 ), VDOT( to_end, v1 ) )/5.0;
			cos_del = cos( delta_angle );
			sin_del = sin( delta_angle );
			radius = MAGNITUDE( to_start );

			x = radius;
			y = 0.0;
			VJOIN2( pt1, ps->ps_bendcenter, x, v1, y, v2 );
			for( i=0 ; i<5 ; i++ )
			{
				x_new = x*cos_del - y*sin_del;
				y_new = x*sin_del + y*cos_del;
				VJOIN2( pt2, ps->ps_bendcenter, x_new, v1, y_new, v2 );
				x = x_new;
				y = y_new;

				code = rt_dist_line3_lseg3( dist, pt, dir, pt1, pt2, &tmp_tol );

				if( code == 0 )
					dist_sq = 0.0;
				else
				{
					point_t p1,p2;
					vect_t seg_vec;
					vect_t diff;

					VJOIN1( p1, pt, dist[0], dir )
					if( dist[1] < 0.0 )
						VMOVE( p2, pt1 )
					else if( dist[1] > 1.0 )
						VMOVE( p2, pt2 )
					else
					{
						VSUB2( seg_vec, pt2, pt1 )
						VJOIN1( p2, pt1, dist[1], seg_vec )
					}
					VSUB2( diff, p1, p2 )
					dist_sq = MAGSQ( diff );
				}

				if( dist_sq < min_dist )
				{
					min_dist = dist_sq;
					nearest = ps;
				}

				VMOVE( pt1, pt2 );
			}
		}
	}
	return( nearest );
}

