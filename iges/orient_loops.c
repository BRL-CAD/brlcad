/*
 *  Authors -
 *	John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
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
	for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		if( lu == lptr->lu )
			continue;

		if( nmg_classify_lu_lu( lu , lptr->lu , &tol ) == NMG_CLASS_AinB )
		{

			if( lptr->inner_loops == (struct loop_list *)NULL )
			{
				lptr->inner_loops = (struct loop_list *)rt_malloc( sizeof( struct loop_list ),
					"Find_inner_loops: lptr->inner_loops" );
				inner = lptr->inner_loops;
			}
			else
			{
				inner = lptr->inner_loops;
				while( inner->next != (struct loop_list *)NULL )
					inner = inner->next;
				inner->next = (struct loop_list *)rt_malloc( sizeof( struct loop_list ),
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
					rt_free( (char *)tmp , "Find_inner_loops: tmp" );
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
	struct loopuse *lu_outer;
	struct loop_list *lptr;
	int orient=OT_SAME;

	NMG_CK_FACEUSE( fu );
	if( fu->orientation != OT_SAME )
		fu = fu->fumate_p;
	if( fu->orientation != OT_SAME )
	{
		rt_log( "Orient_face_loops: fu x%x has orient %s and mate (x%x) has orient %s (no OT_SAME)\n",
			fu , nmg_orientation(fu->orientation) , fu->fumate_p , nmg_orientation( fu->fumate_p->orientation ) );
		rt_bomb( "Face with no OT_SAME use\n" );
	}

	loop_root = (struct loop_list *)NULL;

	nmg_face_bb( fu->f_p , &tol );
	for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
	{
		int outer;
		struct loopuse *lu1;

		/* check if there are any other loops containing this loop */
		outer = 1;
		for( RT_LIST_FOR( lu1 , loopuse , &fu->lu_hd ) )
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

	loop_root = (struct loop_list *)rt_malloc( sizeof( struct loop_list ) , "Orient_face_loops: loop_root" );
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
				RT_LIST_DEQUEUE( &lptr1->lu->l );
				RT_LIST_DEQUEUE( &lptr1->lu->lumate_p->l );
				RT_LIST_APPEND( &fu->lu_hd , &lptr1->lu->lumate_p->l );
				lptr1->lu->lumate_p->up.fu_p = fu;
				RT_LIST_APPEND( &fu->fumate_p->lu_hd , &lptr1->lu->l );
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

	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	{
		struct edgeuse *eu;
		int edge_count=0;
		int edge_no;
		point_t *pts;
		vect_t area;

		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
		{
			lu->orientation = OT_SAME;
			lu->lumate_p->orientation = OT_SAME;
			continue;
		}

		/* count "psuedo-vertices" in loop */
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		{
			struct edge_g_cnurb *eg;

			NMG_CK_EDGEUSE( eu );

			if( *eu->g.magic_p != NMG_EDGE_G_CNURB_MAGIC )
				rt_bomb( "EU on NURB face does not have cnurb geometry\n" );

			eg = eu->g.cnurb_p;
			NMG_CK_EDGE_G_CNURB( eg );

			if( eg->order <= 0 )
				edge_count++;
			else
				edge_count += 5;
		}

		/* allocate memory for "psuedo-vertices" */
		pts = (point_t *)rt_calloc( edge_count, sizeof( point_t ), "Orient_nurb_face_loops: pts" );

		/* Assign uv geometry to each "psuedo-vertex" */
		edge_no = 0;
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		{
			struct edge_g_cnurb *eg;
			struct vertexuse *vu;
			struct vertexuse_a_cnurb *vg1,*vg2;

			eg = eu->g.cnurb_p;

			if( eg->order <= 0 )
			{
				vu = eu->vu_p;
				NMG_CK_VERTEXUSE( vu );
				if( *vu->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC )
					rt_bomb( "Orient_nurb_face_loops: vertexuse in snurb faceuse doesn't have cnurb attribute\n" );
				vg1 = vu->a.cnurb_p;
				VMOVE( pts[edge_no], vg1->param )
				edge_no++;
			}
			else
			{
				fastf_t t1,t2;
				struct cnurb crv;
				hpoint_t crv_pt;
				int coords;
				int i;

				nmg_hack_cnurb( &crv, eg );
				t1 = crv.knot.knots[0];
				t2 = crv.knot.knots[crv.knot.k_size-1];
				coords = RT_NURB_EXTRACT_COORDS( crv.pt_type );

				for( i=0 ; i<5 ; i++ )
				{
					fastf_t t;

					t = t1 + (t2 - t1)*0.2*(fastf_t)i;

					VSETALLN( crv_pt, 0.0, coords )
					rt_nurb_c_eval( &crv, t, crv_pt );
					if( RT_NURB_IS_PT_RATIONAL( crv.pt_type ) )
						VSCALE( pts[edge_no], crv_pt, crv_pt[coords-1] )
					else
						VMOVE( pts[edge_no], crv_pt )
					edge_no++;
				}
			}
		}

		/* translate loop such that pts[0] is at (0,0,0) */
		for( edge_no=1 ; edge_no<edge_count ; edge_no++ )
		{
			VSUB2( pts[edge_no], pts[edge_no], pts[0] )
			pts[edge_no][Z] = 0.0;
		}
		VSETALL( pts[0], 0.0 )

		/* calculate area of loop in uv-space */
		VSETALL( area, 0.0 );
		for( edge_no=1 ; edge_no<edge_count-1 ; edge_no++ )
		{
			vect_t cross;

			VCROSS( cross, pts[edge_no], pts[edge_no+1] );
			VADD2( area, area, cross );
		}

		/* if area is in +Z-direction loop encloses area counter-clockwise
		 * and must be OT_SAME. if area is in -Z-direction, loop encloses
		 * area in clockwise direction nad must be OT_OPPOOSITE
		 */
		if( (area[Z] > 0.0 && !flipped) || (area[Z] < 0.0 && flipped) )
		{
			lu->orientation = OT_SAME;
			lu->lumate_p->orientation = OT_SAME;
		}
		else if( (area[Z] < 0.0 && !flipped) || (area[Z] > 0.0 && flipped) )
		{
			lu->orientation = OT_OPPOSITE;
			lu->lumate_p->orientation = OT_OPPOSITE;
		}
		else
			rt_bomb( "Orient_nurb_face_loops: loop encloses no area in uv-space\n" );

		rt_free( (char *)pts, "Orient_nurb_face_loops: pts" );
	}
}

void
Orient_loops( r )
struct nmgregion *r;
{
	struct shell *s;

	NMG_CK_REGION( r );

	for( RT_LIST_FOR( s , shell , &r->s_hd ) )
	{
		struct faceuse *fu;

		NMG_CK_SHELL( s );

		for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
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
