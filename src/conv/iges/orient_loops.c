/*                  O R I E N T _ L O O P S . C
 * BRL-CAD
 *
 * Copyright (C) 1994-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file orient_loops.c
 *  Authors -
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "./iges_struct.h"
#include "./iges_extern.h"

struct loop_list
{
	struct loopuse *lu;
	struct loop_list *inner_loops;
	struct loop_list *next;
};

static struct loop_list *loop_root;

void
Find_inner_loops( fu , lptr )
struct faceuse *fu;
struct loop_list *lptr;
{
	struct loop_list *inner;
	struct loopuse *lu;

	/* find all loops contained in lptr->lu */
	for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		if( lu == lptr->lu )
			continue;

		if( nmg_classify_lu_lu( lu , lptr->lu , &tol ) == NMG_CLASS_AinB )
		{

			if( lptr->inner_loops == (struct loop_list *)NULL )
			{
				lptr->inner_loops = (struct loop_list *)bu_malloc( sizeof( struct loop_list ),
					"Find_inner_loops: lptr->inner_loops" );
				inner = lptr->inner_loops;
			}
			else
			{
				inner = lptr->inner_loops;
				while( inner->next != (struct loop_list *)NULL )
					inner = inner->next;
				inner->next = (struct loop_list *)bu_malloc( sizeof( struct loop_list ),
					"Find_inner_loops: inner->next" );
				inner = inner->next;
			}
			inner->next = (struct loop_list *)NULL;
			inner->lu = lu;
			inner->inner_loops = (struct loop_list *)NULL;
		}
	}

	/* now eliminate those inner loops that are contained in other inner loops */
	inner = lptr->inner_loops;
	while( inner )
	{
		struct loop_list *inner1,*prev;
		int deleted;

		deleted = 0;
		inner1 = lptr->inner_loops;
		prev = (struct loop_list *)NULL;
		while( inner1 )
		{
			if( inner->lu != inner1->lu )
			{
				if( nmg_classify_lu_lu( inner1->lu , inner->lu , &tol ) == NMG_CLASS_AinB )
				{
					struct loop_list *tmp;

					/* inner1->lu is inside inner->lu,
					 * so delete inner1->lu
					 */
					tmp = inner1;
					if( prev )
						prev->next = inner1->next;
					else
						lptr->inner_loops = inner1->next;

					inner1 = inner1->next;
					bu_free( (char *)tmp , "Find_inner_loops: tmp" );
					deleted = 1;
				}
			}
			if( !deleted )
			{
				prev = inner1;
				inner1 = inner1->next;
			}
		}
		inner = inner->next;
	}

	/* Now find inner loops for all these inner loops */
	inner = lptr->inner_loops;
	while( inner )
	{
		Find_inner_loops( fu , inner );
		inner = inner->next;
	}
}

void
Orient_face_loops( fu )
struct faceuse *fu;
{
	struct loopuse *lu;
	struct loopuse *lu_outer=NULL;
	struct loop_list *lptr;
	int orient=OT_SAME;

	NMG_CK_FACEUSE( fu );
	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;
	if( fu->orientation != OT_SAME )
	{
		bu_log( "Orient_face_loops: fu x%x has orient %s and mate (x%x) has orient %s (no OT_SAME)\n",
			fu , nmg_orientation(fu->orientation) , fu->fumate_p , nmg_orientation( fu->fumate_p->orientation ) );
		rt_bomb( "Face with no OT_SAME use\n" );
	}

	loop_root = (struct loop_list *)NULL;

	nmg_face_bb( fu->f_p , &tol );
	for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		int outer;
		struct loopuse *lu1;

		/* check if there are any other loops containing this loop */
		outer = 1;
		for( BU_LIST_FOR( lu1 , loopuse , &fu->lu_hd ) )
		{
			if( lu1 == lu )
				continue;
			if( nmg_classify_lu_lu( lu , lu1 , &tol ) == NMG_CLASS_AinB )
			{
				outer = 0;
				break;
			}
		}

		if( outer )
		{
			lu_outer = lu;
			break;
		}
	}

	loop_root = (struct loop_list *)bu_malloc( sizeof( struct loop_list ) , "Orient_face_loops: loop_root" );
	loop_root->lu = lu_outer;
	loop_root->next = (struct loop_list *)NULL;
	loop_root->inner_loops = (struct loop_list *)NULL;

	/* now look for loops contained in other loops */
	Find_inner_loops( fu , loop_root );

	/* All the loops in the face are now in the loop_list
	 * structure from outermost working inward.
	 * Adjust the loop orientations by alternating between
	 * OT_SAME and OT_OPPOSITE
	 */
	lptr = loop_root;
	while( lptr )
	{
		struct loop_list *lptr1;

		lptr1 = lptr;
		while( lptr1 )
		{
			if( lptr1->lu->orientation != orient )
			{
				/* exchange lu and lu_mate */
				BU_LIST_DEQUEUE( &lptr1->lu->l );
				BU_LIST_DEQUEUE( &lptr1->lu->lumate_p->l );
				BU_LIST_APPEND( &fu->lu_hd , &lptr1->lu->lumate_p->l );
				lptr1->lu->lumate_p->up.fu_p = fu;
				BU_LIST_APPEND( &fu->fumate_p->lu_hd , &lptr1->lu->l );
				lptr1->lu->up.fu_p = fu->fumate_p;
				lptr1->lu->orientation = orient;
				lptr1->lu->lumate_p->orientation = orient;
			}
			lptr1 = lptr1->next;
		}
		/* move to next inner level */
		lptr = lptr->inner_loops;

		/* loop orientation must reverse */
		if( orient == OT_SAME )
			orient = OT_OPPOSITE;
		else
			orient = OT_SAME;
	}
}

void
Orient_nurb_face_loops( fu )
struct faceuse *fu;
{
	struct face *f;
	struct face_g_snurb *fg;
	struct loopuse *lu;
	int flipped;

	NMG_CK_FACEUSE( fu );

	f = fu->f_p;
	NMG_CK_FACE( f );
	flipped = f->flip;

	if( *f->g.magic_p != NMG_FACE_G_SNURB_MAGIC )
		rt_bomb( "Orient_nurb_face_loops: called with non-nurb faceuse\n" );

	fg = f->g.snurb_p;
	NMG_CK_FACE_G_SNURB( fg );

	for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	{
		int loop_uv_orient;

		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
		{
			lu->orientation = OT_SAME;
			lu->lumate_p->orientation = OT_SAME;
			continue;
		}

		loop_uv_orient = nmg_snurb_calc_lu_uv_orient( lu );

		/* if area is in +Z-direction loop encloses area counter-clockwise
		 * and must be OT_SAME. if area is in -Z-direction, loop encloses
		 * area in clockwise direction nad must be OT_OPPOOSITE
		 */
		if( (loop_uv_orient == OT_SAME && !flipped) ||
		    (loop_uv_orient == OT_OPPOSITE && flipped) )
		{
			lu->orientation = OT_SAME;
			lu->lumate_p->orientation = OT_SAME;
		}
		else if( (loop_uv_orient == OT_OPPOSITE && !flipped) ||
			 (loop_uv_orient == OT_SAME && flipped) )
		{
			lu->orientation = OT_OPPOSITE;
			lu->lumate_p->orientation = OT_OPPOSITE;
		}
		else
			rt_bomb( "Orient_nurb_face_loops: loop encloses no area in uv-space\n" );

	}
}

void
Orient_loops( r )
struct nmgregion *r;
{
	struct shell *s;

	NMG_CK_REGION( r );

	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
	{
		struct faceuse *fu;

		NMG_CK_SHELL( s );

		for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
		{
			struct face *f;

			NMG_CK_FACEUSE( fu );

			if( fu->orientation != OT_SAME )
				continue;

			f = fu->f_p;
			NMG_CK_FACE( f );

			if( !f->g.magic_p )
				rt_bomb( "Face has no geometry!!\n" );

			if( *f->g.magic_p == NMG_FACE_G_PLANE_MAGIC )
				Orient_face_loops( fu );
			else if( *f->g.magic_p == NMG_FACE_G_SNURB_MAGIC )
				Orient_nurb_face_loops( fu );
			else
				rt_bomb( "Face has unrecognized geometry type\n" );
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
