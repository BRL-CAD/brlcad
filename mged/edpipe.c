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
pipe_scale_od( db_int, scale )
struct rt_db_internal *db_int;
fastf_t scale;
{
	struct wdb_pipeseg *ps;
	struct rt_pipe_internal *pipe=(struct rt_pipe_internal *)db_int->idb_ptr;

	RT_PIPE_CK_MAGIC( pipe );

	if( scale < 1.0 )
	{
		/* check that this can be done */
		for( RT_LIST_FOR( ps, wdb_pipeseg, &pipe->pipe_segs_head ) )
		{
			if( ps->ps_id > ps->ps_od*scale )
			{
				rt_log( "Cannot make OD less than ID\n" );
				return;
			}
		}
	}

	for( RT_LIST_FOR( ps, wdb_pipeseg, &pipe->pipe_segs_head ) )
		ps->ps_od *= scale;
}
void
pipe_scale_id( db_int, scale )
struct rt_db_internal *db_int;
fastf_t scale;
{
	struct wdb_pipeseg *ps;
	struct rt_pipe_internal *pipe=(struct rt_pipe_internal *)db_int->idb_ptr;
	fastf_t tmp_id;

	RT_PIPE_CK_MAGIC( pipe );

	if( scale > 1.0 || scale < 0.0 )
	{
		/* check that this can be done */
		for( RT_LIST_FOR( ps, wdb_pipeseg, &pipe->pipe_segs_head ) )
		{
			if( scale > 0.0 )
				tmp_id = ps->ps_id*scale;
			else
				tmp_id = (-scale);
			if( ps->ps_od < tmp_id )
			{
				rt_log( "Cannot make ID greater than OD\n" );
				return;
			}
		}
	}

	for( RT_LIST_FOR( ps, wdb_pipeseg, &pipe->pipe_segs_head ) )
	{
		if( scale > 0.0 )
			ps->ps_id *= scale;
		else
			ps->ps_id = (-scale);
	}
}

void
pipe_seg_scale_od( ps, scale )
struct wdb_pipeseg *ps;
fastf_t scale;
{
	struct wdb_pipeseg *prev;
	struct wdb_pipeseg *next;
	int seg_count=0;
	int id_eq_od=0;
	fastf_t tmp_od;

	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	/* make sure we can make this change */
	if( scale < 1.0 )
	{
		/* need to check that the new OD is not less than ID
		 * of any affected segment.
		 */
		if( scale < 0.0 )
			tmp_od = (-scale);
		else
			tmp_od = scale*ps->ps_od;
		if( ps->ps_id > tmp_od )
		{
			rt_log( "Cannot make OD smaller than ID\n" );
			return;
		}
		prev = RT_LIST_PREV( wdb_pipeseg, &ps->l );
		while( prev->l.magic != RT_LIST_HEAD_MAGIC &&
			 prev->ps_type == WDB_PIPESEG_TYPE_BEND )
		{
			if( scale < 0.0 )
				tmp_od = (-scale);
			else
				tmp_od = scale*prev->ps_od;
			if( prev->ps_id > tmp_od )
			{
				rt_log( "Cannot make OD smaller than ID\n" );
				return;
			}
			seg_count++;
			if( prev->ps_id == tmp_od )
				id_eq_od++;
			prev = RT_LIST_PREV( wdb_pipeseg, &prev->l );
		}
		if( ps->ps_type == WDB_PIPESEG_TYPE_BEND )
		{
			next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
			while( next->ps_type == WDB_PIPESEG_TYPE_BEND )
			{
				if( scale < 0.0 )
					tmp_od = (-scale);
				else
					tmp_od = scale*next->ps_od;
				if( next->ps_id > tmp_od )
				{
					rt_log( "Cannot make OD smaller than ID\n" );
					return;
				}
				seg_count++;
				if( next->ps_id == tmp_od )
					id_eq_od++;
				next = RT_LIST_NEXT( wdb_pipeseg, &next->l );
			}
			if( scale < 0.0 )
				tmp_od = (-scale);
			else
				tmp_od = scale*next->ps_od;
			if( next->ps_id > tmp_od )
			{
				rt_log( "Cannot make OD smaller than ID\n" );
				return;
			}
			seg_count++;
			if( next->ps_id == tmp_od )
				id_eq_od++;
		}
		if( seg_count && id_eq_od == seg_count )
		{
			rt_log( "Cannot make zero wall thickness pipe\n" );
			return;
		}
	}

	if( scale > 0.0 )
		ps->ps_od *= scale;
	else
		ps->ps_od = (-scale);
	prev = RT_LIST_PREV( wdb_pipeseg, &ps->l );
	while( prev->l.magic != RT_LIST_HEAD_MAGIC &&
		 prev->ps_type == WDB_PIPESEG_TYPE_BEND )
	{
		if( scale > 0.0 )
			prev->ps_od *= scale;
		else
			prev->ps_od = (-scale);
		prev = RT_LIST_PREV( wdb_pipeseg, &prev->l );
	}

	if( ps->ps_type == WDB_PIPESEG_TYPE_BEND )
	{
		next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
		while( next->ps_type == WDB_PIPESEG_TYPE_BEND )
		{
			if( scale > 0.0 )
				next->ps_od *= scale;
			else
				next->ps_od = (-scale);
			next = RT_LIST_PNEXT_CIRC( wdb_pipeseg, &next->l );
		}
		if( scale > 0.0 )
			next->ps_od *= scale;
		else
			next->ps_od = (-scale);
	}
}
void
pipe_seg_scale_id( ps, scale )
struct wdb_pipeseg *ps;
fastf_t scale;
{
	struct wdb_pipeseg *prev;
	struct wdb_pipeseg *next;
	int seg_count=0;
	int id_eq_od=0;
	fastf_t tmp_id;

	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	/* make sure we can make this change */
	if( scale > 1.0 || scale < 0.0 )
	{
		/* need to check that the new ID is not greater than OD
		 * of any affected segment.
		 */
		if( scale > 0.0 )
			tmp_id = scale*ps->ps_id;
		else
			tmp_id = (-scale);
		if( ps->ps_od < tmp_id )
		{
			rt_log( "Cannot make ID greater than OD\n" );
			return;
		}
		prev = RT_LIST_PREV( wdb_pipeseg, &ps->l );
		while( prev->l.magic != RT_LIST_HEAD_MAGIC &&
			 prev->ps_type == WDB_PIPESEG_TYPE_BEND )
		{
			if( scale > 0.0 )
				tmp_id = scale*prev->ps_id;
			else
				tmp_id = (-scale);
			if( prev->ps_od < tmp_id )
			{
				rt_log( "Cannot make ID greater than OD\n" );
				return;
			}
			seg_count++;
			if( prev->ps_od == tmp_id )
				id_eq_od++;
			prev = RT_LIST_PREV( wdb_pipeseg, &prev->l );
		}
		if( ps->ps_type == WDB_PIPESEG_TYPE_BEND )
		{
			next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
			while( next->ps_type == WDB_PIPESEG_TYPE_BEND )
			{
				if( scale > 0.0 )
					tmp_id = scale*next->ps_id;
				else
					tmp_id = (-scale);
				if( next->ps_od < tmp_id )
				{
					rt_log( "Cannot make ID greater than OD\n" );
					return;
				}
				seg_count++;
				if( next->ps_od == tmp_id )
					id_eq_od++;
				next = RT_LIST_NEXT( wdb_pipeseg, &next->l );
			}
			if( scale > 0.0 )
				tmp_id = scale*next->ps_id;
			else
				tmp_id = (-scale);
			if( next->ps_od < tmp_id )
			{
				rt_log( "Cannot make ID greater than OD\n" );
				return;
			}
			seg_count++;
			if( next->ps_od == tmp_id )
				id_eq_od++;
		}
		if( seg_count && id_eq_od == seg_count )
		{
			rt_log( "Cannot make zero wall thickness pipe\n" );
			return;
		}
	}

	if( scale > 0.0 )
		ps->ps_id *= scale;
	else
		ps->ps_id = (-scale);
	prev = RT_LIST_PREV( wdb_pipeseg, &ps->l );
	while( prev->l.magic != RT_LIST_HEAD_MAGIC &&
		 prev->ps_type == WDB_PIPESEG_TYPE_BEND )
	{
		if( scale > 0.0 )
			prev->ps_id *= scale;
		else
			prev->ps_od = (-scale);
		prev = RT_LIST_PREV( wdb_pipeseg, &prev->l );
	}

	if( ps->ps_type == WDB_PIPESEG_TYPE_BEND )
	{
		next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
		while( next->ps_type == WDB_PIPESEG_TYPE_BEND )
		{
			if( scale > 0.0 )
				next->ps_id *= scale;
			else
				next->ps_id = (-scale);
			next = RT_LIST_PNEXT_CIRC( wdb_pipeseg, &next->l );
		}
		if( scale > 0.0 )
			next->ps_id *= scale;
		else
			next->ps_id = (-scale);
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
make_bend_lt_180( ps, normal )
struct wdb_pipeseg *ps;
vect_t normal;	/* unit normal */
{
	struct wdb_pipeseg *next;
	vect_t to_start;
	vect_t to_end;
	vect_t v2;
	fastf_t angle;

	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	if( ps->ps_type != WDB_PIPESEG_TYPE_BEND )
		rt_bomb( "make_bend_lt_180 called with non-bend pipe segment\n" );

	next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );

	VSUB2( to_start, ps->ps_start, ps->ps_bendcenter );
	VSUB2( to_end, next->ps_start, ps->ps_bendcenter );
	VCROSS( v2, normal, to_start );

	angle = atan2( VDOT( to_end, v2 ), VDOT( to_end, to_start ) );
	if( angle < 0.0 )
		angle += 2.0*rt_pi;

	if( angle > rt_pi-RT_DOT_TOL )
	{
		VUNITIZE( to_start );
		VUNITIZE( v2 );
		break_bend( ps, to_start, v2, angle/2.0 );
	}
}

static void
get_bend_start_line( ps, pt, dir )
CONST struct wdb_pipeseg *ps;
point_t pt;
vect_t dir;
{
	struct wdb_pipeseg *next;
	struct wdb_pipeseg *prev;
	vect_t to_start;
	vect_t to_end;
	vect_t normal;

	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	if( ps->ps_type != WDB_PIPESEG_TYPE_BEND )
		rt_bomb( "get_bend_start_line called woth non-bend pipe segment\n" );

	VMOVE( pt, ps->ps_start );

	prev = RT_LIST_PREV( wdb_pipeseg, &ps->l );
	if( prev->l.magic != RT_LIST_HEAD_MAGIC && prev->ps_type == WDB_PIPESEG_TYPE_LINEAR )
	{
		struct wdb_pipeseg *pprev;

		pprev = RT_LIST_PREV( wdb_pipeseg, &prev->l );
		while( pprev->l.magic != RT_LIST_HEAD_MAGIC && pprev->ps_type == WDB_PIPESEG_TYPE_LINEAR )
		{
			prev = pprev;
			pprev = RT_LIST_PREV( wdb_pipeseg, &pprev->l );
		}
		VSUB2( dir, pt, prev->ps_start )
	}
	else
	{
		next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );

		VSUB2( to_start, ps->ps_start, ps->ps_bendcenter );
		VSUB2( to_end, next->ps_start, ps->ps_bendcenter );
		VCROSS( normal, to_start, to_end );
		VCROSS( dir, normal, to_start );
	}

	if( VNEAR_ZERO( dir, SQRT_SMALL_FASTF ) )
		VSETALL( dir, 0.0 )
	else
		VUNITIZE( dir )
}

static void
get_bend_end_line( ps, pt, dir )
CONST struct wdb_pipeseg *ps;
point_t pt;
vect_t dir;
{
	struct wdb_pipeseg *next;
	vect_t to_start;
	vect_t to_end;
	vect_t normal;

	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	if( ps->ps_type != WDB_PIPESEG_TYPE_BEND )
		rt_bomb( "get_bend_end_line called woth non-bend pipe segment\n" );

	next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
	VMOVE( pt, next->ps_start );

	if( next->ps_type == WDB_PIPESEG_TYPE_LINEAR )
	{
		while( next->ps_type == WDB_PIPESEG_TYPE_LINEAR )
			next = RT_LIST_NEXT( wdb_pipeseg, &next->l );

		VSUB2( dir, pt, next->ps_start );
	}
	else
	{
		VSUB2( to_start, ps->ps_start, ps->ps_bendcenter );
		VSUB2( to_end, next->ps_start, ps->ps_bendcenter );
		VCROSS( normal, to_start, to_end );
		VCROSS( dir, to_end, normal );
	}

	if( VNEAR_ZERO( dir, SQRT_SMALL_FASTF ) )
		VSETALL( dir, 0.0 )
	else
		VUNITIZE( dir )
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
			next = RT_LIST_NEXT( wdb_pipeseg, &next->l );
		}
		if( prev->l.magic != RT_LIST_HEAD_MAGIC )
		{
			if( prev->ps_type == WDB_PIPESEG_TYPE_BEND )
			{
				VSUB2( to_start, prev->ps_start, prev->ps_bendcenter );
				bend_radius = MAGNITUDE( to_start );
				return( bend_radius );
			}
			prev = RT_LIST_PREV( wdb_pipeseg, &prev->l );
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

void
add_pipeseg( pipe, new_pt )
struct rt_pipe_internal *pipe;
CONST point_t new_pt;
{
	struct wdb_pipeseg *end_ps;
	struct wdb_pipeseg *last_ps;
	struct wdb_pipeseg *new_linear;
	struct wdb_pipeseg *new_bend;
	point_t old_end;
	fastf_t bend_radius;
	vect_t end_dir;
	vect_t tmp_dir;
	vect_t normal;
	vect_t to_start;
	vect_t to_end;
	vect_t d1,d2;
	fastf_t angle;
	fastf_t dist_to_center;
	mat_t mat;

	RT_PIPE_CK_MAGIC( pipe );

	end_ps = RT_LIST_LAST( wdb_pipeseg, &pipe->pipe_segs_head );
	RT_CKMAG( end_ps, WDB_PIPESEG_MAGIC, "pipe segment" );
	
	if( end_ps->ps_type != WDB_PIPESEG_TYPE_END )
	{
		rt_log( "This pipe doesn't have and 'END' segment!!!\n" );
		return;
	}
	VMOVE( old_end, end_ps->ps_start );
	last_ps = RT_LIST_PREV( wdb_pipeseg, &end_ps->l );
	RT_CKMAG( last_ps, WDB_PIPESEG_MAGIC, "pipe segment" );
	if( last_ps->l.magic == RT_LIST_HEAD_MAGIC )
	{
		/* This pipe is just an END segment */
		GETSTRUCT( new_linear, wdb_pipeseg );
		new_linear->ps_type = WDB_PIPESEG_TYPE_LINEAR;
		new_linear->l.magic = WDB_PIPESEG_MAGIC;
		new_linear->ps_od = end_ps->ps_od;
		new_linear->ps_id = end_ps->ps_id;
		VMOVE( new_linear->ps_start, old_end );
		VMOVE( end_ps->ps_start, new_pt );
		RT_LIST_INSERT( &end_ps->l, &new_linear->l );
	}

	VSUB2( tmp_dir, new_pt, old_end );
	dist_to_center = MAGNITUDE( tmp_dir );
	if( dist_to_center < RT_LEN_TOL )	/* nothing new needed */
		return;
	VUNITIZE( tmp_dir );

	bend_radius = get_bend_radius( last_ps );

	if( last_ps->ps_type == WDB_PIPESEG_TYPE_LINEAR )
	{
		VSUB2( end_dir, last_ps->ps_start, old_end );
		VUNITIZE( end_dir );
		if( NEAR_ZERO( VDOT( end_dir, tmp_dir ) + 1.0, RT_DOT_TOL ) )
		{
			/* new point is in same direction as last segment, so just strech it */
			return;
		}
	}
	else
	{
		get_bend_start_line( last_ps, old_end, end_dir );
		VREVERSE( end_dir, end_dir );
		VSUB2( tmp_dir, new_pt, old_end );
		VUNITIZE( tmp_dir );
	}

	VMOVE( end_ps->ps_start, new_pt );

	GETSTRUCT( new_linear, wdb_pipeseg );
	new_linear->ps_type = WDB_PIPESEG_TYPE_LINEAR;
	new_linear->l.magic = WDB_PIPESEG_MAGIC;
	new_linear->ps_od = last_ps->ps_od;
	new_linear->ps_id = last_ps->ps_id;

	if( last_ps->ps_type != WDB_PIPESEG_TYPE_BEND )
	{
		GETSTRUCT( new_bend, wdb_pipeseg );
		new_bend->ps_type = WDB_PIPESEG_TYPE_BEND;
		new_bend->l.magic = WDB_PIPESEG_MAGIC;
		new_bend->ps_od = last_ps->ps_od;
		new_bend->ps_id = last_ps->ps_id;
		VMOVE( new_bend->ps_start, old_end );
	}
	else
		new_bend = last_ps;

	/* get bend center for new bend */
	VCROSS( normal, end_dir, tmp_dir );
	VUNITIZE( normal );
	VCROSS( to_start, normal, end_dir );
	VUNITIZE( to_start );
	VJOIN1( new_bend->ps_bendcenter, old_end, bend_radius, to_start );

	/* calculate start point for new linear segment */
	VSUB2( tmp_dir, new_bend->ps_bendcenter, new_pt );
	angle = asin( bend_radius/MAGNITUDE( tmp_dir ) );
	mat_arb_rot( mat, new_bend->ps_bendcenter, normal, -angle );
	VCROSS( d1, tmp_dir, normal );
	MAT4X3VEC( to_end, mat, d1 );
	VUNITIZE( to_end );
	VJOIN1( new_linear->ps_start, new_bend->ps_bendcenter, bend_radius, to_end );

	if( new_bend != last_ps )
		RT_LIST_APPEND( &last_ps->l, &new_bend->l );
	RT_LIST_APPEND( &new_bend->l, &new_linear->l );

	/* make sure new bend is less than 180 degrees */
	VREVERSE( to_start, to_start );
	VCROSS( d2, to_start, normal );
	angle = atan2( VDOT( to_end, d2 ), VDOT( to_end, to_start) );
	if( angle < 0.0 )
		angle += 2.0*rt_pi;
	if( angle > rt_pi - RT_DOT_TOL )
		break_bend( new_bend, to_start, d2, angle/2.0 );
}


void
ins_pipeseg( pipe, new_pt )
struct rt_pipe_internal *pipe;
CONST point_t new_pt;
{
	struct wdb_pipeseg *start_ps;
	struct wdb_pipeseg *next_ps;
	struct wdb_pipeseg *new_linear;
	struct wdb_pipeseg *new_bend;
	fastf_t dist_to_start;
	vect_t tmp_dir;
	vect_t start_dir;
	vect_t normal;
	vect_t to_start;
	vect_t to_end;
	vect_t d1,d2;
	point_t old_start;
	fastf_t bend_radius;
	fastf_t angle;
	mat_t mat;

	RT_PIPE_CK_MAGIC( pipe );

	start_ps = RT_LIST_FIRST( wdb_pipeseg, &pipe->pipe_segs_head );
	RT_CKMAG( start_ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	if( start_ps->ps_type == WDB_PIPESEG_TYPE_END )
	{
		/* This pipe is just an END segment */
		GETSTRUCT( new_linear, wdb_pipeseg );
		new_linear->ps_type = WDB_PIPESEG_TYPE_LINEAR;
		new_linear->l.magic = WDB_PIPESEG_MAGIC;
		new_linear->ps_od = start_ps->ps_od;
		new_linear->ps_id = start_ps->ps_id;
		VMOVE( new_linear->ps_start, new_pt );
		RT_LIST_INSERT( &start_ps->l, &new_linear->l );
	}

	VSUB2( tmp_dir, new_pt, start_ps->ps_start );
	dist_to_start = MAGNITUDE( tmp_dir );
	if( dist_to_start < RT_LEN_TOL )	/* nothing new needed */
		return;

	bend_radius = get_bend_radius( start_ps );

	if( start_ps->ps_type == WDB_PIPESEG_TYPE_LINEAR )
	{
		VMOVE( old_start, start_ps->ps_start );
		next_ps = RT_LIST_NEXT( wdb_pipeseg, &start_ps->l );
		RT_CKMAG( next_ps, WDB_PIPESEG_MAGIC, "pipe segment" );
		VSUB2( start_dir, next_ps->ps_start, old_start );
		if( NEAR_ZERO( VDOT( start_dir, tmp_dir ) + 1.0, RT_DOT_TOL ) )
		{
			/* new point is in same direction as first segment, so just strech it */
			VMOVE( start_ps->ps_start, new_pt );
			return;
		}
	}
	else
	{
		get_bend_end_line( start_ps, old_start, start_dir );
		VREVERSE( start_dir, start_dir );
		VSUB2( tmp_dir, new_pt, old_start );
		VUNITIZE( tmp_dir );
	}

	GETSTRUCT( new_linear, wdb_pipeseg );
	new_linear->ps_type = WDB_PIPESEG_TYPE_LINEAR;
	new_linear->l.magic = WDB_PIPESEG_MAGIC;
	new_linear->ps_od = start_ps->ps_od;
	new_linear->ps_id = start_ps->ps_id;
	VMOVE( new_linear->ps_start, new_pt );

	if( start_ps->ps_type != WDB_PIPESEG_TYPE_BEND )
	{
		GETSTRUCT( new_bend, wdb_pipeseg );
		new_bend->ps_type = WDB_PIPESEG_TYPE_BEND;
		new_bend->l.magic = WDB_PIPESEG_MAGIC;
		new_bend->ps_od = start_ps->ps_od;
		new_bend->ps_id = start_ps->ps_id;
	}
	else
		new_bend = start_ps;

	/* get bend center for new bend */
	VCROSS( normal, start_dir, tmp_dir );
	VUNITIZE( normal );
	VCROSS( to_start, normal, start_dir );
	VUNITIZE( to_start );
	VJOIN1( new_bend->ps_bendcenter, old_start, bend_radius, to_start );

	/* calculate end point for new linear segment */
	VSUB2( tmp_dir, new_bend->ps_bendcenter, new_pt );
	angle = asin( bend_radius/MAGNITUDE( tmp_dir ) );
	mat_arb_rot( mat, new_bend->ps_bendcenter, normal, -angle );
	VCROSS( d1, tmp_dir, normal );
	MAT4X3VEC( to_end, mat, d1 );
	VUNITIZE( to_end );
	VJOIN1( new_bend->ps_start, new_bend->ps_bendcenter, bend_radius, to_end );

	if( new_bend != start_ps )
		RT_LIST_INSERT( &start_ps->l, &new_bend->l );
	RT_LIST_INSERT( &new_bend->l, &new_linear->l );

	/* make sure new bend is less than 180 degrees */
	VREVERSE( to_start, to_start );
	VCROSS( d2, to_start, normal );
	angle = atan2( VDOT( to_end, d2 ), VDOT( to_end, to_start) );
	if( angle < 0.0 )
		angle += 2.0*rt_pi;
	if( angle > rt_pi - RT_DOT_TOL )
		break_bend( new_bend, to_start, d2, angle/2.0 );
}

struct wdb_pipeseg *
del_pipeseg( ps )
struct wdb_pipeseg *ps;
{
	struct wdb_pipeseg *next;
	struct wdb_pipeseg *prev;
	struct wdb_pipeseg *pprev;
	struct wdb_pipeseg *bend1;
	struct wdb_pipeseg *bend2;
	struct wdb_pipeseg *linear;
	vect_t trans;
	point_t start,end;
	vect_t dir1,dir2,dir3,dir4;
	vect_t normal1,normal2;
	vect_t to_center;
	vect_t to_start,to_end,v2;
	vect_t cent_to_cent;
	vect_t d2,d2_rot;
	vect_t d1,d1_rot;
	fastf_t bend_radius1,bend_radius2;
	fastf_t angle;
	mat_t mat;

	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	if( ps->ps_type == WDB_PIPESEG_TYPE_END )
	{
		rt_log( "Cannot delete END segment\n" );
		return( ps );
	}

	prev = RT_LIST_PREV( wdb_pipeseg, &ps->l );
	if( prev->l.magic == RT_LIST_HEAD_MAGIC )
		prev = (struct wdb_pipeseg *)NULL;

	next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
	RT_CKMAG( next, WDB_PIPESEG_MAGIC, "pipe segment" );

	/* Check if this is the only segment */
	if( next->ps_type == WDB_PIPESEG_TYPE_END && !prev )
	{
		rt_log( "Cannot delete only segment in pipe\n" );
		return( ps );
	}

	/* Check if this is the last segment */
	if( next->ps_type == WDB_PIPESEG_TYPE_END )
	{
		/* just delete the last segment */
		VMOVE( next->ps_start, ps->ps_start );
		next->ps_od = ps->ps_od;
		next->ps_id = ps->ps_id;
		RT_LIST_DEQUEUE( &ps->l );
		rt_free( (char *)ps, "del_pipeseg: ps" );
		if( prev )
			return( prev );
		else
			return( next );
	}

	/* Check if this is the first segment */
	if( !prev )
	{
		/* just delete the first segment */
		RT_LIST_DEQUEUE( &ps->l );
		rt_free( (char *)ps, "del_pipeseg: ps" );
		return( next );
	}

	/* selected segment has segments before and after it */
	if( ps->ps_type == WDB_PIPESEG_TYPE_BEND )
	{
		rt_log( "Cannot delete BEND segments in the middle of a pipe\n" );
		return( ps );
	}

	/* if selected segment is one of two consecutive linear segments,
	 * just remove one and stretch the remaining one
	 */
	if( next->ps_type == WDB_PIPESEG_TYPE_LINEAR )
	{
		if( prev->ps_type == WDB_PIPESEG_TYPE_BEND )
		{
			next->ps_od = prev->ps_od;
			next->ps_id = prev->ps_id;
			VMOVE( next->ps_start, ps->ps_start );
		}

		RT_LIST_DEQUEUE( &ps->l );
		rt_free( (char *)ps, "del_pipeseg: ps" );
		return( next );
	}

	if( prev->ps_type == WDB_PIPESEG_TYPE_LINEAR )
	{
		RT_LIST_DEQUEUE( &ps->l );
		rt_free( (char *)ps, "del_pipeseg: ps" );
		return( prev );
	}

	/* If we reach here, selected segment is linear with bends at both ends */

	if( next->ps_type != WDB_PIPESEG_TYPE_BEND || prev->ps_type != WDB_PIPESEG_TYPE_BEND )
	{
		rt_log( "Error in del_pipeseg, linear segment not surrounded by bends!!!\n" );
		return( ps );
	}

	/* Get translation vector for moving bend segments */
	VSUB2( trans, next->ps_start, ps->ps_start );
	RT_LIST_DEQUEUE( &ps->l );
	rt_free( (char *)ps, "del_pipeseg: ps" );

	/* move all bend segments prior to "ps" by translation vector */
	while( prev->l.magic != RT_LIST_HEAD_MAGIC && prev->ps_type == WDB_PIPESEG_TYPE_BEND )
	{
		VADD2( prev->ps_start, prev->ps_start, trans );
		VADD2( prev->ps_bendcenter, prev->ps_bendcenter, trans );
		prev = RT_LIST_PREV( wdb_pipeseg, &prev->l );
	}

	/* if no linear segments, we are done */
	if( prev->l.magic == RT_LIST_HEAD_MAGIC )
		return( next );

	if( prev->ps_type != WDB_PIPESEG_TYPE_LINEAR )
		rt_bomb( "del_pipeseg: Expecting a linear segment, didn't find one!!!\n" );

	linear = prev;
	bend2 = RT_LIST_NEXT( wdb_pipeseg, &linear->l );
	bend1 = RT_LIST_PREV( wdb_pipeseg, &linear->l );
	if( bend1->l.magic == RT_LIST_HEAD_MAGIC )
		bend1 = (struct wdb_pipeseg *)NULL;
	next = RT_LIST_NEXT( wdb_pipeseg, &bend2->l );

	/* bend1, linear, and bend2 must be adjusted to mate correctly */

	if( bend1 )
	{
		/* calculate new bend center for bend1 */
		get_bend_start_line( bend1, start, dir1 );
		bend_radius1 = get_bend_radius( bend1 );

		/* get line direction away from bend */
		VREVERSE( dir1, dir1 );

		VSUB2( dir2, next->ps_start, start );
		VCROSS( normal1, dir1, dir2 );

		VCROSS( to_center, normal1, dir1 );
		VUNITIZE( to_center );
		VJOIN1( bend1->ps_bendcenter, start, bend_radius1, to_center );
	}
	else
		VSUB2( dir2, next->ps_start, linear->ps_start )

	/* calculate new bend center for bend2 */
	get_bend_end_line( bend2, end, dir3 );
	bend_radius2 = get_bend_radius( bend2 );

	VREVERSE( dir3, dir3 );
	VREVERSE( dir4, dir2 );
	VCROSS( normal2, dir4, dir3 );
	VCROSS( to_center, dir3, normal2 );
	VUNITIZE( to_center );
	VJOIN1( bend2->ps_bendcenter, next->ps_start, bend_radius2, to_center );

	/* calculate angle off normal of center-to-center line for bend/linear junction */
	if( bend1 )
	{
		VSUB2( cent_to_cent, bend2->ps_bendcenter, bend1->ps_bendcenter );
		VCROSS( d1, normal1, cent_to_cent );
	}
	else
		VSUB2( cent_to_cent, bend2->ps_bendcenter, linear->ps_start )

	VCROSS( d2, normal2, cent_to_cent );

	if( bend1 )
	{
		if( VDOT( d1, d2 ) > 0.0 )
			angle = asin( (bend_radius1 - bend_radius2)/MAGNITUDE( cent_to_cent ) );
		else
			angle = asin( (bend_radius1 + bend_radius2)/MAGNITUDE( cent_to_cent ) );
	}
	else
		angle = asin( bend_radius2/MAGNITUDE( cent_to_cent ) );

	/* calulate new end points for linear segment */
	if( NEAR_ZERO( angle, RT_DOT_TOL ) )
	{
		if( bend1 )
		{
			VUNITIZE( d1 );
			VJOIN1( linear->ps_start, bend1->ps_bendcenter, bend_radius1, d1 );
		}
		VUNITIZE( d2 );
		VJOIN1( bend2->ps_start, bend2->ps_bendcenter, bend_radius2, d2 );
		return( linear );
	}

	if( bend1 )
	{
		VUNITIZE( normal1 );
		mat_arb_rot( mat, bend1->ps_bendcenter, normal1, angle );
		MAT4X3VEC( d1_rot, mat, d1 );
		VUNITIZE( d1_rot );
		VJOIN1( linear->ps_start, bend1->ps_bendcenter, bend_radius1, d1_rot );
	}

	VUNITIZE( normal2 );
	mat_arb_rot( mat, bend2->ps_bendcenter, normal2, angle );
	MAT4X3VEC( d2_rot, mat, d2 );
	VUNITIZE( d2_rot );
	VJOIN1( bend2->ps_start, bend2->ps_bendcenter, bend_radius2, d2_rot );

	/* insure that bends are less than 180 degrees */
	if( bend1 )
	{
		VSUB2( to_start, bend1->ps_start, bend1->ps_bendcenter );
		VUNITIZE( to_start );
		VSUB2( to_end, linear->ps_start, bend1->ps_bendcenter );
		VCROSS( v2, to_start, normal1 );
		angle = atan2( VDOT( to_end, v2 ), VDOT( to_end, to_start ) );
		if( angle < 0.0 )
			angle += 2.0*rt_pi;
		if( angle > rt_pi - RT_DOT_TOL )
			break_bend( bend1, to_start, v2, angle/2.0 );
	}

	VSUB2( to_start, bend2->ps_start, bend2->ps_bendcenter );
	VUNITIZE( to_start );
	next = RT_LIST_NEXT( wdb_pipeseg, &bend2->l );
	VSUB2( to_end, next->ps_start, bend2->ps_bendcenter );
	VCROSS( v2, to_start, normal2 );
	angle = atan2( VDOT( to_end, v2 ), VDOT( to_end, to_start ) );
	if( angle < 0.0 )
		angle += 2.0*rt_pi;
	if( angle > rt_pi - RT_DOT_TOL )
		break_bend( bend2, to_start, v2, angle/2.0 );

	return( linear );
}

void
move_bend( pipe, ps, new_pt )
struct rt_pipe_internal *pipe;
struct wdb_pipeseg *ps;
point_t new_pt;
{
	struct wdb_pipeseg *next_bend;
	vect_t next_norm;
	struct wdb_pipeseg *prev_bend;
	vect_t prev_norm;
	struct wdb_pipeseg *bend1;
	struct wdb_pipeseg *bend2;
	struct wdb_pipeseg *linear;
	struct wdb_pipeseg *pprev;
	struct wdb_pipeseg *nnext;
	point_t tmp_pt;
	vect_t cent_to_cent;
	vect_t start1,end1,normal1;
	vect_t start2,end2,normal2;
	vect_t d1,d1_rot;
	vect_t d2,d2_rot;
	vect_t linear_dir;
	mat_t mat1_plus,mat1_minus;
	mat_t mat2_plus,mat2_minus;
	fastf_t bend_radius1,bend_radius2;
	fastf_t angle;
	vect_t trans;

	RT_PIPE_CK_MAGIC( pipe );
	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	if( ps->ps_type != WDB_PIPESEG_TYPE_BEND )
	{
		rt_log( "move_bend called with non-bend pipe segment\n" );
		return;
	}

	next_bend = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
	while( next_bend->ps_type == WDB_PIPESEG_TYPE_BEND )
		next_bend = RT_LIST_NEXT( wdb_pipeseg, &next_bend->l );
	while( next_bend->ps_type == WDB_PIPESEG_TYPE_LINEAR )
		next_bend = RT_LIST_NEXT( wdb_pipeseg, &next_bend->l );
	if( next_bend->ps_type != WDB_PIPESEG_TYPE_END )
	{
		get_bend_end_line( next_bend, tmp_pt, end1 );
		get_bend_start_line( next_bend, tmp_pt, start1 );
		VCROSS( next_norm, end1, start1 );
	}

	prev_bend = RT_LIST_PREV( wdb_pipeseg, &ps->l );
	while( prev_bend->l.magic != RT_LIST_HEAD_MAGIC && prev_bend->ps_type == WDB_PIPESEG_TYPE_BEND )
		prev_bend = RT_LIST_PREV( wdb_pipeseg, &prev_bend->l );
	while( prev_bend->l.magic != RT_LIST_HEAD_MAGIC && prev_bend->ps_type == WDB_PIPESEG_TYPE_LINEAR )
		prev_bend = RT_LIST_PREV( wdb_pipeseg, &prev_bend->l );
	if( prev_bend->l.magic != RT_LIST_HEAD_MAGIC )
	{
		get_bend_end_line( prev_bend, tmp_pt, end1 );
		get_bend_start_line( prev_bend, tmp_pt, start1 );
		VCROSS( prev_norm, end1, start1 );
	}

	/* get translation vector for move */
	VSUB2( trans, new_pt, ps->ps_bendcenter );

	/* move this bend */
	VMOVE( ps->ps_bendcenter, new_pt );
	VADD2( ps->ps_start, ps->ps_start, trans );

	/* move all adjacent bends also */
	bend1 = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
	while( bend1->ps_type == WDB_PIPESEG_TYPE_BEND )
	{
		VADD2( bend1->ps_bendcenter, bend1->ps_bendcenter, trans );
		VADD2( bend1->ps_start, bend1->ps_start, trans );
		bend1 = RT_LIST_NEXT( wdb_pipeseg, &bend1->l );
	}
	bend1 = RT_LIST_PREV( wdb_pipeseg, &ps->l );
	while( bend1->l.magic != RT_LIST_HEAD_MAGIC && bend1->ps_type == WDB_PIPESEG_TYPE_BEND )
	{
		VADD2( bend1->ps_bendcenter, bend1->ps_bendcenter, trans );
		VADD2( bend1->ps_start, bend1->ps_start, trans );
		bend1 = RT_LIST_PREV( wdb_pipeseg, &bend1->l );
	}

	/* find next section consisting of possibly a bend, linear section and another bend */
	linear = ps;
	while( linear->ps_type != WDB_PIPESEG_TYPE_LINEAR && linear->ps_type != WDB_PIPESEG_TYPE_END )
		linear = RT_LIST_NEXT( wdb_pipeseg, &linear->l );

	/* if a linear segment is found, an adjustment is necessary */
	if( linear->ps_type == WDB_PIPESEG_TYPE_LINEAR )
	{
		/* found a linear section that must be adjusted */

		/* move the start point of the linear section to get good
		 * end line for bend1 and a good start line for bend2
		 */
		VADD2( linear->ps_start, linear->ps_start, trans );
		bend1 = RT_LIST_PREV( wdb_pipeseg, &linear->l );

		/* look for a bend that follows the linear section */
		bend2 = linear;
		while( bend2->ps_type != WDB_PIPESEG_TYPE_BEND && bend2->ps_type != WDB_PIPESEG_TYPE_END )
			bend2 = RT_LIST_NEXT( wdb_pipeseg, &bend2->l );
		if( bend2->ps_type != WDB_PIPESEG_TYPE_BEND )
			bend2 = (struct wdb_pipeseg *)NULL;

		/* Get center to center lines between bends */
		if( bend2 )
			VSUB2( cent_to_cent, bend2->ps_bendcenter, bend1->ps_bendcenter )
		else
		{
			nnext = RT_LIST_NEXT( wdb_pipeseg, &linear->l );
			while( nnext->ps_type == WDB_PIPESEG_TYPE_LINEAR )
				nnext = RT_LIST_NEXT( wdb_pipeseg, &nnext->l );
			VSUB2( cent_to_cent, nnext->ps_start, bend1->ps_bendcenter )
		}

		/* get normal and vector normal to center-to-center line for bend1 */
		bend_radius1 = get_bend_radius( bend1 );
		get_bend_start_line( bend1, tmp_pt, start1 );
		get_bend_end_line( bend1, tmp_pt, end1 );
		VCROSS( normal1, end1, start1 );
		VUNITIZE( normal1 );
		VCROSS( d1, cent_to_cent, normal1 );

		if( bend2 )
		{
			/* get normal and vector normal to center-to-center line for bend2 */
			bend_radius2 = get_bend_radius( bend2 );
			get_bend_start_line( bend2, tmp_pt, start2 );
			get_bend_end_line( bend2, tmp_pt, end2 );
			VCROSS( normal2, end2, start2 );
			VUNITIZE( normal2 );
			VCROSS( d2, cent_to_cent, normal2 );

			if( VDOT( normal2, next_norm ) < 0.0 )
			{
				struct wdb_pipeseg *next;
				vect_t to_center;

				/* bendcenter needs to be switched */
				next = RT_LIST_NEXT( wdb_pipeseg, &bend2->l );
				VSUB2( to_center, bend2->ps_bendcenter, next->ps_start );
				VSUB2( bend2->ps_bendcenter, next->ps_start, to_center );
			}
		}

		/* calculate angle that d1 and d2 must rotate about bend centers */
		if( bend2 )
		{
			if( VDOT( d1, d2 ) < 0.0 )
				angle = asin( (bend_radius1+bend_radius2)/MAGNITUDE( cent_to_cent ) );
			else
				angle = asin( (bend_radius1-bend_radius2)/MAGNITUDE( cent_to_cent ) );
		}
		else
			angle = asin( bend_radius1/MAGNITUDE( cent_to_cent ) );

		/* rotate d1 */
		mat_arb_rot( mat1_plus, bend1->ps_bendcenter, normal1, angle );
		mat_arb_rot( mat1_minus, bend1->ps_bendcenter, normal1, -angle );
		if( bend2 )
		{
			mat_arb_rot( mat2_plus, bend2->ps_bendcenter, normal2, angle );
			mat_arb_rot( mat2_minus, bend2->ps_bendcenter, normal2, -angle );
		}
		MAT4X3VEC( d1_rot, mat1_plus, d1 );
		VUNITIZE( d1_rot );
		/* calculate new start point for linear segment */
		VJOIN1( linear->ps_start, bend1->ps_bendcenter, bend_radius1, d1_rot );

		if( bend2 )
		{
			/* rotate d2 */
			if( VDOT( d1, d2 ) < 0.0 )
				MAT4X3VEC( d2_rot, mat2_minus, d2 )
			else
				MAT4X3VEC( d2_rot, mat2_plus, d2 )
			VUNITIZE( d2_rot );
			/* calculate new start point for bend2 */
			VJOIN1( bend2->ps_start, bend2->ps_bendcenter, bend_radius2, d2_rot );
		}

		/* adjust any linear segments between "linear" and bend2 */
		if( bend2 )
			VSUB2( linear_dir, bend2->ps_start, linear->ps_start )
		else
		{
			nnext = RT_LIST_NEXT( wdb_pipeseg, &linear->l );
			while( nnext->ps_type == WDB_PIPESEG_TYPE_LINEAR )
				nnext = RT_LIST_NEXT( wdb_pipeseg, &nnext->l );
			VSUB2( linear_dir, nnext->ps_start, linear->ps_start );
		}
		VUNITIZE( linear_dir );
		nnext = RT_LIST_NEXT( wdb_pipeseg, &linear->l );
		while( nnext->ps_type == WDB_PIPESEG_TYPE_LINEAR )
		{
			vect_t old_vec;

			VSUB2( old_vec, nnext->ps_start, linear->ps_start );
			VJOIN1( nnext->ps_start, linear->ps_start, VDOT( linear_dir, old_vec ), linear_dir );
			nnext = RT_LIST_NEXT( wdb_pipeseg, &nnext->l );
		}

		/* make sure bend2 is less than 180 degrees */
		if( bend2 )
			make_bend_lt_180( bend2, normal2 );
	}

	/* search the previous sections (prior to "ps") for a bend, linear, bend arrangement */
	linear = ps;
	while( linear->l.magic != RT_LIST_HEAD_MAGIC && linear->ps_type != WDB_PIPESEG_TYPE_LINEAR )
		linear = RT_LIST_PREV( wdb_pipeseg, &linear->l );

	/* if a linear segment is found, an adjustment is necessary */
	if( linear->ps_type == WDB_PIPESEG_TYPE_LINEAR )
	{
		/* found a linear section that must be adjusted */
		bend2 = RT_LIST_NEXT( wdb_pipeseg, &linear->l );

		/* look for a bend that precedes the linear section */
		bend1 = linear;
		while( bend1->l.magic != RT_LIST_HEAD_MAGIC && bend1->ps_type != WDB_PIPESEG_TYPE_BEND )
			bend1 = RT_LIST_PREV( wdb_pipeseg, &bend1->l );
		if( bend1->l.magic == RT_LIST_HEAD_MAGIC )
			bend1 = (struct wdb_pipeseg *)NULL;

		/* Get center to center lines between bends */
		if( bend1 )
			VSUB2( cent_to_cent, bend2->ps_bendcenter, bend1->ps_bendcenter )
		else
		{
			struct wdb_pipeseg *prev=(struct wdb_pipeseg *)NULL;

			pprev = RT_LIST_PREV( wdb_pipeseg, &linear->l );
			while( pprev->ps_type == WDB_PIPESEG_TYPE_LINEAR )
			{
				prev = pprev;
				pprev = RT_LIST_PREV( wdb_pipeseg, &pprev->l );
			}
			if( prev )
				VSUB2( cent_to_cent, bend2->ps_bendcenter, prev->ps_start )
			else
				VSUB2( cent_to_cent, bend2->ps_bendcenter, linear->ps_start )
		}

		if( bend1 )
		{
			/* get normal and vector normal to center to center line for bend1 */
			bend_radius1 = get_bend_radius( bend1 );
			get_bend_start_line( bend1, tmp_pt, start1 );
			get_bend_end_line( bend1, tmp_pt, end1 );
			VCROSS( normal1, end1, start1 );
			VUNITIZE( normal1 );
			VCROSS( d1, cent_to_cent, normal1 );
		}

		/* get normal and vector normal to center to center line for bend2 */
		bend_radius2 = get_bend_radius( bend2 );
		get_bend_start_line( bend2, tmp_pt, start2 );
		get_bend_end_line( bend2, tmp_pt, end2 );
		VCROSS( normal2, end2, start2 );
		VUNITIZE( normal2 );
		VCROSS( d2, cent_to_cent, normal2 );

		/* calculate angle that d1 and d2 must rotate about bend centers */
		if( bend1 )
		{
			if( VDOT( d1, d2 ) < 0.0 )
				angle = asin( (bend_radius1+bend_radius2)/MAGNITUDE( cent_to_cent ) );
			else
				angle = asin( (bend_radius1-bend_radius2)/MAGNITUDE( cent_to_cent ) );
		}
		else
			angle = asin( bend_radius2/MAGNITUDE( cent_to_cent ) );

		mat_arb_rot( mat2_plus, bend2->ps_bendcenter, normal2, angle );
		mat_arb_rot( mat2_minus, bend2->ps_bendcenter, normal2, -angle );
		if( bend1 )
		{
			mat_arb_rot( mat1_plus, bend1->ps_bendcenter, normal1, angle );
			mat_arb_rot( mat1_minus, bend1->ps_bendcenter, normal1, -angle );
		}
		if( bend1 )
		{
			/* rotate d1 */
			MAT4X3VEC( d1_rot, mat1_plus, d1 );
			VUNITIZE( d1_rot );
			/* calculate new start point for linear segment */
			VJOIN1( linear->ps_start, bend1->ps_bendcenter, bend_radius1, d1_rot );
		}

		/* rotate d2 */
		if( bend1 )
		{
			if( VDOT( d1, d2 ) < 0.0 )
				MAT4X3VEC( d2_rot, mat2_minus, d2 )
			else
				MAT4X3VEC( d2_rot, mat2_plus, d2 )
		}
		else
			MAT4X3VEC( d2_rot, mat2_minus, d2 )

		VUNITIZE( d2_rot );
		/* calculate new start point for bend2 */
		VJOIN1( bend2->ps_start, bend2->ps_bendcenter, bend_radius2, d2_rot );

		/* adjust any linear segments between "linear" and bend2 */
		VSUB2( linear_dir, bend2->ps_start, linear->ps_start )
		VUNITIZE( linear_dir );
		nnext = RT_LIST_NEXT( wdb_pipeseg, &linear->l );
		while( nnext->ps_type == WDB_PIPESEG_TYPE_LINEAR )
		{
			vect_t old_vec;

			VSUB2( old_vec, nnext->ps_start, linear->ps_start );
			VJOIN1( nnext->ps_start, linear->ps_start, VDOT( linear_dir, old_vec ), linear_dir );
			nnext = RT_LIST_NEXT( wdb_pipeseg, &nnext->l );
		}
		/* make sure bend1 is less than 180 degrees */
		if( bend1 )
			make_bend_lt_180( bend1, normal1 );
	}

	/* make sure ps is less than 180 degrees */
	get_bend_start_line( ps, tmp_pt, start2 );
	get_bend_end_line( ps, tmp_pt, end2 );
	VCROSS( normal2, end2, start2 );
	VUNITIZE( normal2 );
	make_bend_lt_180( ps, normal2 );
}

void
move_linear( pipe, ps, new_pt )
struct rt_pipe_internal *pipe;
struct wdb_pipeseg *ps;
point_t new_pt;
{

	RT_PIPE_CK_MAGIC( pipe );
	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );
}

void
move_end( pipe, ps, new_pt )
struct rt_pipe_internal *pipe;
struct wdb_pipeseg *ps;
point_t new_pt;
{

	RT_PIPE_CK_MAGIC( pipe );
	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );
}

void
move_pipeseg( pipe, ps, new_pt )
struct rt_pipe_internal *pipe;
struct wdb_pipeseg *ps;
point_t new_pt;
{

	RT_PIPE_CK_MAGIC( pipe );
	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	switch( ps->ps_type )
	{
		case WDB_PIPESEG_TYPE_BEND:
			move_bend( pipe, ps, new_pt );
			break;
		case WDB_PIPESEG_TYPE_LINEAR:
			move_linear( pipe, ps, new_pt );
			break;
		case WDB_PIPESEG_TYPE_END:
			move_end( pipe, ps, new_pt );
			break;
		default:
			rt_log( "move_pipeseg: Unrecognized PIPE segment type (%d)\n", ps->ps_type );
			break;
	}
}
