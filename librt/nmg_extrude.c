/*		N M G _ E X T R U D E . C
 *	Routines for extruding nmg's.
 *
 *  Authors -
 *	Michael Markowski
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

#include "conf.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "rtlist.h"
#include "./debug.h"

RT_EXTERN( struct shell *nmg_dup_shell , ( struct shell *s , long ***copy_tbl, CONST struct bn_tol *tol ) );
RT_EXTERN( void nmg_isect_shell_self , ( struct shell *s , CONST struct bn_tol *tol ) );
RT_EXTERN( fastf_t nmg_loop_plane_area , ( struct loopuse *lu , plane_t pl ) );

/*
 *	V e r t s _ i n _ N M G _ L o o p
 *
 *	Count number of vertices in an NMG loop.
 */
static int
verts_in_nmg_loop(lu)
struct loopuse	*lu;
{
	int		cnt;
	struct edgeuse	*eu;
	struct vertex	*v;

	/* Count number of vertices in loop. */
	cnt = 0;
	NMG_CK_LOOPUSE(lu);
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);
			NMG_CK_EDGE(eu->e_p);
			NMG_CK_VERTEXUSE(eu->vu_p);
			NMG_CK_VERTEX(eu->vu_p->v_p);
			cnt++;
		}
	} else if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		v = BU_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p;
		NMG_CK_VERTEX(v);
		cnt++;
	} else
		rt_bomb("verts_in_nmg_loop: bad loopuse\n");
	return(cnt);
}

/*
 *	V e r t s _ i n _ N M G _ F a c e
 *
 *	Count number of vertices in an NMG face.
 */
static int
verts_in_nmg_face(fu)
struct faceuse	*fu;
{
	int		cnt;
	struct loopuse	*lu;

	cnt = 0;
	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))
		cnt += verts_in_nmg_loop(lu);
	return(cnt);
}

/*
 *	T r a n s l a t e _ N M G _ F a c e
 *
 *	Translate a face using a vector's magnitude and direction.
 */
void
nmg_translate_face(fu, Vec, tol)
struct faceuse	*fu;
vect_t		Vec;
struct bn_tol	*tol;
{
	int		cnt,		/* Number of vertices in face. */
			cur,
			i,
			in_there;
	struct vertex	**verts;	/* List of verts in face. */
	struct edgeuse	*eu;
	struct loopuse	*lu;
	struct vertex	*v;
	struct faceuse	*fu_tmp;
	plane_t pl;
	struct bu_ptbl	edge_g_tbl;

	bu_ptbl_init( &edge_g_tbl , 64, " &edge_g_tbl ");

	cur = 0;
	cnt = verts_in_nmg_face(fu);
	verts = (struct vertex **)
		rt_malloc(cnt * sizeof(struct vertex *), "verts");
	for (i = 0; i < cnt; i++)
		verts[i] = NULL;

	/* Go through each loop and translate it. */
	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
			for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				in_there = 0;
				for (i = 0; i < cur && !in_there; i++)
					if (verts[i] == eu->vu_p->v_p)
						in_there = 1;
				if (!in_there) {
					verts[cur++] = eu->vu_p->v_p;
					VADD2(eu->vu_p->v_p->vg_p->coord,
						eu->vu_p->v_p->vg_p->coord,
						Vec);
				}
			}
		} else if (BU_LIST_FIRST_MAGIC(&lu->down_hd)
			== NMG_VERTEXUSE_MAGIC) {
			v = BU_LIST_FIRST(vertexuse, &lu->down_hd)->v_p;
			NMG_CK_VERTEX(v);
			VADD2(v->vg_p->coord, v->vg_p->coord, Vec);
		} else
			rt_bomb("nmg_translate_face: bad loopuse\n");
	}

	fu_tmp = fu;
	if( fu_tmp->orientation != OT_SAME )
		fu_tmp = fu_tmp->fumate_p;

	/* Move edge geometry */
	nmg_edge_g_tabulate( &edge_g_tbl , &fu->l.magic );
	for( i=0 ; i<BU_PTBL_END( &edge_g_tbl ) ; i++ )
	{
		long *ep;
		struct edge_g_lseg *eg;

		ep = BU_PTBL_GET( &edge_g_tbl , i );
		switch( *ep )
		{
			case NMG_EDGE_G_LSEG_MAGIC:
				eg = (struct edge_g_lseg *)ep;
				NMG_CK_EDGE_G_LSEG( eg );
				VADD2( eg->e_pt , eg->e_pt , Vec );
				break;
			case NMG_EDGE_G_CNURB_MAGIC:
				/* XXX Move cnurb edge geometry??? */
				break;
		}
	}

	bu_ptbl_free( &edge_g_tbl );

	if(nmg_loop_plane_area( BU_LIST_FIRST( loopuse , &fu_tmp->lu_hd ) , pl ) < 0.0 )
	{
		rt_bomb( "nmg_translate_face: Cannot calculate plane equation for face\n" );
	}
	nmg_face_g( fu_tmp , pl );
	rt_free((char *)verts, "verts");
}

/*
 *	N M G _ E x t r u d e _ F a c e
 *
 *	Duplicate a given NMG face, move it by specified vector,
 *	and create a solid bounded by these faces.
 */
int
nmg_extrude_face(fu, Vec, tol)
struct faceuse	*fu;	/* Face to extrude. */
vect_t		Vec;	/* Magnitude and direction of extrusion. */
CONST struct bn_tol	*tol;	/* NMG tolerances. */
{
	fastf_t		cosang;
	int		nfaces;
	struct faceuse	*fu2, *nmg_dup_face(), **outfaces;
	int		face_count=2;
	struct loopuse	*lu, *lu2;
	plane_t		n;

#define MIKE_TOL 0.0001

	NMG_CK_FACEUSE( fu );
	BN_CK_TOL( tol );

	/* Duplicate and reverse face. */
	fu2 = nmg_dup_face(fu, fu->s_p);
	nmg_reverse_face( fu2 );
	if( fu2->orientation != OT_OPPOSITE )
		fu2 = fu2->fumate_p;

	/* Figure out which face to translate. */
	NMG_GET_FU_PLANE( n, fu );
	cosang = VDOT(Vec, n);
	if (NEAR_ZERO(cosang, MIKE_TOL))
		rt_bomb("extrude_nmg_face: extrusion cannot be parallel to face\n");
	if (cosang > 0.)
		nmg_translate_face(fu, Vec, tol);
	else if (cosang < 0.)
		nmg_translate_face(fu2->fumate_p, Vec, tol);

	nfaces = verts_in_nmg_face( fu );
	outfaces = (struct faceuse **)rt_calloc( nfaces+2 , sizeof( struct faceuse *) ,
		"nmg_extrude_face: outfaces" );

	outfaces[0] = fu;
	outfaces[1] = fu2->fumate_p;

	for( BU_LIST_FOR2(lu , lu2 , loopuse , &fu->lu_hd , &fu2->lu_hd ) )
	{
		struct edgeuse *eu,*eu2;

		NMG_CK_LOOPUSE( lu );
		NMG_CK_LOOPUSE( lu2 );

		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;
		if( BU_LIST_FIRST_MAGIC( &lu2->down_hd ) != NMG_EDGEUSE_MAGIC )
		{
			bu_log( "nmg_extrude_face: Original face and dup face don't match up!!\n" );
			return( -1 );
		}
		for( BU_LIST_FOR2( eu , eu2 , edgeuse , &lu->down_hd , &lu2->down_hd ) )
		{
			struct vertex	*vertlist[4];

			NMG_CK_EDGEUSE( eu );
			NMG_CK_EDGEUSE( eu2 );

			vertlist[0] = eu->vu_p->v_p;
			vertlist[1] = eu2->vu_p->v_p;
			vertlist[2] = eu2->eumate_p->vu_p->v_p;
			vertlist[3] = eu->eumate_p->vu_p->v_p;
			outfaces[face_count] = nmg_cface( fu->s_p , vertlist , 4 );
			if( nmg_calc_face_g( outfaces[face_count] ) )
			{
				bu_log( "nmg_extrude_face: failed to calculate plane eqn\n" );
				return( -1 );
			}
			face_count++;
		}

	}

	nmg_gluefaces( outfaces , face_count, tol );

	rt_free( (char *)outfaces , "nmg_extrude_face: outfaces" );

	return( 0 );
}

/*	N M G _ F I N D _ V E R T E X _ I N _ L U
 *
 * find a use of vertex v in loopuse lu
 */

struct vertexuse *
nmg_find_vertex_in_lu( v , lu )
CONST struct vertex *v;
CONST struct loopuse *lu;
{
	struct edgeuse *eu;
	struct vertexuse *ret_vu;

	NMG_CK_VERTEX( v );
	NMG_CK_LOOPUSE( lu );

	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
	{
		struct vertexuse *vu;

		vu = BU_LIST_FIRST( vertexuse , &lu->down_hd );
		NMG_CK_VERTEXUSE( vu );

		if( vu->v_p == v )
			return( vu );
		else
			return( (struct vertexuse *)NULL );
	}

	ret_vu = (struct vertexuse *)NULL;
	for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
	{
		NMG_CK_EDGEUSE( eu );

		if( eu->vu_p->v_p == v )
		{
			ret_vu = eu->vu_p;
			break;
		}
	}

	return( ret_vu );
}

/*	N M G _ S T A R T _ N E W _ L O O P
 *
 * recursive routine to build tables of edgeuse that may be used
 * to create new loopuses. lu1 and lu2 are overlapping edgeuses
 * from the same faceuse. This is a support routine for nmg_fix_overlapping loops
 */
static void
nmg_start_new_loop( start_eu , lu1 , lu2 , loops )
struct edgeuse *start_eu;
struct loopuse *lu1;
struct loopuse *lu2;
struct bu_ptbl *loops;
{
	struct bu_ptbl *new_lu_tab;
	struct loopuse *this_lu;
	struct loopuse *other_lu;
	struct edgeuse *eu;
	int edges=0;
	int done=0;

	NMG_CK_EDGEUSE( start_eu );
	NMG_CK_LOOPUSE( lu1 );
	NMG_CK_LOOPUSE( lu2 );

	/* create a table to hold eu pointers for a new loop */
	new_lu_tab = (struct bu_ptbl *)rt_malloc( sizeof( struct bu_ptbl ) , "nmg_start_new_loop: new_lu_tab" );
	bu_ptbl_init( new_lu_tab , 64, " new_lu_tab ");

	/* add this table to the list of loops */
	bu_ptbl_ins( loops , (long *)new_lu_tab );

	/* put edgeuses from lu1 into new_lu_tab until a vertex shared by lu1 and lu2 is encountered
	 * or until start_eu is encountered
	 */

	this_lu = lu1;
	other_lu = lu2;
	eu = start_eu;
	while( !done )
	{
		struct edgeuse *next_eu;
		struct vertexuse *vu2;

		next_eu = BU_LIST_PNEXT_CIRC( edgeuse , &eu->l );

		/* skip this checking until we get by the first edgeuse */
		if( edges )
		{
			/* Are we back to the begining? */
			if( (eu->vu_p->v_p == start_eu->vu_p->v_p ) )
			{
				/* done with this loop */
				done = 1;
				break;
			}

			/* Are we at an intersect point? */
			vu2 = nmg_find_vertex_in_lu( eu->vu_p->v_p , other_lu );
			if( vu2 )
			{
				/* Yes, we may need to start another loop */
				struct edgeuse *eu2;
				struct loopuse *lu_tmp;
				int loop_started=0;
				int i;

				eu2 = vu2->up.eu_p;

				/* check if a loop has already been started here */
				for( i=0 ; i<BU_PTBL_END( loops ) ; i++ )
				{
					struct bu_ptbl *loop_tab;
					struct edgeuse *loop_start_eu;

					loop_tab = (struct bu_ptbl *)BU_PTBL_GET( loops , i );
					loop_start_eu = (struct edgeuse *)BU_PTBL_GET( loop_tab , 0 );
					if( loop_start_eu == eu )
					{
						loop_started = 1;
						break;
					}
				}

				/* if a loop has not already been started here
				 * start one with the current edgeuse
				 */
				if( !loop_started )
					nmg_start_new_loop( eu , this_lu , other_lu , loops );

				/* continue this loop by switching to the other loopuse */
				eu = eu2;
				next_eu = BU_LIST_PNEXT_CIRC( edgeuse , &eu->l );
				lu_tmp = this_lu;
				this_lu = other_lu;
				other_lu = lu_tmp;
			}
		}

		/* add this edgeuse to the current list */
		bu_ptbl_ins( new_lu_tab , (long *)eu );

		edges++;

		/* go to the next edgeuse */
		eu = next_eu;
	}

}

/*	N M G _ F I X _ O V E R L A P P I N G _ L O O P S
 *
 * Looks at each faceuse in the shell and checks if loopuses in that
 * faceuse overlap each other. This code can only handle faceuses that
 * have at most one OT_SAME loopuse and one OT_OPPOSITE loopuse, so
 * nmg_split_loops_into_faces is called to simplify the faceuses.
 *
 * Overlapping OT_SAME and OT_OPPOSITE loops are broken into some
 * number of OT_SAME loopuses. An edgeuse (from the OT_SAME loopuse)
 * departing from a point where the loops intersect and outside the
 * OT_OPPOSITE loopuse is found as a starting point. Edgeuses from this
 * loopuse are moved to a new loopuse until another intersect point is
 * encountered. At that point, another loop is started using the next edgeuse
 * and the current loopuse is continued by following the other loopuse.
 * this is continued until the original edgeuse is encountered.
 *
 * If overlapping loops are found, new loopsuses are created and the
 * original loopuses are killed
 */
void
nmg_fix_overlapping_loops( s , tol )
struct shell *s;
CONST struct bn_tol *tol;
{
	struct faceuse *fu;
	struct edgeuse *start_eu;
	struct bu_ptbl loops;
	int i;

	NMG_CK_SHELL( s );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_fix_overlapping_loops: s = x%x\n" , s );

	/* this routine needs simple faceuses */
	nmg_split_loops_into_faces( &s->l.magic , tol );

	/* This table will contain a list of bu_ptbl's when we are
	 * finished. Each of those bu_ptbl's will be a list of
	 * edgeuses that comprise a new loop
	 */
	bu_ptbl_init( &loops , 64, " &loops ");

	/* process all faceuses in the shell */
	for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		struct loopuse *lu1,*lu2;
		struct edgeuse *eu1;
		struct edgeuse *eu;
		int inside=0;
		int outside=0;

		NMG_CK_FACEUSE( fu );

		/* don't process the same face twice */
		if( fu->orientation != OT_SAME )
			continue;

		/* This is pretty simple-minded right now, assuming that
		 * there are only two loopuses
		 */
		lu1 = BU_LIST_FIRST( loopuse , &fu->lu_hd );
		NMG_CK_LOOPUSE( lu1 );

		lu2 = BU_LIST_PNEXT( loopuse , &lu1->l );

		/* if there is only one loopuse, nothing to do */
		if( BU_LIST_IS_HEAD( lu2 , &fu->lu_hd ) )
			continue;

		NMG_CK_LOOPUSE( lu2 );


		/* if the loopuses aren't both loops af edges, nothing to do */
		if( BU_LIST_FIRST_MAGIC( &lu1->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		if( BU_LIST_FIRST_MAGIC( &lu2->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		/* if both loopuses are the same orientation, something is wrong */
		if( lu1->orientation == lu2->orientation )
		{
			bu_log( "nmg_fix_overlapping_loops: Cannot handle loops of same orientation\n" );
			nmg_pr_fu_briefly( fu , (char *)NULL );
			continue;
		}

		/* At this point we have an OT_SAME and an OT_OPPOSITE loopuses
		 * for simplicity, force lu1 to be the OT_SAME loopuse */
		if( lu1->orientation == OT_OPPOSITE && lu2->orientation == OT_SAME )
		{
			struct loopuse *lu_tmp;

			lu_tmp = lu1;
			lu1 = lu2;
			lu2 = lu_tmp;
		}
		else if( lu2->orientation != OT_OPPOSITE || lu1->orientation != OT_SAME )
		{
			bu_log( "nmg_fix_overlapping_loops: bad loop orientations %s and %s\n",
				nmg_orientation( lu1->orientation ),
				nmg_orientation( lu2->orientation ) );
			continue;
		}

		/* lu1 is OT_SAME and lu2 is OT_OPPOSITE, check for overlap */

		/* count how many vertices in lu2 are inside lu1 and outside lu1 */
		for( BU_LIST_FOR( eu , edgeuse , &lu2->down_hd ) )
		{
			struct vertexuse *vu;

			NMG_CK_EDGEUSE( eu );

			vu = eu->vu_p;

			/* ignore vertices that are shared between the loops */
			if( !nmg_find_vertex_in_lu( vu->v_p , lu1 ) )
			{
				int class;

				class = nmg_classify_pt_loop( vu->v_p->vg_p->coord , lu1 , tol );
				if( class == NMG_CLASS_AoutB )
					outside++;
				else if( class == NMG_CLASS_AinB )
					inside++;
			}
		}

		/* if we don't have vertices both inside and outside lu1,
		 * then there is no overlap
		 */
		if( !inside || !outside ) /* no overlap */
			continue;

		/* the loops overlap, now fix it */

		/* first, split the edges where the two loops cross each other */
		for( BU_LIST_FOR( eu1 , edgeuse , &lu1->down_hd ) )
		{
			vect_t v1;
			struct edgeuse *eu2;

			VSUB2( v1 , eu1->eumate_p->vu_p->v_p->vg_p->coord , eu1->vu_p->v_p->vg_p->coord );
			for( BU_LIST_FOR( eu2 , edgeuse , &lu2->down_hd ) )
			{
				vect_t v2;
				fastf_t dist[2];
				struct vertex *v=(struct vertex *)NULL;

				VSUB2( v2 , eu2->eumate_p->vu_p->v_p->vg_p->coord ,
						eu2->vu_p->v_p->vg_p->coord );

				if( bn_isect_lseg3_lseg3( dist , eu1->vu_p->v_p->vg_p->coord , v1 ,
					eu2->vu_p->v_p->vg_p->coord , v2 , tol ) >= 0 )
				{
					struct edgeuse *new_eu;

					if( dist[0]>0.0 && dist[0]<1.0 &&
					    dist[1]>=0.0 && dist[1]<=1.0 )
					{
						point_t pt;

						if( dist[1] == 0.0 )
							v = eu2->vu_p->v_p;
						else if( dist[1] == 1.0 )
							v = eu2->eumate_p->vu_p->v_p;
						else
						{
							VJOIN1( pt , eu1->vu_p->v_p->vg_p->coord , dist[0] , v1 );
							new_eu = nmg_esplit( v , eu1, 0 );
							v = new_eu->vu_p->v_p;
							if( !v->vg_p )
								nmg_vertex_gv( v , pt );
						}

						VSUB2( v1 , eu1->eumate_p->vu_p->v_p->vg_p->coord ,
								eu1->vu_p->v_p->vg_p->coord );
					}
					if( dist[1]>0.0 && dist[1]<1.0 && dist[0]>=0.0 && dist[0]<=1.0 )
					{
						point_t pt;

						if( dist[0] == 0.0 )
							v = eu1->vu_p->v_p;
						else if( dist[0] == 1.0 )
							v = eu2->eumate_p->vu_p->v_p;
						else
						{
							VJOIN1( pt , eu2->vu_p->v_p->vg_p->coord , dist[1] , v2 );
							new_eu = nmg_esplit( v , eu2, 0 );
							v = new_eu->vu_p->v_p;
							if( !v->vg_p )
								nmg_vertex_gv( v , pt );
						}

						VSUB2( v2 , eu2->eumate_p->vu_p->v_p->vg_p->coord ,
								eu2->vu_p->v_p->vg_p->coord );
					}
				}
			}
		}

		/* find a vertex that lu1 and lu2 share, where eu1 is outside lu2
		 * this will be a starting edgeuse for a new loopuse
		 */
		start_eu = (struct edgeuse *)NULL;
		for( BU_LIST_FOR( eu1 , edgeuse , &lu1->down_hd ) )
		{
			struct vertex *v1,*v2;
			point_t mid_pt;

			/* must be a shared vertex */
			if( !nmg_find_vertex_in_lu( eu1->vu_p->v_p , lu2 ) )
				continue;

			v1 = eu1->vu_p->v_p;
			NMG_CK_VERTEX( v1 );
			v2 = eu1->eumate_p->vu_p->v_p;
			NMG_CK_VERTEX( v2 );

			/* use midpoint to determine if edgeuse is in or out of lu2 */
			VBLEND2( mid_pt , 0.5 , v1->vg_p->coord , 0.5 , v2->vg_p->coord )

			if( nmg_classify_pt_loop( mid_pt , lu2 , tol ) == NMG_CLASS_AoutB )
			{
				start_eu = eu1;
				break;
			}
		}

		if( !start_eu )
		{
			bu_log( "nmg_fix_overlapping_loops: cannot find start point for new loops\n" );
			bu_log( "lu1=x%x, lu2=x%x\n" , lu1 , lu2 );
			nmg_pr_fu_briefly( fu , (char *)NULL );
			continue;;
		}

		bu_ptbl_reset( &loops );

		/* start new loop
		 * this routine will recurse, building as many tables as needed
		 */
		nmg_start_new_loop( start_eu , lu1 , lu2 , &loops );

		/* use loops table to create the new loops */
		for( i=0 ; i<BU_PTBL_END( &loops ) ; i++ )
		{
			struct loopuse *new_lu;
			struct loopuse *new_lu_mate;
			struct bu_ptbl *loop_tab;
			int eu_no;

			/* each table represents a new loopuse to be constructed */
			loop_tab = (struct bu_ptbl *)BU_PTBL_GET( &loops , i );

			/* if there are some entries in this table, make a new loopuse */
			if( BU_PTBL_END( loop_tab ) )
			{
				/* create new loop */
				new_lu = nmg_mlv( &fu->l.magic , (struct vertex *)NULL , OT_SAME );
				new_lu_mate = new_lu->lumate_p;

				/* get rid of vertex just created */
				nmg_kvu( BU_LIST_FIRST( vertexuse , &new_lu->down_hd ) );
				nmg_kvu( BU_LIST_FIRST( vertexuse , &new_lu_mate->down_hd ) );

				/* move edgeuses to new loops */
				for( eu_no=0 ; eu_no<BU_PTBL_END( loop_tab ) ; eu_no++ )
				{
					struct edgeuse *mv_eu;

					/* get edgeuse to be moved */
					mv_eu = (struct edgeuse *)BU_PTBL_GET( loop_tab , eu_no );
					NMG_CK_EDGEUSE( mv_eu );

					/* move it to new loopuse */
					BU_LIST_DEQUEUE( &mv_eu->l );
					BU_LIST_INSERT( &new_lu->down_hd , &mv_eu->l );
					mv_eu->up.lu_p = new_lu;

					/* move edgeuse mate to loopuse mate */
					BU_LIST_DEQUEUE( &mv_eu->eumate_p->l );
					BU_LIST_APPEND( &new_lu_mate->down_hd , &mv_eu->eumate_p->l );
					mv_eu->eumate_p->up.lu_p = new_lu_mate;
				}

				bu_ptbl_free( loop_tab );
				rt_free( (char *)loop_tab , "nmg_fix_overlapping_loops: loop_tab" );
			}
		}

		/* kill empty loopuses left in faceuse */
		lu1 = BU_LIST_FIRST( loopuse , &fu->lu_hd );
		while( BU_LIST_NOT_HEAD( lu1 , &fu->lu_hd ) )
		{
			struct loopuse *next_lu;

			next_lu = BU_LIST_PNEXT( loopuse , &lu1->l );

			if( BU_LIST_IS_EMPTY( &lu1->down_hd ) )
			{
				if( nmg_klu( lu1 ) )
					rt_bomb( "nmg_fix_overlapping_loops: Emptied faceuse!!\n" );
			}
			lu1 = next_lu;
		}
	}
	bu_ptbl_free( &loops );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_fix_overlapping_loops: done\n" );
}

/*	N M G _ B R E A K _ C R O S S E D _ L O O P S
 *
 *	Extrusion may create crossed loops within a face.
 *	This routine intersects each edge within a loop with every other edge
 *	in the loop
 */
void
nmg_break_crossed_loops( is , tol )
struct shell *is;
CONST struct bn_tol *tol;
{
	struct faceuse *fu;

	NMG_CK_SHELL( is );
	BN_CK_TOL( tol );

	for( BU_LIST_FOR( fu , faceuse , &is->fu_hd ) )
	{
		struct loopuse *lu;

		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			continue;

		for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			struct edgeuse *eu1,*eu2;
			vect_t v1,v2;

			NMG_CK_LOOPUSE( lu );

			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
				continue;

			for( BU_LIST_FOR( eu1 , edgeuse , &lu->down_hd ) )
			{
				VSUB2( v1 , eu1->eumate_p->vu_p->v_p->vg_p->coord ,
						eu1->vu_p->v_p->vg_p->coord );

				eu2 = BU_LIST_PNEXT( edgeuse , eu1 );
				while( BU_LIST_NOT_HEAD( eu2 , &lu->down_hd ) )
				{
					fastf_t dist[2];

					VSUB2( v2 , eu2->eumate_p->vu_p->v_p->vg_p->coord ,
							eu2->vu_p->v_p->vg_p->coord );

					if( bn_isect_lseg3_lseg3( dist , eu1->vu_p->v_p->vg_p->coord , v1 ,
						eu2->vu_p->v_p->vg_p->coord , v2 , tol ) >= 0 )
					{
						point_t pt;
						struct edgeuse *new_eu;
						struct vertex *v=(struct vertex *)NULL;

						if( dist[0]>0.0 && dist[0]<1.0 &&
							dist[1]>=0.0 && dist[1]<=1.0 )
						{
							if( dist[1] == 0.0 )
								v = eu2->vu_p->v_p;
							else if( dist[1] == 1.0 )
								v = eu2->eumate_p->vu_p->v_p;
							else
							{
								VJOIN1( pt , eu1->vu_p->v_p->vg_p->coord ,
									dist[0] , v1 );
								v = nmg_find_pt_in_shell( is , pt , tol );
							}

							new_eu = nmg_esplit( v , eu1, 0 );
							v = new_eu->vu_p->v_p;
							if( !v->vg_p )
								nmg_vertex_gv( v , pt );

							VSUB2( v1 , eu1->eumate_p->vu_p->v_p->vg_p->coord ,
								eu1->vu_p->v_p->vg_p->coord );
						}
						if( dist[1] > 0.0 && dist[1] < 1.0 &&
							dist[0]>=0.0 && dist[0]<=1.0 )
						{
							if( dist[0] == 0.0 )
								v = eu1->vu_p->v_p;
							else if( dist[0] == 1.0 )
								v = eu1->eumate_p->vu_p->v_p;
							else
							{
								VJOIN1( pt , eu2->vu_p->v_p->vg_p->coord , dist[1] , v2 );
								v = nmg_find_pt_in_shell( is , pt , tol );
							}

							new_eu = nmg_esplit( v , eu2, 0 );
							v = new_eu->vu_p->v_p;
							if( !v->vg_p )
								nmg_vertex_gv( v , pt );
						}
					}
					eu2 = BU_LIST_PNEXT( edgeuse , eu2 );
				}
			}
		}
	}
}

/*
 *	N M G _ E X T R U D E _ C L E A N U P
 *
 *	Clean up after nmg_extrude_shell.
 *	intersects each face with every other face in the shell and
 *	makes new face boundaries at the intersections.
 *	decomposes the result into seperate shells.
 *	where faces have intersected, new shells will be created.
 *	These shells are detected and killed
 */
struct shell *
nmg_extrude_cleanup( is , is_void , tol )
struct shell *is;
CONST int is_void;
CONST struct bn_tol *tol;
{
	struct model *m;
	struct nmgregion *new_r;
	struct faceuse *fu;
	struct loopuse *lu;
	struct vertexuse *vu;
	struct nmgregion *old_r;
	struct shell *s_tmp;

	NMG_CK_SHELL( is );
	BN_CK_TOL( tol );

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_extrude_cleanup( is=x%x )\n" , is );

	m = nmg_find_model( &is->l.magic );

	/* intersect each face in the shell with every other face in the same shell */
	nmg_isect_shell_self( is , tol );

	/* Extrusion may create loops that overlap */
	nmg_fix_overlapping_loops( is , tol );

	/* look for self-touching loops */
	for( BU_LIST_FOR( fu , faceuse , &is->fu_hd ) )
	{
		if( fu->orientation != OT_SAME )
			continue;

		lu = BU_LIST_LAST( loopuse , &fu->lu_hd );
		while( BU_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			struct loopuse *new_lu;
			int orientation;

			/* check this loopuse */
			while( (vu=(struct vertexuse *)nmg_loop_touches_self( lu ) ) != (struct vertexuse *)NULL )
			{
				/* Split this touching loop, but give both resulting loops
				 * the same orientation as the original. This will result
				 * in the part of the loop that needs to be discarded having
				 * an incorrect orientation with respect to the face.
				 * This incorrect orientation will be discovered later by
				 * "nmg_bad_face_normals" and will result in the undesirable
				 * portion's demise
				 */

				orientation = lu->orientation;
				new_lu = nmg_split_lu_at_vu( lu , vu );
				new_lu->orientation = orientation;
				lu->orientation = orientation;
				new_lu->lumate_p->orientation = orientation;
				lu->lumate_p->orientation = orientation;
			}

			lu = BU_LIST_PLAST( loopuse , &lu->l );
		}
	}

	nmg_rebound( m , tol );

	/* remember the nmgregion where "is" came from */
	old_r = is->r_p;

	/* make a new nmgregion , shell, and vertex */
	new_r = nmg_mrsv( m );

	/* s_tmp is the shell just created */
	s_tmp = BU_LIST_FIRST( shell , &new_r->s_hd );

	/* move our shell (is) to the new nmgregion
	 * in preparaion for nmg_decompose_shell.
	 * don't want to confuse pieces of this shell
	 * with other shells in "old_r"
	 */
	(void)nmg_mv_shell_to_region( is , new_r );

	/* kill the unused, newly created shell */
	if( nmg_ks( s_tmp ) )
		rt_bomb( "nmg_extrude_shell: Nothing got moved to new region\n" );

	/* now decompose our shell, count number of inside shells */
	if( (nmg_decompose_shell( is , tol )) < 2 )
	{
		/*  we still have only one shell */
		if( nmg_bad_face_normals( is , tol ) )
		{
			(void)nmg_ks( is );
			is = (struct shell *)NULL;
		}
		else if( is_void != -1 && nmg_shell_is_void( is ) != is_void )
		{
			(void)nmg_ks( is );
			is = (struct shell *)NULL;
		}
		else
			(void)nmg_mv_shell_to_region( is , old_r );

		nmg_kr( new_r );
		new_r = NULL;
	}
	else
	{
		/* look at each shell in "new_r" */
		s_tmp = BU_LIST_FIRST( shell , &new_r->s_hd );
		while( BU_LIST_NOT_HEAD( s_tmp , &new_r->s_hd ) )
		{
			struct shell *next_s;
			int kill_it=0;

			next_s = BU_LIST_PNEXT( shell , &s_tmp->l );

			if( nmg_bad_face_normals( s_tmp , tol ) )
				kill_it = 1;

			if( !kill_it )
			{
				if( is_void != -1 && nmg_shell_is_void( s_tmp ) != is_void )
					kill_it = 1;
			}

			if( kill_it )
			{
				/* Bad shell, kill it */
				if( nmg_ks( s_tmp ) )
				{
					nmg_kr( new_r );
					new_r = (struct nmgregion *)NULL;
					is = (struct shell *)NULL;
					break;
				}
			}
			s_tmp = next_s;
		}
	}

	if( new_r )
	{
		/* merge remaining shells in "new_r" */
		is = BU_LIST_FIRST( shell , &new_r->s_hd );

		s_tmp = BU_LIST_PNEXT( shell , &is->l );
		while( BU_LIST_NOT_HEAD( s_tmp , &new_r->s_hd ) )
		{
			struct shell *next_s;

			next_s = BU_LIST_PNEXT( shell , &s_tmp->l );

			if( s_tmp == is )
			{
				s_tmp = next_s;
				continue;
			}

			nmg_js( is , s_tmp , tol );
			s_tmp = next_s;
		}

		/* move it all back into the original nmgregion */
		if( is )
			(void)nmg_mv_shell_to_region( is , old_r );

		/* kill the temporary nmgregion */
		if( BU_LIST_NON_EMPTY( &new_r->s_hd ) )
			bu_log( "nmg_extrude_cleanup: temporary nmgregion not empty!!\n" );

		(void)nmg_kr( new_r );
	}
	return( is );
}

/*
 *	N M G _ H O L L O W _ S H E L L
 *
 *	Hollows out a shell producing a wall thickness of thickness "thick"
 *	by creating a new "inner" shell and combining the two shells.
 *
 *	If the original shell is closed, the new shell is simply
 *	merged with the original shell.  If the original shell is open, then faces
 *	are constructed along the free edges of the two shells to make a closed shell.
 *
 *	if approximate is non-zero, new vertex geometry at vertices where more than
 *	three faces intersect may be approximated by a point of minimum distance from
 *	the intersecting faces.
 *
 */
void
nmg_hollow_shell( s , thick , approximate , tol )
struct shell *s;
CONST fastf_t thick;
CONST int approximate;
CONST struct bn_tol *tol;
{
	struct nmgregion *new_r,*old_r;
	struct vertexuse *vu;
	struct edgeuse *eu;
	struct loopuse *lu;
	struct faceuse *fu;
	struct face_g_plane *fg_p;
	struct model *m;
	struct shell *is;	/* inside shell */
	struct shell *s_tmp;
	struct bu_ptbl shells;
	long *flags;
	long **copy_tbl;
	int shell_no;
	int is_void;
	int s_tmp_is_closed;

	if( rt_g.NMG_debug & DEBUG_BASIC )
		bu_log( "nmg_extrude_shell( s=x%x , thick=%f)\n" , s , thick );

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	if( thick < 0.0 )
	{
		bu_log( "nmg_extrude_shell: thickness less than zero not allowed" );
		return;
	}

	if( thick < tol->dist )
	{
		bu_log( "nmg_extrude_shell: thickness less than tolerance not allowed" );
		return;
	}

	m = nmg_find_model( (long *)s );

	/* remember region where this shell came from */
	old_r = s->r_p;
	NMG_CK_REGION( old_r );

	/* move this shell to another region */
	new_r = nmg_mrsv( m );
	s_tmp = BU_LIST_FIRST( shell , &new_r->s_hd );
	(void)nmg_mv_shell_to_region( s , new_r );

	/* decompose this shell */
	(void)nmg_decompose_shell( s , tol );

	/* kill the extra shell created by nmg_mrsv above */
	(void)nmg_ks( s_tmp );

	/* recompute the bounding boxes */
	nmg_region_a( new_r , tol );

	/* make a list of all the shells in the new region */
	bu_ptbl_init( &shells , 64, " &shells ");
	for( BU_LIST_FOR( s_tmp , shell , &new_r->s_hd ) )
		bu_ptbl_ins( &shells , (long *)s_tmp );

	/* extrude a copy of each shell, one at a time */
	for( shell_no=0 ; shell_no<BU_PTBL_END( &shells ) ; shell_no ++ )
	{
		s_tmp = (struct shell *)BU_PTBL_GET( &shells , shell_no );

		/* first make a copy of this shell */
		is = nmg_dup_shell( s_tmp , &copy_tbl, tol );

		/* make a translation table for this model */
		flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_extrude_shell flags" );

		/* now adjust all the planes, first move them inward by distance "thick" */
		for( BU_LIST_FOR( fu , faceuse , &is->fu_hd ) )
		{
			NMG_CK_FACEUSE( fu );
			NMG_CK_FACE( fu->f_p );
			fg_p = fu->f_p->g.plane_p;
			NMG_CK_FACE_G_PLANE( fg_p );

			/* move the faces by the distance "thick" */
			if( NMG_INDEX_TEST_AND_SET( flags , fg_p ) )
			{
				if( fu->f_p->flip )
					fg_p->N[3] += thick;
				else
					fg_p->N[3] -= thick;
			}
		}

		/* Reverse the normals of all the faces */
		nmg_invert_shell( is , tol );

		is_void = nmg_shell_is_void( is );

		/* now start adjusting the vertices
		 * Use the original shell so that we can pass the original vertex to nmg_inside_vert
		 */
		for( BU_LIST_FOR( fu , faceuse , &s_tmp->fu_hd ) )
		{
			if( fu->orientation != OT_SAME )
				continue;

			for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
				{
					/* the vertex in a loop of one vertex
					 * must show up in an edgeuse somewhere,
					 * so don't mess with it here */
					continue;
				}
				else
				{
					for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
					{
						struct vertex *new_v;

						NMG_CK_EDGEUSE( eu );
						vu = eu->vu_p;
						NMG_CK_VERTEXUSE( vu );
						new_v = NMG_INDEX_GETP( vertex , copy_tbl , vu->v_p );
						NMG_CK_VERTEX( new_v )
						if( NMG_INDEX_TEST_AND_SET( flags , new_v ) )
						{
							/* move this vertex */
							if( nmg_in_vert( new_v , approximate , tol ) )
								rt_bomb( "Failed to get a new point from nmg_inside_vert\n" );
						}
					}
				}
			}
		}

		/* recompute the bounding boxes */
		nmg_region_a( is->r_p , tol );

		nmg_vmodel( m );

		s_tmp_is_closed = nmg_check_closed_shell( s_tmp , tol );
		if( s_tmp_is_closed )
			is = nmg_extrude_cleanup( is , is_void , tol );

		/* Inside shell is done */
		if( is )
		{
			if( s_tmp_is_closed )
			{
				if( !nmg_check_closed_shell( is , tol ) )
				{
					bu_log( "nmg_extrude_shell: inside shell is not closed, calling nmg_close_shell\n" );
					nmg_close_shell( is );
				}

				nmg_shell_coplanar_face_merge( is , tol , 0 );
				nmg_simplify_shell( is );

				/* now merge the inside and outside shells */
				nmg_js( s_tmp , is , tol );
			}
			else
			{
				if( nmg_check_closed_shell( is , tol ) )
				{
					bu_log( "nmg_extrude_shell: inside shell is closed, outer isn't!!\n" );
					nmg_shell_coplanar_face_merge( is , tol , 0 );
					nmg_simplify_shell( is );
					nmg_js( s_tmp , is , tol );
				}
				else
				{
					/* connect the boundaries of the two open shells */
					nmg_open_shells_connect( s_tmp , is , copy_tbl , tol );
				}
			}
		}

		/* recompute the bounding boxes */
		nmg_region_a( s_tmp->r_p , tol );

		/* free memory */
		rt_free( (char *)flags , "nmg_extrude_shell: flags" );
		rt_free( (char *)copy_tbl , "nmg_extrude_shell: copy_tbl" );
	}

	/* put it all back together */
	for( shell_no=0 ; shell_no<BU_PTBL_END( &shells ) ; shell_no++ )
	{
		struct shell *s2;

		s2 = (struct shell *)BU_PTBL_GET( &shells , shell_no );
		if( s2 != s )
			nmg_js( s , s2 , tol );
	}

	bu_ptbl_free( &shells );

	(void)nmg_mv_shell_to_region( s , old_r );
	nmg_kr( new_r );
}

/*	N M G _ E X T R U D E _ S H E L L
 *
 * Extrudes a shell (s) by a distance (dist) in the direction
 * of the face normals if normal_ward, or the opposite direction
 * if !normal_ward.  The shell (s) is modified by adjusting the
 * plane equations for each face and calculating new vertex geometry.
 * if approximate is non-zero, new vertex geometry, for vertices
 * where more than three faces intersect, will be approximated
 * by a point with minimum distance from the intersecting faces.
 * if approximate is zero, additional faces and/or edges may be added to the shell.
 *
 * returns:
 *	a pointer to the modified shell on success
 *	NULL on failure
 */
struct shell *
nmg_extrude_shell( s , dist , normal_ward , approximate , tol )
struct shell *s;
CONST fastf_t dist;
CONST int normal_ward;
CONST int approximate;
CONST struct bn_tol *tol;
{
	fastf_t thick;
	int along_normal;
	struct model *m;
	struct nmgregion *new_r,*old_r;
	struct shell *s_tmp,*s2;
	struct bu_ptbl shells;
	struct bu_ptbl verts;
	int shell_no;
	int failed=0;

	NMG_CK_SHELL( s );
	BN_CK_TOL( tol );

	if( NEAR_ZERO( dist , tol->dist ) )
	{
		bu_log( "nmg_extrude_shell: Cannot extrude a distance less than tolerance distance\n" );
		return( s );
	}

	along_normal = normal_ward;
	if( dist < 0.0 )
	{
		thick = (-dist);
		along_normal = (!normal_ward);
	}
	else
		thick = dist;

	m = nmg_find_model( &s->l.magic );
	NMG_CK_MODEL( m );

	old_r = s->r_p;
	NMG_CK_REGION( old_r );

	/* decompose this shell and extrude each piece seperately */
	new_r = nmg_mrsv( m );
	s_tmp = BU_LIST_FIRST( shell , &new_r->s_hd );
	(void)nmg_mv_shell_to_region( s , new_r );
	(void)nmg_decompose_shell( s , tol );

	/* kill the not-needed shell created by nmg_mrsv() */
	(void)nmg_ks( s_tmp );

	/* recompute the bounding boxes */
	nmg_region_a( new_r , tol );

	/* make a list of all the shells to be extruded */
	bu_ptbl_init( &shells , 64, " &shells ");
	for( BU_LIST_FOR( s_tmp , shell , &new_r->s_hd ) )
		bu_ptbl_ins( &shells , (long *)s_tmp );

	bu_ptbl_init( &verts , 64, " &verts ");

	/* extrude each shell */
	for( shell_no=0 ; shell_no < BU_PTBL_END( &shells ) ; shell_no++ )
	{
		int vert_no;
		int is_void;
		long *flags;
		struct faceuse *fu;

		s_tmp = (struct shell *)BU_PTBL_GET( &shells , shell_no );
		NMG_CK_SHELL( s_tmp );

		is_void = nmg_shell_is_void( s_tmp );

		/* make a translation table for this model */
		flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "nmg_extrude_shell flags" );

		/* now adjust all the planes, first move them by distance "thick" */
		for( BU_LIST_FOR( fu , faceuse , &s_tmp->fu_hd ) )
		{
			struct face_g_plane *fg_p;

			NMG_CK_FACEUSE( fu );
			NMG_CK_FACE( fu->f_p );
			fg_p = fu->f_p->g.plane_p;
			NMG_CK_FACE_G_PLANE( fg_p );

			/* move the faces by the distance "thick" */
			if( NMG_INDEX_TEST_AND_SET( flags , fg_p ) )
			{
				if( along_normal ^ fu->f_p->flip )
					fg_p->N[3] += thick;
				else
					fg_p->N[3] -= thick;
			}
		}

		rt_free( (char *)flags , "nmg_extrude_shell flags" );

		/* get table of vertices in this shell */
		nmg_vertex_tabulate( &verts , &s_tmp->l.magic );

		/* now move all the vertices */
		for( vert_no = 0 ; vert_no < BU_PTBL_END( &verts ) ; vert_no++ )
		{
			struct vertex *new_v;

			new_v = (struct vertex *)BU_PTBL_GET( &verts , vert_no );
			NMG_CK_VERTEX( new_v );

			if( nmg_in_vert( new_v , approximate , tol ) )
			{
				bu_log( "nmg_extrude_shell: Failed to calculate new vertex at v=x%x was ( %f %f %f )\n",
					new_v , V3ARGS( new_v->vg_p->coord ) );
				failed = 1;
				goto out;
			}
		}

		bu_ptbl_free( &verts );

		if( approximate )	/* need to recalculate plane eqns */
		{
			for( BU_LIST_FOR( fu , faceuse , &s_tmp->fu_hd ) )
			{
				struct loopuse *lu;
				int got_plane=0;

				if( fu->orientation != OT_SAME )
					continue;

				for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
				{
					fastf_t area;
					plane_t pl;

					if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
						continue;

					if( lu->orientation != OT_SAME )
						continue;

					area = nmg_loop_plane_area( lu , pl );

					if( area > 0.0 )
					{
						nmg_face_g( fu , pl );
						got_plane = 1;
						break;
					}
				}
				if( !got_plane )
				{
					bu_log( "nmg_extrude_shell: Cannot recalculate plane for face:\n" );
					nmg_pr_fu_briefly( fu , (char *)NULL );
					failed = 1;
					goto out;
				}
			}
		}

		/* recompute the bounding boxes */
		nmg_region_a( s_tmp->r_p , tol );

		(void)nmg_extrude_cleanup( s_tmp , is_void , tol );
	}

out:
	bu_ptbl_free( &shells );

	/* put it all back together */
	if( BU_LIST_NON_EMPTY( &new_r->s_hd ) )
	{
		s_tmp = BU_LIST_FIRST( shell , &new_r->s_hd );
		s2 = BU_LIST_PNEXT( shell , &s_tmp->l );
		while( BU_LIST_NOT_HEAD( s2 , &new_r->s_hd ) )
		{
			struct shell *next_s;

			next_s = BU_LIST_PNEXT( shell , &s2->l );
			nmg_js( s_tmp , s2 );

			s2 = next_s;
		}
	}
	else
		s_tmp = (struct shell *)NULL;

	if( s_tmp )
		(void)nmg_mv_shell_to_region( s_tmp , old_r );

	nmg_kr( new_r );

	if( failed )
		return( (struct shell *)NULL );
	else
		return( s_tmp );
}
