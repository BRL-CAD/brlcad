/*
 *			E D P I P E . C
 *
 * Functions -
 *	split_pipept - split a pipe segment at a given point
 *	find_pipept_nearest_pt - find which segment of a pipe is nearest
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
split_pipept( pipe_hd, ps, new_pt )
struct rt_list *pipe_hd;
struct wdb_pipept *ps;
point_t new_pt;
{
}

void
pipe_scale_od( db_int, scale )
struct rt_db_internal *db_int;
fastf_t scale;
{
	struct wdb_pipept *ps;
	struct rt_pipe_internal *pipe=(struct rt_pipe_internal *)db_int->idb_ptr;

	RT_PIPE_CK_MAGIC( pipe );

	if( scale < 1.0 )
	{
		/* check that this can be done */
		for( RT_LIST_FOR( ps, wdb_pipept, &pipe->pipe_segs_head ) )
		{
			fastf_t tmp_od;

			tmp_od = ps->pp_od*scale;
			if( ps->pp_id > tmp_od )
			{
				rt_log( "Cannot make OD less than ID\n" );
				return;
			}
			if( tmp_od > 2.0*ps->pp_bendradius )
			{
				rt_log( "Cannot make outer radius greater than bend radius\n" );
				return;
			}
		}
	}

	for( RT_LIST_FOR( ps, wdb_pipept, &pipe->pipe_segs_head ) )
		ps->pp_od *= scale;
}
void
pipe_scale_id( db_int, scale )
struct rt_db_internal *db_int;
fastf_t scale;
{
	struct wdb_pipept *ps;
	struct rt_pipe_internal *pipe=(struct rt_pipe_internal *)db_int->idb_ptr;
	fastf_t tmp_id;

	RT_PIPE_CK_MAGIC( pipe );

	if( scale > 1.0 || scale < 0.0 )
	{
		/* check that this can be done */
		for( RT_LIST_FOR( ps, wdb_pipept, &pipe->pipe_segs_head ) )
		{
			if( scale > 0.0 )
				tmp_id = ps->pp_id*scale;
			else
				tmp_id = (-scale);
			if( ps->pp_od < tmp_id )
			{
				rt_log( "Cannot make ID greater than OD\n" );
				return;
			}
			if( tmp_id > 2.0*ps->pp_bendradius )
			{
				rt_log( "Cannot make inner radius greater than bend radius\n" );
				return;
			}
		}
	}

	for( RT_LIST_FOR( ps, wdb_pipept, &pipe->pipe_segs_head ) )
	{
		if( scale > 0.0 )
			ps->pp_id *= scale;
		else
			ps->pp_id = (-scale);
	}
}

void
pipe_seg_scale_od( ps, scale )
struct wdb_pipept *ps;
fastf_t scale;
{
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
			tmp_od = scale*ps->pp_od;
		if( ps->pp_id > tmp_od )
		{
			rt_log( "Cannot make OD smaller than ID\n" );
			return;
		}
		if( tmp_od > 2.0*ps->pp_bendradius )
		{
			rt_log( "Cannot make outer radius greater than bend radius\n" );
			return;
		}
	}

	if( scale > 0.0 )
		ps->pp_od *= scale;
	else
		ps->pp_od = (-scale);
}
void
pipe_seg_scale_id( ps, scale )
struct wdb_pipept *ps;
fastf_t scale;
{
	fastf_t tmp_id;

	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	/* make sure we can make this change */
	if( scale > 1.0 || scale < 0.0 )
	{
		/* need to check that the new ID is not greater than OD */
		if( scale > 0.0 )
			tmp_id = scale*ps->pp_id;
		else
			tmp_id = (-scale);
		if( ps->pp_od < tmp_id )
		{
			rt_log( "Cannot make ID greater than OD\n" );
			return;
		}
		if( tmp_id > 2.0*ps->pp_bendradius )
		{
			rt_log( "Cannot make inner radius greater than bend radius\n" );
			return;
		}
	}

	if( scale > 0.0 )
		ps->pp_id *= scale;
	else
		ps->pp_id = (-scale);
}

struct wdb_pipept *
find_pipept_nearest_pt( pipe_hd, pt )
CONST struct rt_list *pipe_hd;
CONST point_t pt;
{
	struct wdb_pipept *ps;
	struct wdb_pipept *nearest=(struct wdb_pipept *)NULL;
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

	for( RT_LIST_FOR( ps, wdb_pipept, pipe_hd ) )
	{
		fastf_t dist;

		dist = rt_dist_line3_pt3( pt, dir, ps->pp_coord );
		if( dist < min_dist )
		{
			min_dist = dist;
			nearest = ps;
		}
	}
	return( nearest );
}

void
add_pipept( pipe, pp, new_pt )
struct rt_pipe_internal *pipe;
struct wdb_pipept *pp;
CONST point_t new_pt;
{
	struct wdb_pipept *last;
	struct wdb_pipept *new;

	RT_PIPE_CK_MAGIC( pipe );
	if( pp )
		RT_CKMAG( pp, WDB_PIPESEG_MAGIC, "pipe point" )

	if( pp )
		last = pp;
	else
	{
		/* add new point to end of pipe solid */
		last = RT_LIST_LAST( wdb_pipept, &pipe->pipe_segs_head );
		if( last->l.magic == RT_LIST_HEAD_MAGIC )
		{
			GETSTRUCT( new, wdb_pipept );
			new->l.magic = WDB_PIPESEG_MAGIC;
			new->pp_od = 30.0;
			new->pp_id = 0.0;
			new->pp_bendradius = 40.0;
			VMOVE( new->pp_coord, new_pt );
			RT_LIST_INSERT( &pipe->pipe_segs_head, &new->l );
			return;
		}
	}

	/* build new point */
	GETSTRUCT( new, wdb_pipept );
	new->l.magic = WDB_PIPESEG_MAGIC;
	new->pp_od = last->pp_od;
	new->pp_id = last->pp_id;
	new->pp_bendradius = last->pp_bendradius;
	VMOVE( new->pp_coord, new_pt );

	if( !pp )	/* add to end of pipe solid */
		RT_LIST_INSERT( &pipe->pipe_segs_head, &new->l )
	else		/* append after current point */
		RT_LIST_APPEND( &pp->l, &new->l )

	if( rt_pipe_ck( &pipe->pipe_segs_head ) )
	{
		/* won't work here, so refuse to do it */
		RT_LIST_DEQUEUE( &new->l );
		rt_free( (char *)new, "add_pipept: new " );
	}
}


void
ins_pipept( pipe, pp, new_pt )
struct rt_pipe_internal *pipe;
struct wdb_pipept *pp;
CONST point_t new_pt;
{
	struct wdb_pipept *first;
	struct wdb_pipept *new;

	RT_PIPE_CK_MAGIC( pipe );
	if( pp )
		RT_CKMAG( pp, WDB_PIPESEG_MAGIC, "pipe point" )

	if( pp )
		first = pp;
	else
	{
		/* insert new point at start of pipe solid */
		first = RT_LIST_FIRST( wdb_pipept, &pipe->pipe_segs_head );
		if( first->l.magic == RT_LIST_HEAD_MAGIC )
		{
			GETSTRUCT( new, wdb_pipept );
			new->l.magic = WDB_PIPESEG_MAGIC;
			new->pp_od = 30.0;
			new->pp_id = 0.0;
			new->pp_bendradius = 40.0;
			VMOVE( new->pp_coord, new_pt );
			RT_LIST_APPEND( &pipe->pipe_segs_head, &new->l );
			return;
		}
	}

	/* build new point */
	GETSTRUCT( new, wdb_pipept );
	new->l.magic = WDB_PIPESEG_MAGIC;
	new->pp_od = first->pp_od;
	new->pp_id = first->pp_id;
	new->pp_bendradius = first->pp_bendradius;
	VMOVE( new->pp_coord, new_pt );

	if( !pp )	/* add to start of pipe */
		RT_LIST_APPEND( &pipe->pipe_segs_head, &new->l )
	else		/* insert before current point */
		RT_LIST_INSERT( &pp->l, &new->l )

	if( rt_pipe_ck( &pipe->pipe_segs_head ) )
	{
		/* won't work here, so refuse to do it */
		RT_LIST_DEQUEUE( &new->l );
		rt_free( (char *)new, "ins_pipept: new " );
	}
}

struct wdb_pipept *
del_pipept( ps )
struct wdb_pipept *ps;
{
	struct wdb_pipept *next;
	struct wdb_pipept *prev;
	struct wdb_pipept *head;

	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	head = ps;
	while( head->l.magic != RT_LIST_HEAD_MAGIC )
		head = RT_LIST_NEXT( wdb_pipept, &head->l );

	next = RT_LIST_NEXT( wdb_pipept, &ps->l );
	if( next->l.magic == RT_LIST_HEAD_MAGIC )
		next = (struct wdb_pipept *)NULL;

	prev = RT_LIST_PREV( wdb_pipept, &ps->l );
	if( prev->l.magic == RT_LIST_HEAD_MAGIC )
		prev = (struct wdb_pipept *)NULL;

	if( !prev && !next )
	{
		rt_log( "Cannot delete last point in pipe\n" );
		return( ps );
	}

	RT_LIST_DEQUEUE( &ps->l );

	if( rt_pipe_ck( head ) )
	{
		rt_log( "Cannot delete this point, it will result in an illegal pipe\n" );
		if( next )
			RT_LIST_INSERT( &next->l, &ps->l )
		else if( prev )
			RT_LIST_APPEND( &prev->l, &ps->l )
		else
			RT_LIST_INSERT( &head->l, &ps->l )

		return( ps );
	}
	else
		rt_free( (char *)ps, "del_pipept: ps" );

	if( prev )
		return( prev );
	else
		return( next );

}

void
move_pipept( pipe, ps, new_pt )
struct rt_pipe_internal *pipe;
struct wdb_pipept *ps;
point_t new_pt;
{
	point_t old_pt;

	RT_PIPE_CK_MAGIC( pipe );
	RT_CKMAG( ps, WDB_PIPESEG_MAGIC, "pipe segment" );

	VMOVE( old_pt, ps->pp_coord );

	VMOVE( ps->pp_coord, new_pt );
	if( rt_pipe_ck( &pipe->pipe_segs_head ) )
	{
		rt_log( "Cannot move point there\n" );
		VMOVE( ps->pp_coord, old_pt );
	}
}
