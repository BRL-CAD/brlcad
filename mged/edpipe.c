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
split_pipeseg( pipe_hd, ps, pt )
struct rt_list *pipe_hd;
struct wdb_pipeseg *ps;
point_t pt;
{
	struct wdb_pipeseg *new_linear;
	struct wdb_pipeseg *new_bend;
	struct wdb_pipeseg *next;
	struct wdb_pipeseg *prev;
	vect_t v1,v2;
	vect_t n1,n2;
	vect_t normal;
	vect_t new_dir1,new_dir2;
	fastf_t bend_radius;

	RT_CK_LIST_HEAD( pipe_hd );
	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	if( ps->ps_type != WDB_PIPESEG_TYPE_LINEAR )
	{
		rt_log( "Can only split linear pipe segments\n" );
		return;
	}

	GETSTRUCT( new_linear, wdb_pipeseg );
	new_linear->ps_type = WDB_PIPESEG_TYPE_LINEAR;
	new_linear->l.magic = WDB_PIPESEG_MAGIC;
	new_linear->ps_od = ps->ps_od;
	new_linear->ps_id = ps->ps_id;

	GETSTRUCT( new_bend, wdb_pipeseg );
	new_bend->ps_type = WDB_PIPESEG_TYPE_BEND ;
	new_bend->l.magic = WDB_PIPESEG_MAGIC;
	new_bend->ps_od = ps->ps_od;
	new_bend->ps_id = ps->ps_id;

	next = RT_LIST_NEXT( wdb_pipeseg, &ps->l );
	VSUB2( new_dir1, ps->ps_start, pt );
	VUNITIZE( new_dir1 );
	VSUB2( new_dir2, next->ps_start, pt );
	VUNITIZE( new_dir2 );
	VCROSS( normal, new_dir1, new_dir2 );

	prev = RT_LIST_PREV( wdb_pipeseg, &ps->l );
	if( RT_LIST_IS_HEAD( &prev->l, pipe_hd ) && next->ps_type == WDB_PIPESEG_TYPE_END )
	{
		fastf_t alpha;

		/* this is the only segment in the current pipe */

		bend_radius = ps->ps_od;
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

		{
			struct wdb_pipeseg *tmp;

			for( RT_LIST_FOR( tmp, wdb_pipeseg, pipe_hd ) )
				rt_pipeseg_print( tmp, (double)1.0 );
		}
	}
	else
		rt_log( "Not implimented yet\n" );
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

