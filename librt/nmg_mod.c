/*
 *			N M G _ M O D . C
 *
 *  Routines for modifying n-Manifold Geometry data structures.
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1991 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"

/*
 *			N M G _ M E R G E _ R E G I O N S
 */
void
nmg_merge_regions( r1, r2, tol )
struct nmgregion *r1;
struct nmgregion *r2;
CONST struct bn_tol *tol;
{
	struct model *m;

	NMG_CK_REGION( r1 );
	NMG_CK_REGION( r2 );
	BN_CK_TOL( tol );

	m = r1->m_p;
	NMG_CK_MODEL( m );

	if( r2->m_p != m )
		rt_bomb( "nmg_merge_regions: Tried to merge regions from different models!!" );

	/* move all of r2's faces into r1 */
	while( BU_LIST_NON_EMPTY( &r2->s_hd ) )
	{
		struct shell *s;

		s = BU_LIST_FIRST( shell, &r2->s_hd );
		BU_LIST_DEQUEUE( &s->l );
		s->r_p = r1;
		BU_LIST_APPEND( &r1->s_hd, &s->l );
	}

	(void)nmg_kr( r2 );
	nmg_rebound( m, tol );
}

/************************************************************************
 *									*
 *				SHELL Routines				*
 *									*
 ************************************************************************/

/*
 *			N M G _ S H E L L _ C O P L A N A R _ F A C E _ M E R G E
 *
 *  A geometric routine to
 *  find all pairs of faces in a shell that have the same plane equation
 *  (to within the given tolerance), and combine them into a single face.
 *
 *  Note that this may result in some of the verticies being very slightly
 *  off the plane equation, but the geometry routines need to be prepared
 *  for this in any case.
 *  If the "simplify" flag is set, pairs of loops in the face that touch
 *  will be combined into a single loop where possible.
 *
 *  XXX Perhaps should be recast as "nmg_shell_shared_face_merge()", leaving
 *  XXX all the geometric calculations to the code in nmg_fuse.c ?
 */
void
nmg_shell_coplanar_face_merge( s, tol, simplify )
struct shell		*s;
CONST struct bn_tol	*tol;
CONST int		simplify;
{
	struct model	*m;
	int		len;
	char		*flags1;
	char		*flags2;
	struct faceuse	*fu1;
	struct faceuse	*fu2;
	struct face	*f1;
	struct face	*f2;
	struct face_g_plane	*fg1;
	struct face_g_plane	*fg2;

	NMG_CK_SHELL(s);
	m = nmg_find_model( &s->l.magic );
	len = sizeof(char) * m->maxindex * 2;
	flags1 = (char *)rt_calloc( sizeof(char), m->maxindex * 2,
		"nmg_shell_coplanar_face_merge flags1[]" );
	flags2 = (char *)rt_calloc( sizeof(char), m->maxindex * 2,
		"nmg_shell_coplanar_face_merge flags2[]" );

	/* Visit each face in the shell */
	for( BU_LIST_FOR( fu1, faceuse, &s->fu_hd ) )  {
		plane_t		n1;

		if( BU_LIST_NEXT_IS_HEAD(fu1, &s->fu_hd) )  break;
		f1 = fu1->f_p;
		NMG_CK_FACE(f1);
		if( NMG_INDEX_TEST(flags1, f1) )  continue;
		NMG_INDEX_SET(flags1, f1);

		fg1 = f1->g.plane_p;
		NMG_CK_FACE_G_PLANE(fg1);
		NMG_GET_FU_PLANE( n1, fu1 );

		/* For this face, visit all remaining faces in the shell. */
		/* Don't revisit any faces already considered. */
		bcopy( flags1, flags2, len );
		for( fu2 = BU_LIST_NEXT(faceuse, &fu1->l);
		     BU_LIST_NOT_HEAD(fu2, &s->fu_hd);
		     fu2 = BU_LIST_NEXT(faceuse,&fu2->l)
		)  {
			register fastf_t	dist;
			plane_t			n2;

			f2 = fu2->f_p;
			NMG_CK_FACE(f2);
			if( NMG_INDEX_TEST(flags2, f2) )  continue;
			NMG_INDEX_SET(flags2, f2);

			fg2 = f2->g.plane_p;
			NMG_CK_FACE_G_PLANE(fg2);

			if( fu2->fumate_p == fu1 || fu1->fumate_p == fu2 )
				rt_bomb("nmg_shell_coplanar_face_merge() mate confusion\n");

			/* See if face geometry is shared & same direction */
			if( fg1 != fg2 || f1->flip != f2->flip )  {
				/* If plane equations are different, done */
				NMG_GET_FU_PLANE( n2, fu2 );

				/* Compare distances from origin */
				dist = n1[3] - n2[3];
				if( !NEAR_ZERO(dist, tol->dist) ) continue;

				/*
				 *  Compare angle between normals.
				 *  Can't just use BN_VECT_ARE_PARALLEL here,
				 *  because they must point in the same direction.
				 */
				dist = VDOT( n1, n2 );
				if( !(dist >= tol->para) ) continue;

				if( nmg_ck_fu_verts( fu2, f1, tol ) )
					continue;
			}

			/*
			 * Plane equations are the same, within tolerance,
			 * or by shared fg topology.
			 * Move everything into fu1, and
			 * kill now empty faceuse, fumate, and face
			 */
			{
				struct faceuse	*prev_fu;
				prev_fu = BU_LIST_PREV(faceuse, &fu2->l);
				/* The prev_fu can never be the head */
				if( BU_LIST_IS_HEAD(prev_fu, &s->fu_hd) )
					rt_bomb("prev is head?\n");

				nmg_jf( fu1, fu2 );

				fu2 = prev_fu;
			}

			/* There is now the option of simplifying the face,
			 * by removing unnecessary edges.
			 */
			if( simplify )  {
				struct loopuse *lu;

				for (BU_LIST_FOR(lu, loopuse, &fu1->lu_hd))
					nmg_simplify_loop(lu);
			}
		}
	}
	rt_free( (char *)flags1, "nmg_shell_coplanar_face_merge flags1[]" );
	rt_free( (char *)flags2, "nmg_shell_coplanar_face_merge flags2[]" );

	nmg_shell_a( s, tol );

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_shell_coplanar_face_merge(s=x%x, tol=x%x, simplify=%d)\n",
			s, tol, simplify);
	}
}

/*
 *			N M G _ S I M P L I F Y _ S H E L L
 *
 *  Simplify all the faces in this shell, where possible.
 *  Under some circumstances this may result in an empty shell as a result.
 *
 * Returns -
 *	0	If all was OK
 *	1	If shell is now empty
 */
int
nmg_simplify_shell(s)
struct shell *s;
{
	struct faceuse *fu;
	int ret_val;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		if( nmg_simplify_face(fu) )  {
			struct faceuse	*kfu = fu;
			fu = BU_LIST_PREV( faceuse, &fu->l );
			nmg_kfu( kfu );
		}
	}

	ret_val = nmg_shell_is_empty(s);

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_simplify_shell(s=x%x) returns %d\n", s , ret_val);
	}

	return( ret_val );
}

/*
 *			N M G _ R M _ R E D U N D A N C I E S
 *
 *  Remove all redundant parts between the different "levels" of a shell.
 *  Remove wire loops that match face loops.
 *  Remove wire edges that match edges in wire loops or face loops.
 *  Remove lone vertices (stored as wire loops on a single vertex) that
 *  match vertices in a face loop, wire loop, or wire edge.
 */
void
nmg_rm_redundancies(s, tol)
struct shell	*s;
CONST struct bn_tol *tol;
{
	struct faceuse	*fu;
	struct loopuse	*lu;
	struct edgeuse	*eu;
	struct vertexuse	*vu;
	long		magic1;

	NMG_CK_SHELL(s);
	BN_CK_TOL( tol );

	if( BU_LIST_NON_EMPTY( &s->fu_hd ) )  {
		/* Compare wire loops -vs- loops in faces */
		lu = BU_LIST_FIRST( loopuse, &s->lu_hd );
		while( BU_LIST_NOT_HEAD( lu, &s->lu_hd ) )  {
			register struct loopuse	*nextlu;
			NMG_CK_LOOPUSE(lu);
			NMG_CK_LOOP(lu->l_p);
			nextlu = BU_LIST_PNEXT( loopuse, lu );
			if( nmg_is_loop_in_facelist( lu->l_p, &s->fu_hd ) )  {
				/* Dispose of wire loop (and mate)
				 * which match face loop */
				if( nextlu == lu->lumate_p )
					nextlu = BU_LIST_PNEXT(loopuse, nextlu);
				nmg_klu( lu );
			}
			lu = nextlu;
		}

		/* Compare wire edges -vs- edges in loops in faces */
		eu = BU_LIST_FIRST( edgeuse, &s->eu_hd );
		while( BU_LIST_NOT_HEAD( eu, &s->eu_hd ) )  {
			register struct edgeuse *nexteu;
			NMG_CK_EDGEUSE(eu);
			NMG_CK_EDGE(eu->e_p);
			nexteu = BU_LIST_PNEXT( edgeuse, eu );
			if( nmg_is_edge_in_facelist( eu->e_p, &s->fu_hd ) )  {
				/* Dispose of wire edge (and mate) */
				if( nexteu == eu->eumate_p )
					nexteu = BU_LIST_PNEXT(edgeuse, nexteu);
				(void)nmg_keu(eu);
			}
			eu = nexteu;
		}
	}

	/* Compare wire edges -vs- edges in wire loops */
	eu = BU_LIST_FIRST( edgeuse, &s->eu_hd );
	while( BU_LIST_NOT_HEAD( eu, &s->eu_hd ) )  {
		register struct edgeuse *nexteu;
		NMG_CK_EDGEUSE(eu);
		NMG_CK_EDGE(eu->e_p);
		nexteu = BU_LIST_PNEXT( edgeuse, eu );
		if( nmg_is_edge_in_looplist( eu->e_p, &s->lu_hd ) )  {
			/* Kill edge use and mate */
			if( nexteu == eu->eumate_p )
				nexteu = BU_LIST_PNEXT(edgeuse, nexteu);
			(void)nmg_keu(eu);
		}
		eu = nexteu;
	}

	/* Compare lone vertices against everything else */
	/* Individual vertices are stored as wire loops on a single vertex */
	lu = BU_LIST_FIRST( loopuse, &s->lu_hd );
	while( BU_LIST_NOT_HEAD( lu, &s->lu_hd ) )  {
		register struct loopuse	*nextlu;
		NMG_CK_LOOPUSE(lu);
		nextlu = BU_LIST_PNEXT( loopuse, lu );
		magic1 = BU_LIST_FIRST_MAGIC( &lu->down_hd );
		if( magic1 != NMG_VERTEXUSE_MAGIC )  {
			lu = nextlu;
			continue;
		}
		vu = BU_LIST_PNEXT( vertexuse, &lu->down_hd );
		NMG_CK_VERTEXUSE(vu);
		NMG_CK_VERTEX(vu->v_p);
		if( nmg_is_vertex_in_facelist( vu->v_p, &s->fu_hd ) ||
		    nmg_is_vertex_in_looplist( vu->v_p, &s->lu_hd,0 ) ||
		    nmg_is_vertex_in_edgelist( vu->v_p, &s->eu_hd ) )  {
		    	/* Kill lu and mate */
			if( nextlu == lu->lumate_p )
				nextlu = BU_LIST_PNEXT(loopuse, nextlu);
			nmg_klu( lu );
			lu = nextlu;
			continue;
		}
		lu = nextlu;
	}

	/* There really shouldn't be a lone vertex by now */
	if( s->vu_p )  bu_log("nmg_rm_redundancies() lone vertex?\n");

	/* get rid of matching OT_SAME/OT_OPPOSITE loops in same faceuse */
	fu = BU_LIST_FIRST( faceuse, &s->fu_hd );
	while( BU_LIST_NOT_HEAD( &fu->l, &s->fu_hd ) )
	{
		struct faceuse *next_fu;
		int lu1_count;
		int lu_count;

		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
		{
			fu = BU_LIST_NEXT( faceuse, &fu->l );
			continue;
		}

		next_fu = BU_LIST_NEXT( faceuse, &fu->l );
		if( next_fu == fu->fumate_p )
			next_fu = BU_LIST_NEXT( faceuse, &next_fu->l );

		lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
		while( BU_LIST_NOT_HEAD( &lu->l, &fu->lu_hd ) )
		{
			struct loopuse *next_lu;
			struct loopuse *lu1;
			struct edgeuse *eu;

			NMG_CK_LOOPUSE( lu );

			next_lu = BU_LIST_NEXT( loopuse, &lu->l );

			if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			{
				lu = next_lu;
				continue;
			}

			/* count edges in lu */
			lu_count = 0;
			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				lu_count++;

			/* look for anther loopuse with opposite orientation */
			lu1 = BU_LIST_FIRST( loopuse, &fu->lu_hd );
			while( BU_LIST_NOT_HEAD( &lu1->l, &fu->lu_hd ) )
			{
				struct loopuse *next_lu1;
				int found;

				NMG_CK_LOOPUSE( lu1 );

				next_lu1 = BU_LIST_PNEXT( loopuse, &lu1->l );

				if( BU_LIST_FIRST_MAGIC( &lu1->down_hd ) != NMG_EDGEUSE_MAGIC )
				{
					lu1 = next_lu1;
					continue;
				}

				if( lu1 == lu )
				{
					lu1 = next_lu1;
					continue;
				}

				if( lu1->orientation == lu->orientation )
				{
					lu1 = next_lu1;
					continue;
				}

				/* count edges in lu1 */
				lu1_count = 0;
				for( BU_LIST_FOR( eu, edgeuse, &lu1->down_hd ) )
					lu1_count++;

				if( lu_count != lu1_count )
				{
					lu1 = next_lu1;
					continue;
				}

				if( nmg_classify_lu_lu( lu, lu1, tol ) != NMG_CLASS_AonBshared )
				{
					lu1 = next_lu1;
					continue;
				}

				/* lu and lu1 are identical, but with opposite orientations
				 * kill them both
				 */

				if( next_lu1 == lu )
					next_lu1 = BU_LIST_NEXT( loopuse, &next_lu1->l );

				if( next_lu == lu1 )
					next_lu = BU_LIST_NEXT( loopuse, &next_lu->l );

				(void)nmg_klu( lu );
				if( nmg_klu( lu1 ) )
				{
					/* faceuse is empty, kill it */
					if( nmg_kfu( fu ) )
					{
						/* shell is empty, set "nexts" to get out */
						next_fu = (struct faceuse *)(&s->fu_hd);
						next_lu = (struct loopuse *)NULL;
					}
					else
					{
						/* shell not empty, but fu is */
						next_lu = (struct loopuse *)NULL;
					}
				}
				break;
			}

			if( !next_lu )
				break;

			lu = next_lu;
		}
		fu = next_fu;
	}

	/* get rid of redundant loops in same fu (OT_SAME within an OT_SAME), etc. */
	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			continue;

		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		{
			struct loopuse *lu1;

			NMG_CK_LOOPUSE( lu );

			/* look for another loop with same orientation */
			lu1 = BU_LIST_FIRST( loopuse, &fu->lu_hd );
			while( BU_LIST_NOT_HEAD( &lu1->l, &fu->lu_hd ) )
			{
				struct loopuse *next_lu;
				struct loopuse *lu2;
				int found;

				NMG_CK_LOOPUSE( lu1 );

				next_lu = BU_LIST_PNEXT( loopuse, &lu1->l );

				if( lu1 == lu )
				{
					lu1 = next_lu;
					continue;
				}

				if( lu1->orientation != lu->orientation )
				{
					lu1 = next_lu;
					continue;
				}

				if( nmg_classify_lu_lu( lu1, lu, tol ) != NMG_CLASS_AinB )
				{
					lu1 = next_lu;
					continue;
				}

				/* lu1 is within lu and has same orientation.
				 * Check if there is a loop with opposite
				 * orientation between them.
				 */
				found = 0;
				for( BU_LIST_FOR( lu2, loopuse, &fu->lu_hd ) )
				{
					int class1,class2;

					NMG_CK_LOOPUSE( lu2 );

					if( lu2 == lu || lu2 == lu1 )
						continue;

					if( lu2->orientation == lu->orientation )
						continue;

					class1 = nmg_classify_lu_lu( lu2, lu, tol );
					class2 = nmg_classify_lu_lu( lu1, lu2, tol );

					if( class1 == NMG_CLASS_AinB &&
					    class2 == NMG_CLASS_AinB )
					{
						found = 1;
						break;
					}
				}
				if( !found )
				{
					/* lu1 is a redundant loop */
					(void) nmg_klu( lu1 );
				}
				lu1 = next_lu;
			}
		}
	}

	/* get rid of redundant loops in same fu where there are two identical
	 * loops, but with opposite orientation
	 */
	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		if( fu->orientation != OT_SAME )
			continue;

		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		{
			struct loopuse *lu1;

			if( lu->orientation != OT_SAME )
				continue;

			lu1 = BU_LIST_FIRST( loopuse, &fu->lu_hd );
			while( BU_LIST_NOT_HEAD( &lu1->l, &fu->lu_hd ) )
			{
				int class;
				struct loopuse *next_lu;

				next_lu = BU_LIST_PNEXT( loopuse, &lu1->l );

				if( lu1 == lu || lu1->orientation != OT_OPPOSITE )
				{
					lu1 = next_lu;
					continue;
				}

				class = nmg_classify_lu_lu( lu, lu1, tol );

				if( class == NMG_CLASS_AonBshared )
				{
					nmg_klu( lu1 ); /* lu1 is redudndant */
				}

				lu1 = next_lu;
			}
		}
	}

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_rm_redundancies(s=x%x)\n", s);
	}
}

/*
 *			N M G _ S A N I T I Z E _ S _ L V
 *
 *	Remove those pesky little vertex-only loops of orientation "orient".
 *	Typically these will be OT_BOOLPLACE markers created in the
 *	process of doing intersections for the boolean operations.
 */
void
nmg_sanitize_s_lv(s, orient)
struct shell	*s;
int		orient;
{
	struct faceuse *fu;
	struct loopuse *lu;
	pointp_t pt;

	NMG_CK_SHELL(s);

	/* sanitize the loop lists in the faces of the shell */
	fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
	while (BU_LIST_NOT_HEAD(fu, &s->fu_hd) ) {

		/* all of those vertex-only/orient loops get deleted
		 */
		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		while (BU_LIST_NOT_HEAD(lu, &fu->lu_hd)) {
			if (lu->orientation == orient) {
				lu = BU_LIST_PNEXT(loopuse,lu);
				nmg_klu(BU_LIST_PLAST(loopuse, lu));
			} else if (lu->orientation == OT_UNSPEC &&
			    BU_LIST_FIRST_MAGIC(&lu->down_hd) ==
			    NMG_VERTEXUSE_MAGIC) {
				register struct vertexuse *vu;
				vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
				pt = vu->v_p->vg_p->coord;
				VPRINT("nmg_sanitize_s_lv() OT_UNSPEC at", pt);
				lu = BU_LIST_PNEXT(loopuse,lu);
			} else {
				lu = BU_LIST_PNEXT(loopuse,lu);
			}
		}

		/* step forward, avoiding re-processing our mate (which would
		 * have had it's loops processed with ours)
		 */
		if (BU_LIST_PNEXT(faceuse, fu) == fu->fumate_p)
			fu = BU_LIST_PNEXT_PNEXT(faceuse, fu);
		else
			fu = BU_LIST_PNEXT(faceuse, fu);

		/* If previous faceuse has no more loops, kill it */
		if (BU_LIST_IS_EMPTY( &(BU_LIST_PLAST(faceuse, fu))->lu_hd )) {
			/* All of the loopuses of the previous face were
			 * BOOLPLACE's so the face will now go away
			 */
			nmg_kfu(BU_LIST_PLAST(faceuse, fu));
		}
	}

	/* Sanitize any wire/vertex loops in the shell */
	lu = BU_LIST_FIRST(loopuse, &s->lu_hd);
	while (BU_LIST_NOT_HEAD(lu, &s->lu_hd) ) {
		if (lu->orientation == orient) {
			lu = BU_LIST_PNEXT(loopuse,lu);
			nmg_klu(BU_LIST_PLAST(loopuse, lu));
		} else if (lu->orientation == OT_UNSPEC &&
		    BU_LIST_FIRST_MAGIC(&lu->down_hd) ==
		    NMG_VERTEXUSE_MAGIC) {
			register struct vertexuse *vu;
			vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
			pt = vu->v_p->vg_p->coord;
			VPRINT("nmg_sanitize_s_lv() OT_UNSPEC at", pt);
			lu = BU_LIST_PNEXT(loopuse,lu);
		} else {
			lu = BU_LIST_PNEXT(loopuse,lu);
		}
	}

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_sanitize_s_lv(s=x%x, orient=%d)\n",	s, orient);
	}
}

/*
 *			N M G _ S _ S P L I T _ T O U C H I N G L O O P S
 *
 *  For every loop in a shell, invoke nmg_split_touchingloops() on it.
 *
 *  Needed before starting classification, to separate interior (touching)
 *  loop segments into true interior loops, so each can be processed
 *  separately.
 */
void
nmg_s_split_touchingloops(s, tol)
struct shell		*s;
CONST struct bn_tol	*tol;
{
	struct faceuse	*fu;
	struct loopuse	*lu;

	NMG_CK_SHELL(s);
	BN_CK_TOL(tol);

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_s_split_touching_loops(s=x%x, tol=x%x) START\n", s, tol);
	}

	/* First, handle any splitting */
	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
			(void)nmg_loop_split_at_touching_jaunt( lu, tol );
			nmg_split_touchingloops( lu, tol );
		}
	}
	for( BU_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
		(void)nmg_loop_split_at_touching_jaunt( lu, tol );
		nmg_split_touchingloops( lu, tol );
	}

	/* Second, reorient any split loop fragments */
	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
			if( lu->orientation != OT_UNSPEC )  continue;
			nmg_lu_reorient( lu );
		}
	}

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_s_split_touching_loops(s=x%x, tol=x%x) END\n", s, tol);
	}
}

/*
 *			N M G _ S _ J O I N _ T O U C H I N G L O O P S
 *
 *  For every loop in a shell, invoke nmg_join_touchingloops() on it.
 */
void
nmg_s_join_touchingloops(s, tol)
struct shell		*s;
CONST struct bn_tol	*tol;
{
	struct faceuse	*fu;
	struct loopuse	*lu;

	NMG_CK_SHELL(s);
	BN_CK_TOL(tol);

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_s_join_touching_loops(s=x%x, tol=x%x) START\n", s, tol);
	}

	/* First, handle any joining */
	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
			nmg_join_touchingloops( lu );
		}
	}
	for( BU_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
		nmg_join_touchingloops( lu );
	}

	/* Second, reorient any disoriented loop fragments */
	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
			if( lu->orientation != OT_UNSPEC )  continue;
			nmg_lu_reorient( lu );
		}
	}

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_s_join_touching_loops(s=x%x, tol=x%x) END\n", s, tol);
	}
}

/*
 *			N M G _ J S
 *
 *  Join two shells into one.
 *  This is mostly an up-pointer re-labeling activity, as it is left up to
 *  the caller to ensure that there are no non-explicit intersections.
 *
 *  Upon return, s2 will no longer exist.
 *
 *  The 'tol' arg is used strictly for printing purposes.
 */
void
nmg_js( s1, s2, tol )
register struct shell	*s1;		/* destination */
register struct shell	*s2;		/* source */
CONST struct bn_tol	*tol;
{
	struct faceuse	*fu2;
	struct faceuse	*nextfu;
	struct loopuse	*lu;
	struct loopuse	*nextlu;
	struct edgeuse	*eu;
	struct edgeuse	*nexteu;
	struct vertexuse *vu;

	NMG_CK_SHELL(s1);
	NMG_CK_SHELL(s2);
	BN_CK_TOL(tol);

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_js(s1=x%x, s2=x%x) START\n", s1, s2);
	}

	if( rt_g.NMG_debug & DEBUG_VERIFY )
		nmg_vshell( &s1->r_p->s_hd, s1->r_p );

	/*
	 *  For each face in the shell, process all the loops in the face,
	 *  and then handle the face and all loops as a unit.
	 */
	fu2 = BU_LIST_FIRST( faceuse, &s2->fu_hd );
	while( BU_LIST_NOT_HEAD( fu2, &s2->fu_hd ) )  {
		struct faceuse	*fu1;

		nextfu = BU_LIST_PNEXT( faceuse, fu2 );

		/* Faceuse mates will be handled at same time as OT_SAME fu */
		if( fu2->orientation != OT_SAME )  {
			fu2 = nextfu;
			continue;
		}

		/* Consider this face */
		NMG_CK_FACEUSE(fu2);
		NMG_CK_FACE(fu2->f_p);

		if( nextfu == fu2->fumate_p )
			nextfu = BU_LIST_PNEXT(faceuse, nextfu);

		/* If there is a face in the destination shell that
		 * shares face geometry with this face, then
		 * move all the loops into the other face,
		 * and eliminate this redundant face.
		 */
		fu1 = nmg_find_fu_with_fg_in_s( s1, fu2 );
		if( fu1 && fu1->orientation == OT_SAME )  {
			if (rt_g.NMG_debug & DEBUG_BASIC)
				bu_log("nmg_js(): shared face_g_plane, doing nmg_jf()\n");
			nmg_jf( fu1, fu2 );
			/* fu2 pointer is invalid here */
			fu2 = fu1;
		} else {
			nmg_mv_fu_between_shells( s1, s2, fu2 );
		}

		fu2 = nextfu;
	}
#if 0
	if( rt_g.NMG_debug & DEBUG_VERIFY )
		nmg_vshell( &s1->r_p->s_hd, s1->r_p );
#endif

	/*
	 *  For each loop in the shell, process.
	 *  Each loop is either a wire-loop, or a vertex-with-self-loop.
	 *  Both get the same treatment.
	 */
	lu = BU_LIST_FIRST( loopuse, &s2->lu_hd );
	while( BU_LIST_NOT_HEAD( lu, &s2->lu_hd ) )  {
		nextlu = BU_LIST_PNEXT( loopuse, lu );

		NMG_CK_LOOPUSE(lu);
		NMG_CK_LOOP( lu->l_p );
		if( nextlu == lu->lumate_p )
			nextlu = BU_LIST_PNEXT(loopuse, nextlu);

		nmg_mv_lu_between_shells( s1, s2, lu );
		lu = nextlu;
	}
#if 0
	if( rt_g.NMG_debug & DEBUG_VERIFY )
		nmg_vshell( &s1->r_p->s_hd, s1->r_p );
#endif

	/*
	 *  For each wire-edge in the shell, ...
	 */
	eu = BU_LIST_FIRST( edgeuse, &s2->eu_hd );
	while( BU_LIST_NOT_HEAD( eu, &s2->eu_hd ) )  {
		nexteu = BU_LIST_PNEXT( edgeuse, eu );

		/* Consider this edge */
		NMG_CK_EDGEUSE(eu);
		NMG_CK_EDGE( eu->e_p );
		if( nexteu == eu->eumate_p )
			nexteu = BU_LIST_PNEXT(edgeuse, nexteu);
		nmg_mv_eu_between_shells( s1, s2, eu );
		eu = nexteu;
	}
#if 0
	if( rt_g.NMG_debug & DEBUG_VERIFY )
		nmg_vshell( &s1->r_p->s_hd, s1->r_p );
#endif

	/*
	 * Final case:  shell of a single vertexuse
	 */
	if( vu = s2->vu_p )  {
		NMG_CK_VERTEXUSE( vu );
		NMG_CK_VERTEX( vu->v_p );
		nmg_mv_vu_between_shells( s1, s2, vu );
		s2->vu_p = (struct vertexuse *)0;	/* sanity */
	}

	if( BU_LIST_NON_EMPTY( &s2->fu_hd ) )  {
		rt_bomb("nmg_js():  s2 still has faces!\n");
	}
	if( BU_LIST_NON_EMPTY( &s2->lu_hd ) )  {
		rt_bomb("nmg_js():  s2 still has wire loops!\n");
	}
	if( BU_LIST_NON_EMPTY( &s2->eu_hd ) )  {
		rt_bomb("nmg_js():  s2 still has wire edges!\n");
	}
	if(s2->vu_p) {
		rt_bomb("nmg_js():  s2 still has verts!\n");
	}

	/* s2 is completely empty now, which is an invalid condition */
	nmg_ks( s2 );

	/* Some edges may need faceuse parity touched up. */
	nmg_s_radial_harmonize( s1, tol );

	if( rt_g.NMG_debug & DEBUG_VERIFY )
		nmg_vshell( &s1->r_p->s_hd, s1->r_p );

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_js(s1=x%x, s2=x%x) END\n", s1, s2);
	}
}

/*
 *			N M G _ I N V E R T _ S H E L L
 *
 *  Reverse the surface normals, and invert the orientation state of
 *  all faceuses in a shell.
 * 
 *  This turns the shell "inside out", such as might be needed for the
 *  right hand side term of a subtraction operation.
 *
 *  While this function is operating, the parity of faceuses radially
 *  around edgeuses is disrupted, hence this atomic interface to
 *  invert the shell.
 *
 *  The 'tol' argument is used strictly for printing.
 */
void
nmg_invert_shell( s, tol )
struct shell		*s;
CONST struct bn_tol	*tol;
{
	struct model	*m;
	struct faceuse	*fu;
	char		*tags;

	NMG_CK_SHELL(s);
	m = s->r_p->m_p;
	NMG_CK_MODEL(m);
	BN_CK_TOL(tol);

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_invert_shell(x%x)\n", s);
	}

	/* Allocate map of faces visited */
	tags = rt_calloc( m->maxindex+1, 1, "nmg_invert_shell() tags[]" );

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		/* By tagging on faces, marks fu and fumate together */
		if( NMG_INDEX_TEST( tags, fu->f_p ) )  continue;
		NMG_INDEX_SET( tags, fu->f_p );
		/* Process fu and fumate together */
		nmg_reverse_face(fu);
	}
	rt_free( tags, "nmg_invert_shell() tags[]" );
}

/************************************************************************
 *									*
 *				FACE Routines				*
 *									*
 ************************************************************************/

/*
 *			N M G _ C M F A C E
 *
 *	Create a face with exactly one exterior loop (and no holes),
 *	given an array of pointers to struct vertex pointers.
 *	Intended to help create a "3-manifold" shell, where
 *	each edge has only two faces alongside of it.
 *	(Shades of winged edges!)
 *
 *	"verts" is an array of "n" pointers to pointers to (struct vertex).
 *	"s" is the parent shell for the new face.
 *
 *	The new face will consist of a single loop
 *	made from n edges between the n vertices.  Before an edge is created
 *	between a pair of verticies, we check to see if there is already an
 *	edge with exactly one edgeuse+mate (in this shell)
 *	that runs between the two verticies.
 *	If such an edge can be found, the newly created edgeuses will just
 *	use the existing edge.
 *	This means that no special call to nmg_gluefaces() is needed later.
 *
 *	If a pointer in verts is a pointer to a null vertex pointer, a new
 *	vertex is created.  In this way, new verticies can be created
 *	conveniently within a user's list of known verticies
 *
 *	verts		pointers to struct vertex	    vertex structs
 *
 *	-------		--------
 *   0	|  +--|-------->|   +--|--------------------------> (struct vertex)
 *	-------		--------	---------
 *   1	|  +--|------------------------>|   +---|---------> (struct vertex)
 *	-------		--------	---------
 *   2	|  +--|-------->|   +--|--------------------------> (struct vertex)
 *	-------		--------
 *  ...
 *	-------				---------
 *   n	|  +--|------------------------>|   +---|---------> (struct vertex)
 *	-------				---------
 *
 *
 *	The vertices *must* be listed in "counter-clockwise" (CCW) order.
 *	This routine makes only topology, without reference to any geometry.
 *
 *	Note that this routine inserts new vertices (by edge use splitting)
 *	at the head of the loop, which reverses the order.
 *	Therefore, the caller's vertices are traversed in reverse order
 *	to counter this behavior, and
 *	to effect the proper vertex order in the final face loop.
 *
 *	Also note that this routine uses one level more of indirection
 *	in the verts[] array than nmg_cface().
 *
 *  Recent improvement: the lu's list of eu's traverses the
 *  verts[] array in order specified by the caller.  Imagine that.
 */
struct faceuse *
nmg_cmface(s, verts, n)
struct shell	*s;
struct vertex	**verts[];
int		n;
{
	struct faceuse *fu;
	struct edgeuse *eu, *eur, *euold;
	struct loopuse	*lu;
	struct vertexuse	*vu;
	int i;

	NMG_CK_SHELL(s);

	if (n < 1) {
		bu_log("nmg_cmface(s=x%x, verts=x%x, n=%d.)\n",
			s, verts, n );
		rt_bomb("nmg_cmface() trying to make bogus face\n");
	}

	/* make sure verts points to some real storage */
	if (!verts) {
		bu_log("nmg_cmface(s=x%x, verts=x%x, n=%d.) null pointer to array start\n",
			s, verts, n );
		rt_bomb("nmg_cmface\n");
	}

	/* validate each of the pointers in verts */
	for (i=0 ; i < n ; ++i) {
		if (verts[i]) {
			if (*verts[i]) {
				/* validate the vertex pointer */
				NMG_CK_VERTEX(*verts[i]);
			}
		} else {
			bu_log("nmg_cmface(s=x%x, verts=x%x, n=%d.) verts[%d]=NULL\n",
				s, verts, n, i );
			rt_bomb("nmg_cmface\n");
		}
	}

	lu = nmg_mlv(&s->l.magic, *verts[0], OT_SAME);
	fu = nmg_mf(lu);
	fu->orientation = OT_SAME;
	fu->fumate_p->orientation = OT_OPPOSITE;
	vu = BU_LIST_FIRST( vertexuse, &lu->down_hd);
	NMG_CK_VERTEXUSE(vu);
	eu = nmg_meonvu(vu);
	NMG_CK_EDGEUSE(eu);

	if (!(*verts[0]))  {
		NMG_CK_VERTEXUSE(eu->vu_p);
		NMG_CK_VERTEX(eu->vu_p->v_p);
		*verts[0] = eu->vu_p->v_p;
	}

	for (i = n-1 ; i > 0 ; i--) {
		/* Get the edgeuse most recently created */
		euold = BU_LIST_FIRST( edgeuse, &lu->down_hd );
		NMG_CK_EDGEUSE(euold);

		if (rt_g.NMG_debug & DEBUG_CMFACE)
			bu_log("nmg_cmface() euold: %8x\n", euold);

		/* look for pre-existing edge between these
		 * verticies
		 */
		if (*verts[i]) {
			/* look for an existing edge to share */
			eur = nmg_findeu(*verts[(i+1)%n], *verts[i], s, euold, 1);
			eu = nmg_eusplit(*verts[i], euold, 0);
			if (eur) {
				nmg_je(eur, eu);

				if (rt_g.NMG_debug & DEBUG_CMFACE)
					bu_log("nmg_cmface() found another edgeuse (%8x) between %8x and %8x\n",
						eur, *verts[(i+1)%n], *verts[i]);
			} else {
				if (rt_g.NMG_debug & DEBUG_CMFACE)
				    bu_log("nmg_cmface() didn't find edge from verts[%d]%8x to verts[%d]%8x\n",
					i+1, *verts[(i+1)%n], i, *verts[i]);
			}
		} else {
			eu = nmg_eusplit(*verts[i], euold, 0);
			*verts[i] = eu->vu_p->v_p;

			if (rt_g.NMG_debug & DEBUG_CMFACE)  {
				bu_log("nmg_cmface() *verts[%d] was null, is now %8x\n",
					i, *verts[i]);
			}
		}
	}

	if( n > 1 )  {
		if (eur = nmg_findeu(*verts[0], *verts[1], s, euold, 1))  {
			nmg_je(eur, euold);
		} else  {
		    if (rt_g.NMG_debug & DEBUG_CMFACE)
			bu_log("nmg_cmface() didn't find edge from verts[%d]%8x to verts[%d]%8x\n",
				0, *verts[0], 1, *verts[1]);
		}
	}

	if(rt_g.NMG_debug & DEBUG_BASIC)  {
		/* Sanity check */
		euold = BU_LIST_FIRST( edgeuse, &lu->down_hd );
		NMG_CK_EDGEUSE(euold);
		if( euold->vu_p->v_p != *verts[0] )  {
			bu_log("ERROR nmg_cmface() lu first vert s/b x%x, was x%x\n",
				*verts[0], euold->vu_p->v_p );
			for( i=0; i < n; i++ )  {
				bu_log("  *verts[%2d]=x%x, eu->vu_p->v_p=x%x\n",
					i, *verts[i], euold->vu_p->v_p);
				euold = BU_LIST_PNEXT_CIRC( edgeuse, &euold->l );
			}
			rt_bomb("nmg_cmface() bogus eu ordering\n");
		}
	}

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_cmface(s=x%x, verts[]=x%x, n=%d) fu=x%x\n",
			s, verts, n, fu);
	}
	return (fu);
}

/*
 *			N M G _ C F A C E
 *
 *	Create a loop within a face, given a list of vertices.
 *
 *	"verts" is an array of "n" pointers to (struct vertex).  "s" is the
 *	parent shell for the new face.  The face will consist of a single loop
 *	made from edges between the n vertices.
 *
 *	If verts is a null pointer (no vertex list), all vertices of the face
 *	will be new points.  Otherwise, verts is a pointer to a list of
 *	vertices to use in creating the face/loop.  Null entries within the
 *	list will cause a new vertex to be created for that point.  Such new
 *	vertices will be inserted into the list for return to the caller.
 *
 *	The vertices should be listed in
 *	"counter-clockwise" (CCW) order if this is an ordinary face (loop),
 *	and in "clockwise" (CW) order if this is an interior
 * 	("hole" or "subtracted") face (loop).
 *	This routine makes only topology, without reference to any geometry.
 *
 *	Note that this routine inserts new vertices (by edge use splitting)
 *	at the head of the loop, which reverses the order.
 *	Therefore, the caller's vertices are traversed in reverse order
 *	to counter this behavior, and
 *	to effect the proper vertex order in the final face loop.
 */
struct faceuse *
nmg_cface(s, verts, n)
struct shell *s;
struct vertex *verts[];
int n;
{
	struct faceuse *fu;
	struct edgeuse *eu;
	struct loopuse	*lu;
	struct vertexuse *vu;
	int i;

	NMG_CK_SHELL(s);
	if (n < 1) {
		bu_log("nmg_cface(s=x%x, verts=x%x, n=%d.)\n",
			s, verts, n );
		rt_bomb("nmg_cface() trying to make bogus face\n");
	}

	if (verts) {
		for (i=0 ; i < n ; ++i) {
			if (verts[i]) {
				NMG_CK_VERTEX(verts[i]);
			}
		}
		lu = nmg_mlv(&s->l.magic, verts[n-1], OT_SAME);
		fu = nmg_mf(lu);
		vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
		eu = nmg_meonvu(vu);

		if (!verts[n-1])
			verts[n-1] = eu->vu_p->v_p;

		for (i = n-2 ; i >= 0 ; i--) {
			eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
			eu = nmg_eusplit(verts[i], eu, 0);
			if (!verts[i])
				verts[i] = eu->vu_p->v_p;
		}

	} else {
		lu = nmg_mlv(&s->l.magic, (struct vertex *)NULL, OT_SAME);
		fu = nmg_mf(lu);
		vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
		eu = nmg_meonvu(vu);
		while (--n) {
			(void)nmg_eusplit((struct vertex *)NULL, eu, 0);
		}
	}
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_cface(s=x%x, verts[]=x%x, n=%d) fu=x%x\n",
			s, verts, n, fu);
	}
	return (fu);
}

/*
 *			N M G _ A D D _ L O O P _ T O _ F A C E
 *
 *	Create a new loop within a face, given a list of vertices.
 *	Modified version of nmg_cface().
 *
 *	"verts" is an array of "n" pointers to (struct vertex).  "s" is the
 *	parent shell for the new face.  The face will consist of a single loop
 *	made from edges between the n vertices.
 *
 *	If verts is a null pointer (no vertex list), all vertices of the face
 *	will be new points.  Otherwise, verts is a pointer to a list of
 *	vertices to use in creating the face/loop.  Null entries within the
 *	list will cause a new vertex to be created for that point.  Such new
 *	vertices will be inserted into the list for return to the caller.
 *
 *	The vertices should be listed in "counter-clockwise" (CCW) order if
 *	this is an ordinary face (loop), and in "clockwise" (CW) order if
 *	this is an interior ("hole" or "subtracted") face (loop).  This
 *	routine makes only topology, without reference to any geometry.
 *
 *	Note that this routine inserts new vertices (by edge use splitting)
 *	at the head of the loop, which reverses the order.  Therefore, the
 *	caller's vertices are traversed in reverse order to counter this
 *	behavior, and to effect the proper vertex order in the final face
 *	loop.
 */
struct faceuse *
nmg_add_loop_to_face(s, fu, verts, n, dir)
struct shell *s;
struct faceuse *fu;
struct vertex *verts[];
int n, dir;
{
	int i;
	struct edgeuse *eu;
	struct loopuse *lu;
	struct vertexuse *vu;

	NMG_CK_SHELL(s);
	if (n < 1) {
		bu_log("nmg_add_loop_to_face(s=x%x, verts=x%x, n=%d.)\n",
			s, verts, n );
		rt_bomb("nmg_add_loop_to_face: request to make 0 faces\n");
	}

	if (verts) {
		if (!fu) {
			lu = nmg_mlv(&s->l.magic, verts[n-1], dir);
			fu = nmg_mf(lu);
		} else {
			lu = nmg_mlv(&fu->l.magic, verts[n-1], dir);
		}
		vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
		eu = nmg_meonvu(vu);
		if (!verts[n-1])
			verts[n-1] = eu->vu_p->v_p;

		for (i = n-2 ; i >= 0 ; i--) {
			eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
			eu = nmg_eusplit(verts[i], eu, 0);
			if (!verts[i])
				verts[i] = eu->vu_p->v_p;
		}
	} else {
		if (!fu) {
			lu = nmg_mlv(&s->l.magic, (struct vertex *)NULL, dir);
			fu = nmg_mf(lu);
		} else {
			lu = nmg_mlv(&fu->l.magic, (struct vertex *)NULL, dir);
		}
		vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
		eu = nmg_meonvu(vu);
		while (--n) {
			(void)nmg_eusplit((struct vertex *)NULL, eu, 0);
		}
	}
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_add_loop_to_face(s=x%x, fu=x%x, verts[]=x%x, n=%d, %s) fu=x%x\n",
			s, fu, verts, n,
			nmg_orientation(dir) );
	}
	return (fu);
}

/*
 *			N M G _ F U _ P L A N E E Q N
 *
 *  Given a convex face that has been constructed with edges listed in
 *  counter-clockwise (CCW) order, compute the surface normal and plane
 *  equation for this face.
 *
 *
 *			D                   C
 *	                *-------------------*
 *	                |                   |
 *	                |   .<...........   |
 *	   ^     N      |   .           ^   |     ^
 *	   |      \     |   .  counter- .   |     |
 *	   |       \    |   .   clock   .   |     |
 *	   |C-B     \   |   .   wise    .   |     |C-B
 *	   |         \  |   v           .   |     |
 *	   |          \ |   ...........>.   |     |
 *	               \|                   |
 *	                *-------------------*
 *	                A                   B
 *			      <-----
 *				A-B
 *
 *  If the vertices in the loop are given in the order A B C D
 *  (e.g., counter-clockwise),
 *  then the outward pointing surface normal can be computed as:
 *
 *		N = (C-B) x (A-B)
 *
 *  This is the "right hand rule".
 *  For reference, note that a vector which points "into" the loop
 *  can be subsequently found by taking the cross product of the
 *  surface normal and any edge vector, e.g.:
 *
 *		Left = N x (B-A)
 *	or	Left = N x (C-B)
 *
 *  This routine will skip on past edges that start and end on
 *  the same vertex, in an attempt to avoid trouble.
 *  However, the loop *must* be convex for this routine to work.
 *  Otherwise, the surface normal may be inadvertently reversed.
 *
 *  Returns -
 *	0	OK
 *	-1	failure
 */
int
nmg_fu_planeeqn( fu, tol )
struct faceuse		*fu;
CONST struct bn_tol	*tol;
{
	struct edgeuse		*eu, *eu_final, *eu_next;
	struct loopuse		*lu;
	plane_t			plane;
	struct vertex		*a, *b, *c;
	register int		both_equal;

	BN_CK_TOL(tol);

	NMG_CK_FACEUSE(fu);
	lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	NMG_CK_LOOPUSE(lu);

	if( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
	{
		bu_log( "nmg_fu_planeeqn(): First loopuse does not contain edges\n" );
		return(-1);
	}
	eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(eu->vu_p);
	a = eu->vu_p->v_p;
	NMG_CK_VERTEX(a);
	NMG_CK_VERTEX_G(a->vg_p);

	eu_next = eu;
	do {
		eu_next = BU_LIST_PNEXT_CIRC(edgeuse, eu_next);
		NMG_CK_EDGEUSE(eu_next);
		if( eu_next == eu )
		{
			bu_log( "nmg_fu_planeeqn(): First loopuse contains only one edgeuse\n" );
			return -1;
		}
		NMG_CK_VERTEXUSE(eu_next->vu_p);
		b = eu_next->vu_p->v_p;
		NMG_CK_VERTEX(b);
		NMG_CK_VERTEX_G(b->vg_p);
	} while( (b == a
		|| bn_pt3_pt3_equal(a->vg_p->coord, b->vg_p->coord, tol))
		&& eu_next->vu_p != eu->vu_p );

	eu_final = eu_next;
	do {
		eu_final = BU_LIST_PNEXT_CIRC(edgeuse, eu_final);
		NMG_CK_EDGEUSE(eu_final);
		if( eu_final == eu )
		{
			bu_log( "nmg_fu_planeeqn(): Cannot find three distinct vertices\n" );
			return -1;
		}
		NMG_CK_VERTEXUSE(eu_final->vu_p);
		c = eu_final->vu_p->v_p;
		NMG_CK_VERTEX(c);
		NMG_CK_VERTEX_G(c->vg_p);
		both_equal = (c == b) ||
		    bn_pt3_pt3_equal(a->vg_p->coord, c->vg_p->coord, tol) ||
		    bn_pt3_pt3_equal(b->vg_p->coord, c->vg_p->coord, tol);
	} while( (both_equal
		|| bn_3pts_collinear(a->vg_p->coord, b->vg_p->coord,
			c->vg_p->coord, tol))
		&& eu_next->vu_p != eu->vu_p );

	if (bn_mk_plane_3pts(plane,
	    a->vg_p->coord, b->vg_p->coord, c->vg_p->coord, tol) < 0 ) {
		bu_log("nmg_fu_planeeqn(): bn_mk_plane_3pts failed on (%g,%g,%g) (%g,%g,%g) (%g,%g,%g)\n",
			V3ARGS( a->vg_p->coord ),
			V3ARGS( b->vg_p->coord ),
			V3ARGS( c->vg_p->coord ) );
	    	HPRINT("plane", plane);
		return(-1);
	}
	if (plane[0] == 0.0 && plane[1] == 0.0 && plane[2] == 0.0) {
		bu_log("nmg_fu_planeeqn():  Bad plane equation from bn_mk_plane_3pts\n" );
	    	HPRINT("plane", plane);
		return(-1);
	}
	nmg_face_g( fu, plane);

	/* Check and make sure all verts are within tol->dist of face */
	if( nmg_ck_fu_verts( fu, fu->f_p, tol ) != 0 )  {
		bu_log("nmg_fu_planeeqn(fu=x%x) ERROR, verts are not within tol of face\n" , fu );
		return -1;
	}

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_fu_planeeqn(fu=x%x, tol=x%x)\n", fu, tol);
	}
	return(0);
}

/*
 *			N M G _ G L U E F A C E S
 *
 *	given a shell containing "n" faces whose outward oriented faceuses are
 *	enumerated in "fulist", glue the edges of the faces together
 *	Most especially useful after using nmg_cface() several times to
 *	make faces which share vertex structures.
 *
 */
void
nmg_gluefaces(fulist, n, tol)
struct faceuse *fulist[];
int n;
CONST struct bn_tol *tol;
{
	struct shell	*s;
	struct loopuse	*lu;
	struct edgeuse	*eu;
	int		i;
	int		f_no;		/* Face number */
	
	NMG_CK_FACEUSE(fulist[0]);
	s = fulist[0]->s_p;
	NMG_CK_SHELL(s);

	/* First, perform some checks */
	for (i = 0 ; i < n ; ++i) {
		register struct faceuse	*fu;

		fu = fulist[i];
		NMG_CK_FACEUSE(fu);
		if (fu->s_p != s) {
			bu_log("nmg_gluefaces() in %s at %d. faceuses don't share parent\n",
				__FILE__, __LINE__);
			rt_bomb("nmg_gluefaces\n");
		}
	}

	for (i=0 ; i < n ; ++i) {
		for( BU_LIST_FOR( lu , loopuse , &fulist[i]->lu_hd ) ) {
			NMG_CK_LOOPUSE( lu );

			if( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
				continue;

			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
				for( f_no = i+1; f_no < n; f_no++ )  {
					struct loopuse		*lu2;
					register struct edgeuse	*eu2;

					for( BU_LIST_FOR( lu2 , loopuse , &fulist[f_no]->lu_hd ) ) {
						NMG_CK_LOOPUSE( lu2 );
						if( BU_LIST_FIRST_MAGIC(&lu2->down_hd) != NMG_EDGEUSE_MAGIC )
							continue;
						for( BU_LIST_FOR( eu2, edgeuse, &lu2->down_hd ) )  {
							if (EDGESADJ(eu, eu2))
								nmg_radial_join_eu(eu, eu2, tol);
						}
					}
				}
			}
		}
	}

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_gluefaces(fulist=x%x, n=%d)\n", fulist, n);
	}
}


/*			N M G _ S I M P L I F Y _ F A C E
 *
 *
 *	combine adjacent loops within a face which serve no apparent purpose
 *	by remaining separate and distinct.  Kill "wire-snakes" in face.
 *
 * Returns -
 *	0	If all was OK
 *	1	If faceuse is now empty
 */
int
nmg_simplify_face(fu)
struct faceuse *fu;
{
	struct loopuse *lu;
	int ret_val;

	NMG_CK_FACEUSE(fu);

	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))  {
		nmg_simplify_loop(lu);
	}

	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd))  {
		if( nmg_kill_snakes(lu) )  {
			struct loopuse	*klu = lu;
			lu = BU_LIST_PREV( loopuse, &lu->l );
			nmg_klu(klu);
		}
	}

	ret_val = BU_LIST_IS_EMPTY(&fu->lu_hd);

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_simplify_face(fut=x%x) return=%d\n", fu, ret_val);
	}

	return( ret_val );
}

/*
 *			N M G _ R E V E R S E _ F A C E
 *
 *
 *  This routine reverses the direction of the Normal vector which defines
 *  the plane of the face.  
 *
 *  The OT_SAME faceuse becomes the OT_OPPOSITE faceuse, and vice versa.  
 *  This preserves the convention that OT_SAME loopuses in the
 *  OT_SAME faceuse are counter-clockwise rotating about the surface normal.
 *
 *
 *	     Before			After
 *
 * 
 * N	  OT_SAME		  OT_OPPOSITE
 *  \	.<---------.		.<---------.
 *   \	|fu        ^		|fu        ^
 *    \	|  .------ | ->.	|  .------ | ->.
 *     \|  ^fumate |   |	|  ^fumate |   |
 *	|  |       |   |	|  |       |   |
 *	|  |       |   |	|  |       |   |
 *	V  |       |   |	V  |       |   |\
 *	.--------->.   |	.--------->.   | \
 *	   |           V	   |           V  \
 *	   .<----------.	   .<----------.   \
 *	    OT_OPPOSITE		     OT_SAME        \
 *						     N
 *
 *
 *  Also note that this reverses the same:opposite:opposite:same
 *  parity radially around each edge.  This can create parity errors
 *  until all faces of this shell have been processed.
 *
 *  Applications programmers should use nmg_invert_shell(),
 *  which does not have this problem.
 *  This routine is for internal use only.
 */
void
nmg_reverse_face( fu )
register struct faceuse	*fu;
{
	register struct faceuse	*fumate;

	NMG_CK_FACEUSE(fu);
	fumate = fu->fumate_p;
	NMG_CK_FACEUSE(fumate);
	NMG_CK_FACE(fu->f_p);
	NMG_CK_FACE_G_PLANE(fu->f_p->g.plane_p);

	/* reverse face normal vector */
	fu->f_p->flip = !fu->f_p->flip;

	/* switch which face is "outside" face */
	if (fu->orientation == OT_SAME)  {
		if (fumate->orientation != OT_OPPOSITE)  {
			bu_log("nmg_reverse_face(fu=x%x) fu:SAME, fumate:%d\n",
				fu, fumate->orientation);
			rt_bomb("nmg_reverse_face() orientation clash\n");
		} else {
			fu->orientation = OT_OPPOSITE;
			fumate->orientation = OT_SAME;
		}
	} else if (fu->orientation == OT_OPPOSITE) {
		if (fumate->orientation != OT_SAME)  {
			bu_log("nmg_reverse_face(fu=x%x) fu:OPPOSITE, fumate:%d\n",
				fu, fumate->orientation);
			rt_bomb("nmg_reverse_face() orientation clash\n");
		} else {
			fu->orientation = OT_SAME;
			fumate->orientation = OT_OPPOSITE;
		}
	} else {
		/* Unoriented face? */
		bu_log("ERROR nmg_reverse_face(fu=x%x), orientation=%d.\n",
			fu, fu->orientation );
	}

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_reverse_face(fu=x%x) fumate=x%x\n", fu, fumate);
	}
}

#if 0
/*
 * XXX Called in nmg_misc.c / nmg_reverse_face_and_radials()
 *			N M G _ F A C E _ F I X _ R A D I A L _ P A R I T Y
 *
 *  Around an edge, consider all the edgeuses that belong to a single shell.
 *  The faceuses pertaining to those edgeuses must maintain the appropriate
 *  parity with their radial faceuses, so that OT_SAME is always radial to
 *  OT_SAME, and OT_OPPOSITE is always radial to OT_OPPOSITE.
 *
 *  If a radial edgeuse is encountered that belongs to *this* face, then
 *  it might not have been processed by this routine yet, and is ignored
 *  for the purposes of checking parity.
 *
 *  When moving faces between shells, sometimes this parity relationship
 *  needs to be fixed, which can be easily accomplished by exchanging
 *  the incorrect edgeuse with it's mate in the radial edge linkages.
 *
 *  XXX Note that this routine will not work right in the presence of
 *  XXX dangling faces.
 *
 *  Note that this routine can't be used incrementally, because
 *  after an odd number (like one) of faceuses have been "fixed",
 *  there is an inherrent parity error, which will cause wrong
 *  decisions to be made.  Therefore, *all* faces have to be moved from
 *  one shell to another before the radial parity can be "fixed".
 *  Even then, this isn't going to work right unless we are given
 *  a list of all the "suspect" faceuses, so the initial parity
 *  value can be properly established.
 *
 *  XXXX I am of the opinion this routine is neither useful nor correct
 *  XXXX in it's present form, except for limited special cases.
 *
 *  The 'tol' arg is used strictly for printing purposes.
 *
 *  Returns -
 *	count of number of edges fixed.
 */
int
nmg_face_fix_radial_parity( fu, tol )
struct faceuse		*fu;
CONST struct bn_tol	*tol;
{
	struct loopuse *lu;
	struct edgeuse *eu;
	struct faceuse *fu2;
	struct shell	*s;
	long		count = 0;

	NMG_CK_FACEUSE( fu );
	s = fu->s_p;
	NMG_CK_SHELL(s);
	BN_CK_TOL(tol);

	/* Make sure we are now dealing with the OT_SAME faceuse */
	if( fu->orientation == OT_SAME )
		fu2 = fu;
	else
		fu2 = fu->fumate_p;

	for( BU_LIST_FOR( lu , loopuse , &fu2->lu_hd ) )  {
		NMG_CK_LOOPUSE( lu );

		/* skip loops of a single vertex */
		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		/*
		 *  Consider every edge of this loop.
		 *  Initial sequencing is:
		 *    before(radial), eu, eumate, after(radial)
		 */
		for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )  {
			struct edgeuse	*eumate;
			struct edgeuse	*before;
			struct edgeuse	*sbefore;	/* searched before */
			struct edgeuse	*after;

			eumate = eu->eumate_p;
			before = eu->radial_p;
			after = eumate->radial_p;
			NMG_CK_EDGEUSE(eumate);
			NMG_CK_EDGEUSE(before);
			NMG_CK_EDGEUSE(after);

			/* If no other edgeuses around edge, done. */
			if( before == eumate )
				continue;

			/*
			 *  Search in 'before' direction, until it's in
			 *  same shell.  Also ignore edgeuses from this FACE,
			 *  as they may not have been fixed yet.
			 */
			for( sbefore = before;
			     sbefore != eu && sbefore != eumate;
			     sbefore = sbefore->eumate_p->radial_p
			)  {
				struct faceuse	*bfu;
				if( nmg_find_s_of_eu(sbefore) != s )  continue;

				bfu = nmg_find_fu_of_eu(sbefore);
				/* If edgeuse isn't part of a faceuse, skip */
				if( !bfu )  continue;
				if( bfu->f_p == fu->f_p )  continue;
				/* Found a candidate */
				break;
			}

			/* If search found no others from this shell, done. */
			if( sbefore == eu || sbefore == eumate )
				continue;

			/*
			 *  'eu' is in an OT_SAME faceuse.
			 *  If the first faceuse in the 'before' direction
			 *  from this shell is OT_SAME, no fix is required.
			 */
			if( sbefore->up.lu_p->up.fu_p->orientation == OT_SAME )
				continue;

#if 0
bu_log("sbefore eu=x%x, before=x%x, eu=x%x, eumate=x%x, after=x%x\n",
			sbefore, before, eu, eumate, after );
nmg_pr_fu_around_eu(eu, tol );
#endif
			/*
			 *  Rearrange order to be: before, eumate, eu, after.
			 *  NOTE: do NOT use sbefore here.
			 */
			before->radial_p = eumate;
			eumate->radial_p = before;

			after->radial_p = eu;
			eu->radial_p = after;
			count++;
			if( rt_g.NMG_debug & DEBUG_BASIC )  {
				bu_log("nmg_face_fix_radial_parity() exchanging eu=x%x & eumate=x%x on edge x%x\n",
					eu, eumate, eu->e_p);
			}
#if 0
			/* Can't do this incrementally, it blows up after 1st "fix" */
			if( rt_g.NMG_debug )  {
nmg_pr_fu_around_eu(eu, tol );
				if( nmg_check_radial( eu, tol ) )
					rt_bomb("nmg_face_fix_radial_parity(): nmg_check_radial failed\n");
			}
#endif
		}
	}

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_face_fix_radial_parity(fu=x%x) returns %d\n", fu, count);
	}

	return count;
}
#endif

/*
 *			N M G _ M V _ F U _ B E T W E E N _ S H E L L S
 *
 *  Move faceuse from 'src' shell to 'dest' shell.
 */
void
nmg_mv_fu_between_shells( dest, src, fu )
struct shell		*dest;
register struct shell	*src;
register struct faceuse	*fu;
{
	register struct faceuse	*fumate;

	NMG_CK_FACEUSE(fu);
	fumate = fu->fumate_p;
	NMG_CK_FACEUSE(fumate);

	if (fu->s_p != src) {
		bu_log("nmg_mv_fu_between_shells(dest=x%x, src=x%x, fu=x%x), fu->s_p=x%x isnt src shell\n",
			dest, src, fu, fu->s_p );
		rt_bomb("fu->s_p isnt source shell\n");
	}
	if (fumate->s_p != src) {
		bu_log("nmg_mv_fu_between_shells(dest=x%x, src=x%x, fu=x%x), fumate->s_p=x%x isn't src shell\n",
			dest, src, fu, fumate->s_p );
		rt_bomb("fumate->s_p isnt source shell\n");
	}

	/* Remove fu from src shell */
	BU_LIST_DEQUEUE( &fu->l );
	if( BU_LIST_IS_EMPTY( &src->fu_hd ) )  {
		/* This was the last fu in the list, bad news */
		bu_log("nmg_mv_fu_between_shells(dest=x%x, src=x%x, fu=x%x), fumate=x%x not in src shell\n",
			dest, src, fu, fumate );
		rt_bomb("src shell emptied before finding fumate\n");
	}

	/* Remove fumate from src shell */
	BU_LIST_DEQUEUE( &fumate->l );

	/*
	 * Add fu and fumate to dest shell,
	 * preserving implicit OT_SAME, OT_OPPOSITE order
	 */
	if( fu->orientation == OT_SAME )  {
		BU_LIST_APPEND( &dest->fu_hd, &fu->l );
		BU_LIST_APPEND( &fu->l, &fumate->l );
	} else {
		BU_LIST_APPEND( &dest->fu_hd, &fumate->l );
		BU_LIST_APPEND( &fumate->l, &fu->l );
	}

	fu->s_p = dest;
	fumate->s_p = dest;

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_mv_fu_between_shells(dest=x%x, src=x%x, fu=x%x)\n", dest , src , fu);
	}
}

/*
 *			N M G _ M O V E _ F U _ F U
 *
 *  Move everything from the source faceuse into the destination faceuse.
 *  Exists as a support routine for nmg_jf() only.
 */
static void
nmg_move_fu_fu( dest_fu, src_fu )
register struct faceuse	*dest_fu;
register struct faceuse	*src_fu;
{
	register struct loopuse	*lu;

	NMG_CK_FACEUSE(dest_fu);
	NMG_CK_FACEUSE(src_fu);

	if( dest_fu->orientation != src_fu->orientation )
		rt_bomb("nmg_move_fu_fu: differing orientations\n");

	/* Move all loopuses from src to dest faceuse */
	while( BU_LIST_WHILE( lu, loopuse, &src_fu->lu_hd ) )  {
		BU_LIST_DEQUEUE( &(lu->l) );
		BU_LIST_INSERT( &(dest_fu->lu_hd), &(lu->l) );
		lu->up.fu_p = dest_fu;
	}
	/* The src_fu is invalid here, with an empty lu_hd list */

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_move_fu_fu(dest_fu=x%x, src_fu=x%x)\n", dest_fu , src_fu);
	}
}

/*
 *			N M G _ J F
 *
 *  Join two faces together by
 *  moving everything from the source faceuse and mate into the
 *  destination faceuse and mate, taking into account face orientations.
 *  The source face is destroyed by this operation.
 */
void
nmg_jf(dest_fu, src_fu)
register struct faceuse	*dest_fu;
register struct faceuse	*src_fu;
{
	NMG_CK_FACEUSE(dest_fu);
	NMG_CK_FACEUSE(src_fu);

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_jf(dest_fu=x%x, src_fu=x%x)\n", dest_fu, src_fu);
	}

	if( src_fu->f_p == dest_fu->f_p )
		rt_bomb("nmg_jf() src and dest faces are the same\n");

	if( dest_fu->orientation == src_fu->orientation )  {
		nmg_move_fu_fu(dest_fu, src_fu);
		nmg_move_fu_fu(dest_fu->fumate_p, src_fu->fumate_p);
	} else {
		nmg_move_fu_fu(dest_fu, src_fu->fumate_p);
		nmg_move_fu_fu(dest_fu->fumate_p, src_fu);
	}
	/* The src_fu is invalid here, having an empty lu_hd list */
	nmg_kfu(src_fu);

}

/*
 *			N M G _ D U P _ F A C E
 *
 *  Construct a duplicate of a face into the shell 's'.
 *  The vertex geometry is copied from the source face into topologically
 *  distinct (new) vertex and vertex_g structs.
 *  They will start out being geometricly coincident, but it is anticipated
 *  that the caller will modify the geometry, e.g. as in an extrude operation.
 *
 *  It is the caller's responsibility to re-bound the new face after
 *  making any modifications.
 */
struct faceuse *
nmg_dup_face(fu, s)
struct faceuse *fu;
struct shell *s;
{
	struct loopuse *new_lu, *lu;
	struct faceuse *new_fu = (struct faceuse *)NULL;
	struct model	*m;
	struct model	*m_f;
	plane_t		pl;
	long		**trans_tbl;
	long		tbl_size;

	NMG_CK_FACEUSE(fu);
	NMG_CK_FACE(fu->f_p);
	NMG_CK_SHELL(s);

	/* allocate the table that holds the translations between existing
	 * elements and the duplicates we will create.
	 */
	m = nmg_find_model( (long *)s );
	tbl_size = m->maxindex;

	m_f = nmg_find_model( (long *)fu );
	if (m != m_f)
		tbl_size += m_f->maxindex;

	/* Needs double space, because model will grow as dup proceeds */
	trans_tbl = (long **)rt_calloc(tbl_size*2, sizeof(long *),
			"nmg_dup_face trans_tbl");

	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (rt_g.NMG_debug & DEBUG_BASIC)
			bu_log("nmg_dup_face() duping %s loop...",
				nmg_orientation(lu->orientation));
		if (new_fu) {
			new_lu = nmg_dup_loop(lu, &new_fu->l.magic, trans_tbl);
		} else {
			new_lu = nmg_dup_loop(lu, &s->l.magic, trans_tbl);
			new_fu = nmg_mf(new_lu);

			/* since nmg_mf() FORCES the orientation of the
			 * initial loop to be OT_SAME, we need to re-set
			 * the orientation of new_lu if lu->orientation
			 * is not OT_SAME.  In general, the first loopuse on
			 * a faceuse's linked list will be OT_SAME, but
			 * in some circumstances (such as after booleans)
			 * the first loopuse MAY be OT_OPPOSITE
			 */
			if (lu->orientation != OT_SAME) {
				new_lu->orientation = lu->orientation;
				new_lu->lumate_p->orientation =
					lu->lumate_p->orientation;
			}
		}
		if (rt_g.NMG_debug & DEBUG_BASIC)
			bu_log(".  Duped %s loop\n",
				nmg_orientation(new_lu->orientation));
	}

	/* Create duplicate, independently modifiable face geometry */
	switch (*fu->f_p->g.magic_p) {
	case NMG_FACE_G_PLANE_MAGIC:
		NMG_GET_FU_PLANE( pl, fu );
		nmg_face_g(new_fu, pl);
		break;
	case NMG_FACE_G_SNURB_MAGIC:
		{
			struct face_g_snurb	*old = fu->f_p->g.snurb_p;
			struct face_g_snurb	*new;
			/* Create a new, duplicate snurb */
			nmg_face_g_snurb(new_fu,
				old->order[0], old->order[1],
				old->u.k_size, old->v.k_size,
				NULL, NULL,
				old->s_size[0], old->s_size[1],
				old->pt_type,
				NULL );
			new = new_fu->f_p->g.snurb_p;
			/* Copy knots */
			bcopy( old->u.knots, new->u.knots, old->u.k_size*sizeof(fastf_t) );
			bcopy( old->v.knots, new->v.knots, old->v.k_size*sizeof(fastf_t) );
			/* Copy mesh */
			bcopy( old->ctl_points, new->ctl_points,
				old->s_size[0] * old->s_size[1] *
				RT_NURB_EXTRACT_COORDS(old->pt_type) *
				sizeof(fastf_t) );
		}
	}
	new_fu->orientation = fu->orientation;
	new_fu->fumate_p->orientation = fu->fumate_p->orientation;

	rt_free((char *)trans_tbl, "nmg_dup_face trans_tbl");

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_dup_face(fu=x%x, s=x%x) new_fu=x%x\n",
			fu, s, new_fu);
	}

	return(new_fu);
}

/************************************************************************
 *									*
 *				LOOP Routines				*
 *									*
 ************************************************************************/

/*
 *			N M G _ J L
 *
 *  Join two loops together which share a common edge,
 *  such that both occurances of the common edge are deleted.
 *  This routine always leaves "lu" intact, and kills the loop
 *  radial to "eu" (after stealing all it's edges).
 *
 *  Either both loops must be of the same orientation, or then
 *  first loop must be OT_SAME, and the second loop must be OT_OPPOSITE.
 *  Joining OT_SAME & OT_OPPOSITE always gives an OT_SAME result.
 *		Above statment is not true!!!! I have added nmg_lu_reorient() -JRA
 *  Since "lu" must survive, it must be the OT_SAME one.
 */
void
nmg_jl(lu, eu)
struct loopuse *lu;
struct edgeuse *eu;
{
	struct loopuse	*lu2;
	struct edgeuse	*eu_r;		/* use of shared edge in lu2 */
	struct edgeuse	*nexteu;

	NMG_CK_LOOPUSE(lu);

	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGEUSE(eu->eumate_p);
	eu_r = eu->radial_p;
	NMG_CK_EDGEUSE(eu_r);
	NMG_CK_EDGEUSE(eu_r->eumate_p);

	lu2 = eu_r->up.lu_p;
	NMG_CK_LOOPUSE(lu2);

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_jl(lu=x%x, eu=x%x) lu2=x%x\n", lu, eu, lu2);
	}

	if (eu->up.lu_p != lu)
		rt_bomb("nmg_jl: edgeuse is not child of loopuse?\n");

	if (lu2->l.magic != NMG_LOOPUSE_MAGIC)
		rt_bomb("nmg_jl: radial edgeuse not part of loopuse\n");

	if (lu2 == lu)
		rt_bomb("nmg_jl: trying to join a loop to itself\n");

	if (lu->up.magic_p != lu2->up.magic_p)
		rt_bomb("nmg_jl: loopuses do not share parent\n");

	if (lu2->orientation != lu->orientation)  {
		if( lu->orientation != OT_SAME || lu2->orientation != OT_OPPOSITE )  {
			bu_log("nmg_jl: lu2 = %s, lu = %s\n",
				nmg_orientation(lu2->orientation),
				nmg_orientation(lu->orientation) );
			rt_bomb("nmg_jl: can't join loops of different orientation!\n");
		} else {
			/* Consuming an OPPOSITE into a SAME is OK */
		}
	}

	if (eu->radial_p->eumate_p->radial_p->eumate_p != eu ||
	    eu->eumate_p->radial_p->eumate_p->radial_p != eu)
	    	rt_bomb("nmg_jl: edgeuses must be sole uses of edge to join loops\n");

	/*
	 * Remove all the edgeuses "ahead" of our radial and insert them
	 * "behind" the current edgeuse.
	 * Operates on lu and lu's mate simultaneously.
	 */
	nexteu = BU_LIST_PNEXT_CIRC(edgeuse, eu_r);
	while (nexteu != eu_r) {
		BU_LIST_DEQUEUE(&nexteu->l);
		BU_LIST_INSERT(&eu->l, &nexteu->l);
		nexteu->up.lu_p = eu->up.lu_p;

		BU_LIST_DEQUEUE(&nexteu->eumate_p->l);
		BU_LIST_APPEND(&eu->eumate_p->l, &nexteu->eumate_p->l);
		nexteu->eumate_p->up.lu_p = eu->eumate_p->up.lu_p;

		nexteu = BU_LIST_PNEXT_CIRC(edgeuse, eu_r);
	}

	/*
	 * The other loop just has the one (shared) edgeuse left in it.
	 * Delete the other loop (and it's mate).
	 */
	nmg_klu(lu2);

	/*
	 * Kill the one remaining use of the (formerly) "shared" edge in lu
	 * and voila: one contiguous loop.
	 */
	if( nmg_keu(eu) )  rt_bomb("nmg_jl() loop vanished?\n");

	nmg_lu_reorient( lu );
}

/*
 *			N M G _ J O I N _ 2 L O O P S
 *
 *  Intended to join an interior and exterior loop together,
 *  by building a bridge between the two indicated vertices.
 *
 *  This routine can be used to join two exterior loops which do not
 *  overlap, and it can also be used to join an exterior loop with
 *  a loop of oposite orientation that lies entirely within it.
 *  This restriction is important, but not checked for.
 *
 *  If the two vertexuses reference distinct vertices, then two new
 *  edges are built to bridge the loops together.
 *  If the two vertexuses share the same vertex, then it is even easier.
 *
 *  Returns the replacement for vu2.
 */
struct vertexuse *
nmg_join_2loops( vu1, vu2 )
struct vertexuse	*vu1;
struct vertexuse	*vu2;
{
	struct edgeuse	*eu1, *eu2;
	struct edgeuse	*first_new_eu;
	struct edgeuse	*second_new_eu;
	struct edgeuse	*final_eu2;
	struct loopuse	*lu1, *lu2;
	int		new_orient;

	NMG_CK_VERTEXUSE(vu1);
	NMG_CK_VERTEXUSE(vu2);
	eu1 = vu1->up.eu_p;
	eu2 = vu2->up.eu_p;
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	lu1 = eu1->up.lu_p;
	lu2 = eu2->up.lu_p;
	NMG_CK_LOOPUSE(lu1);
	NMG_CK_LOOPUSE(lu2);

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_join_2loops(vu1=x%x, vu2=x%x) lu1=x%x, lu2=x%x\n",
			vu1, vu2, lu1, lu2 );
	}

	if( lu1 == lu2 || lu1->l_p == lu2->l_p )
		rt_bomb("nmg_join_2loops: can't join loop to itself\n");

	if( lu1->up.fu_p != lu2->up.fu_p )
		rt_bomb("nmg_join_2loops: can't join loops in different faces\n");

	if( vu1->v_p != vu2->v_p )  {
		/*
		 *  Start by taking a jaunt from vu1 to vu2 and back.
		 */

		/* insert 0 length edge, before eu1 */
		first_new_eu = nmg_eins(eu1);

		/* split the new edge, and connect it to vertex 2 */
		second_new_eu = nmg_eusplit( vu2->v_p, first_new_eu, 0 );
		first_new_eu = BU_LIST_PPREV_CIRC(edgeuse, second_new_eu);

		/* Make the two new edgeuses share just one edge */
		nmg_je( second_new_eu, first_new_eu );

		/* first_new_eu is eu that enters shared vertex */
		vu1 = second_new_eu->vu_p;
	} else {
		second_new_eu = eu1;
		first_new_eu = BU_LIST_PPREV_CIRC(edgeuse, second_new_eu);
		NMG_CK_EDGEUSE(second_new_eu);
	}
	/* second_new_eu is eu that departs from shared vertex */
	vu2 = second_new_eu->vu_p;	/* replacement for original vu2 */
	if( vu1->v_p != vu2->v_p )  rt_bomb("nmg_join_2loops: jaunt failed\n");

	/*
	 *  Gobble edges off of loop2 (starting with eu2),
	 *  and insert them into loop1,
	 *  between first_new_eu and second_new_eu.
	 *  The final edge from loop 2 will then be followed by
	 *  second_new_eu.
	 */
	final_eu2 = BU_LIST_PPREV_CIRC(edgeuse, eu2 );
	while( BU_LIST_NON_EMPTY( &lu2->down_hd ) )  {
		eu2 = BU_LIST_PNEXT_CIRC(edgeuse, final_eu2);

		BU_LIST_DEQUEUE(&eu2->l);
		BU_LIST_INSERT(&second_new_eu->l, &eu2->l);
		eu2->up.lu_p = lu1;

		BU_LIST_DEQUEUE(&eu2->eumate_p->l);
		BU_LIST_APPEND(&second_new_eu->eumate_p->l, &eu2->eumate_p->l);
		eu2->eumate_p->up.lu_p = lu1->lumate_p;
	}

	nmg_lu_reorient( lu1 );

	/* Kill entire (null) loop associated with lu2 */
	nmg_klu(lu2);

	return vu2;
}

/* XXX These should be included in nmg_join_2loops, or be called by it */
/*
 *			N M G _ J O I N _ S I N G V U _ L O O P
 *
 *  vu1 is in a regular loop, vu2 is in a loop of a single vertex
 *  A jaunt is taken from vu1 to vu2 and back to vu1, and the
 *  old loop at vu2 is destroyed.
 *  Return is the new vu that replaces vu2.
 */
struct vertexuse *
nmg_join_singvu_loop( vu1, vu2 )
struct vertexuse	*vu1, *vu2;
{
    	struct edgeuse	*eu1;
	struct edgeuse	*first_new_eu, *second_new_eu;
	struct loopuse	*lu2;

	NMG_CK_VERTEXUSE( vu1 );
	NMG_CK_VERTEXUSE( vu2 );

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_join_singvu_loop(vu1=x%x, vu2=x%x) lu1=x%x, lu2=x%x\n",
			vu1, vu2, vu1->up.lu_p, vu2->up.lu_p );
	}

	if( *vu2->up.magic_p != NMG_LOOPUSE_MAGIC ||
	    *vu1->up.magic_p != NMG_EDGEUSE_MAGIC )  rt_bomb("nmg_join_singvu_loop bad args\n");

	if( vu1->v_p == vu2->v_p )  rt_bomb("nmg_join_singvu_loop same vertex\n");

    	/* Take jaunt from vu1 to vu2 and back */
    	eu1 = vu1->up.eu_p;
    	NMG_CK_EDGEUSE(eu1);

    	/* Insert 0 length edge */
    	first_new_eu = nmg_eins(eu1);
	/* split the new edge, and connect it to vertex 2 */
	second_new_eu = nmg_eusplit( vu2->v_p, first_new_eu, 0 );
	first_new_eu = BU_LIST_PPREV_CIRC(edgeuse, second_new_eu);
	/* Make the two new edgeuses share just one edge */
	nmg_je( second_new_eu, first_new_eu );

	/* Kill loop lu2 associated with vu2 */
	lu2 = vu2->up.lu_p;
	NMG_CK_LOOPUSE(lu2);
	nmg_klu(lu2);

	return second_new_eu->vu_p;
}

/*
 *			N M G _ J O I N _ 2 S I N G V U _ L O O P S
 *
 *  Both vertices are part of single vertex loops.
 *  Converts loop on vu1 into a real loop that connects them together,
 *  with a single edge (two edgeuses).
 *  Loop on vu2 is killed.
 *  Returns replacement vu for vu2.
 *  Does not change the orientation.
 */
struct vertexuse *
nmg_join_2singvu_loops( vu1, vu2 )
struct vertexuse	*vu1, *vu2;
{
	struct edgeuse	*first_new_eu, *second_new_eu;
	struct loopuse	*lu1, *lu2;

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_join_2singvu_loops(vu1=x%x, vu2=x%x) lu1=x%x, lu2=x%x\n",
			vu1, vu2, vu1->up.lu_p, vu2->up.lu_p );
	}

	NMG_CK_VERTEXUSE( vu1 );
	NMG_CK_VERTEXUSE( vu2 );

	if( *vu2->up.magic_p != NMG_LOOPUSE_MAGIC ||
	    *vu1->up.magic_p != NMG_LOOPUSE_MAGIC )  rt_bomb("nmg_join_2singvu_loops bad args\n");

	if( vu1->v_p == vu2->v_p )  rt_bomb("nmg_join_2singvu_loops same vertex\n");

    	/* Take jaunt from vu1 to vu2 and back */
	/* Make a 0 length edge on vu1 */
	first_new_eu = nmg_meonvu(vu1);
	/* split the new edge, and connect it to vertex 2 */
	second_new_eu = nmg_eusplit( vu2->v_p, first_new_eu, 0 );
	first_new_eu = BU_LIST_PPREV_CIRC(edgeuse, second_new_eu);
	/* Make the two new edgeuses share just one edge */
	nmg_je( second_new_eu, first_new_eu );

	/* Kill loop lu2 associated with vu2 */
	lu2 = vu2->up.lu_p;
	NMG_CK_LOOPUSE(lu2);
	nmg_klu(lu2);

	lu1 = vu1->up.eu_p->up.lu_p;
	NMG_CK_LOOPUSE(lu1);

	return second_new_eu->vu_p;
}

/*			N M G _ C U T _ L O O P
 *
 *	Divide a loop of edges between two vertexuses.
 *
 *	Make a new loop between the two vertexes, and split it and
 *	the loop of the vertexuses at the same time.
 *
 *
 *		BEFORE					AFTER
 *
 *
 *     Va    eu1  vu1		Vb	       Va   eu1  vu1             Vb
 *	* <---------* <---------*		* <--------*  * <--------*
 *	|					|	      |		 
 *	|			^		|	   ^  |		 ^
 *	|	 Original	|		| Original |  |	   New   |
 *	|	   Loopuse	|		| Loopuse  |  |	 Loopuse |
 *	V			|		V          |  V	/	 |
 *				|		           |   /	 |
 *	*----------> *--------> *		*--------> *  *--------> *
 *     Vd	     vu2 eu2	Vc	       Vd             vu2  eu2   Vc
 *
 *  Returns the new loopuse pointer.  The new loopuse will contain "vu2"
 *  and the edgeuse associated with "vu2" as the FIRST edgeuse on the
 *  list of edgeuses.  The edgeuse for the new edge  (connecting
 *  the verticies indicated by vu1 and vu2) will be the LAST edgeuse on the
 *  new loopuse's list of edgeuses.
 *
 *  It is the caller's responsibility to re-bound the loops.
 *
 *  Both old and new loopuse will have orientation OT_UNSPEC.  It is the
 *  callers responsibility to determine what the orientations should be.
 *  This can be conveniently done with nmg_lu_reorient().
 *
 *  Here is a simple example of how the new loopuse might have a different
 *  orientation than the original one:
 *
 *
 *		F<----------------E
 *		|                 ^
 *		|                 |
 *		|      C--------->D
 *		|      ^          .
 *		|      |          .
 *		|      |          .
 *		|      B<---------A
 *		|                 ^
 *		v                 |
 *		G---------------->H
 *
 *  When nmg_cut_loop(A,D) is called, the new loop ABCD is clockwise,
 *  even though the original loop was counter-clockwise.
 *  There is no way to determine this without referring to the
 *  face normal and vertex geometry, which being a topology routine
 *  this routine shouldn't do.
 *
 *  Returns -
 *	NULL	Error
 *	lu	Loopuse of new loop, on success.
 */
struct loopuse *
nmg_cut_loop(vu1, vu2)
struct vertexuse *vu1, *vu2;
{
	struct loopuse *lu, *oldlu;
	struct edgeuse *eu1, *eu2, *eunext, *neweu, *eu;
	struct model	*m;
	FILE		*fd;
	char		name[32];
	static int	i=0;

	NMG_CK_VERTEXUSE(vu1);
	NMG_CK_VERTEXUSE(vu2);

	eu1 = vu1->up.eu_p;
	eu2 = vu2->up.eu_p;
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	oldlu = eu1->up.lu_p;
	NMG_CK_LOOPUSE(oldlu);

	if (eu2->up.lu_p != oldlu) {
		rt_bomb("nmg_cut_loop() vertices not decendants of same loop\n");
	}

	if( vu1->v_p == vu2->v_p )  {
		/* The loops touch, use a different routine */
		lu = nmg_split_lu_at_vu( oldlu, vu1 );
		goto out;
	}

	NMG_CK_FACEUSE(oldlu->up.fu_p);
	m = oldlu->up.fu_p->s_p->r_p->m_p;
	NMG_CK_MODEL(m);

	if (rt_g.NMG_debug & DEBUG_CUTLOOP) {
		bu_log("\tnmg_cut_loop\n");
		if (rt_g.NMG_debug & DEBUG_PLOTEM) {
			long		*tab;
			tab = (long *)rt_calloc( m->maxindex, sizeof(long),
				"nmg_cut_loop flag[] 1" );

			(void)sprintf(name, "Before_cutloop%d.pl", ++i);
			bu_log("nmg_cut_loop() plotting %s\n", name);
			if ((fd = fopen(name, "w")) == (FILE *)NULL) {
				(void)perror(name);
				exit(-1);
			}

			nmg_pl_fu(fd, oldlu->up.fu_p, tab, 100, 100, 100);
			nmg_pl_fu(fd, oldlu->up.fu_p->fumate_p, tab, 100, 100, 100);
			(void)fclose(fd);
			rt_free( (char *)tab, "nmg_cut_loop flag[] 1" );
		}
	}

	/* make a new loop structure for the new loop & throw away
	 * the vertexuse we don't need
	 */
	lu = nmg_mlv(oldlu->up.magic_p, (struct vertex *)NULL, OT_UNSPEC );
	oldlu->orientation = oldlu->lumate_p->orientation = OT_UNSPEC;

	nmg_kvu(BU_LIST_FIRST(vertexuse, &lu->down_hd));
	nmg_kvu(BU_LIST_FIRST(vertexuse, &lu->lumate_p->down_hd));
	/* nmg_kvu() does BU_LIST_INIT() on down_hd */
	/* The loopuse is considered invalid until it gets some edges */

	/* move the edges into one of the uses of the new loop */
	for (eu = eu2 ; eu != eu1 ; eu = eunext) {
		eunext = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);

		BU_LIST_DEQUEUE(&eu->l);
		BU_LIST_INSERT(&lu->down_hd, &eu->l);
		BU_LIST_DEQUEUE(&eu->eumate_p->l);
		BU_LIST_APPEND(&lu->lumate_p->down_hd, &eu->eumate_p->l);
		eu->up.lu_p = lu;
		eu->eumate_p->up.lu_p = lu->lumate_p;
	}

	/* make a wire edge in the shell to "cap off" the new loop */
	neweu = nmg_me(eu1->vu_p->v_p, eu2->vu_p->v_p, nmg_find_s_of_eu(eu1));

	/* move the new edgeuse into the new loopuse */
	BU_LIST_DEQUEUE(&neweu->l);
	BU_LIST_INSERT(&lu->down_hd, &neweu->l);
	neweu->up.lu_p = lu;

	/* move the new edgeuse mate into the new loopuse mate */
	BU_LIST_DEQUEUE(&neweu->eumate_p->l);
	BU_LIST_APPEND(&lu->lumate_p->down_hd, &neweu->eumate_p->l);
	neweu->eumate_p->up.lu_p = lu->lumate_p;

	/* now we go back and close up the loop we just ripped open */
	eunext = nmg_me(eu2->vu_p->v_p, eu1->vu_p->v_p, nmg_find_s_of_eu(eu1));

	BU_LIST_DEQUEUE(&eunext->l);
	BU_LIST_INSERT(&eu1->l, &eunext->l);
	BU_LIST_DEQUEUE(&eunext->eumate_p->l);
	BU_LIST_APPEND(&eu1->eumate_p->l, &eunext->eumate_p->l);
	eunext->up.lu_p = eu1->up.lu_p;
	eunext->eumate_p->up.lu_p = eu1->eumate_p->up.lu_p;

	/* make new edgeuses radial to each other, sharing the new edge */
	nmg_je(neweu, eunext);

	nmg_ck_lueu(oldlu, "oldlu");
	nmg_ck_lueu(lu, "lu");	/*LABLABLAB*/


	if (rt_g.NMG_debug & DEBUG_CUTLOOP && rt_g.NMG_debug & DEBUG_PLOTEM) {
		long		*tab;
		tab = (long *)rt_calloc( m->maxindex, sizeof(long),
			"nmg_cut_loop flag[] 2" );

		(void)sprintf(name, "After_cutloop%d.pl", i);
		bu_log("nmg_cut_loop() plotting %s\n", name);
		if ((fd = fopen(name, "w")) == (FILE *)NULL) {
			(void)perror(name);
			exit(-1);
		}

		nmg_pl_fu(fd, oldlu->up.fu_p, tab, 100, 100, 100);
		nmg_pl_fu(fd, oldlu->up.fu_p->fumate_p, tab, 100, 100, 100);
		(void)fclose(fd);
		rt_free( (char *)tab, "nmg_cut_loop flag[] 2" );
	}
out:
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_cut_loop(vu1=x%x, vu2=x%x) old_lu=x%x, new_lu=x%x\n",
			vu1, vu2, oldlu, lu );
	}

	return lu;
}

/*
 *			N M G _ S P L I T _ L U _ A T _ V U
 *
 *  In a loop which has at least two distinct uses of a vertex,
 *  split off the edges from "split_vu" to the second occurance of
 *  the vertex into a new loop.
 *  It is the caller's responsibility to re-bound the loops.
 *
 *  The old and new loopuses will have orientation OT_UNSPEC.  It is the
 *  callers responsibility to determine what their orientations should be.
 *  This can be conveniently done with nmg_lu_reorient().
 *
 *  Here is an example:
 *
 *	              E<__________B <___________A
 *	              |           ^\            ^
 *	              |          /  \           |
 *	              |         /    \          |
 *	              |        /      v         |
 *	              |       D<_______C        |
 *	              v                         |
 *	              F________________________>G
 *	
 *  When nmg_split_lu_at_vu(lu, B) is called, the old loopuse continues to
 *  be counter clockwise and OT_SAME, but the new loopuse BCD is now
 *  clockwise.  It needs to be marked OT_OPPOSITE.  Without referring
 *  to the geometry, this can't be determined.
 *
 *  Intended primarily for use by nmg_split_touchingloops().
 *
 *  Returns -
 *	NULL	Error
 *	lu	Loopuse of new loop, on success.
 */
struct loopuse *
nmg_split_lu_at_vu( lu, split_vu )
struct loopuse		*lu;
struct vertexuse	*split_vu;
{
	struct edgeuse		*eu;
	struct vertexuse	*vu;
	struct loopuse		*newlu;
	struct loopuse		*newlumate;
	struct vertex		*split_v;
	int			iteration;

	split_v = split_vu->v_p;
	NMG_CK_VERTEX(split_v);

	eu = split_vu->up.eu_p;
	NMG_CK_EDGEUSE(eu);

	if( eu->up.lu_p != lu )  {
		/* Could not find indicated vertex */
		newlu = (struct loopuse *)0;		/* FAIL */
		goto out;
	}

	/* Make a new loop in the same face */
	lu->orientation = lu->lumate_p->orientation = OT_UNSPEC;
	newlu = nmg_mlv( lu->up.magic_p, (struct vertex *)NULL, OT_UNSPEC );
	NMG_CK_LOOPUSE(newlu);
	newlumate = newlu->lumate_p;
	NMG_CK_LOOPUSE(newlumate);

	/* Throw away unneeded lone vertexuse */
	nmg_kvu(BU_LIST_FIRST(vertexuse, &newlu->down_hd));
	nmg_kvu(BU_LIST_FIRST(vertexuse, &newlumate->down_hd));
	/* nmg_kvu() does BU_LIST_INIT() on down_hd */

	/* Move edges & mates into new loop until vertex is repeated */
	for( iteration=0; iteration < 10000; iteration++ )  {
		struct edgeuse	*eunext;
		eunext = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);

		BU_LIST_DEQUEUE(&eu->l);
		BU_LIST_INSERT(&newlu->down_hd, &eu->l);
		BU_LIST_DEQUEUE(&eu->eumate_p->l);
		BU_LIST_APPEND(&newlumate->down_hd, &eu->eumate_p->l);

		/* Change edgeuse & mate up pointers */
		eu->up.lu_p = newlu;
		eu->eumate_p->up.lu_p = newlumate;

		/* Advance to next edgeuse */
		eu = eunext;

		/* When split_v is encountered, stop */
		vu = eu->vu_p;
		NMG_CK_VERTEXUSE(vu);
		if( vu->v_p == split_v )  break;
	}
	if( iteration >= 10000 )  rt_bomb("nmg_split_lu_at_vu:  infinite loop\n");
out:
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_split_lu_at_vu( lu=x%x, split_vu=x%x ) newlu=x%x\n",
			lu, split_vu, newlu );
	}
	return newlu;
}

/*
 *			N M G _ F I N D _ R E P E A T E D _ V _ I N _ L U
 *
 *  Given a vertexuse of an edgeuse in a loopuse, see if the vertex is
 *  used by at least one other vertexuse in that same loopuse.
 *
 *  Returns -
 *	vu	If this vertex appears elsewhere in the loopuse.
 *	NULL	If this is the only occurance of this vertex in the loopuse.
 *  XXX move to nmg_info.c
 */
struct vertexuse *
nmg_find_repeated_v_in_lu( vu )
struct vertexuse	*vu;
{
	struct vertexuse	*tvu;		/* vu to test */
	struct loopuse		*lu;
	struct vertex		*v;

	v = vu->v_p;
	NMG_CK_VERTEX(v);

	if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
		return (struct vertexuse *)0;
	if( *vu->up.eu_p->up.magic_p != NMG_LOOPUSE_MAGIC )
		return (struct vertexuse *)0;
	lu = vu->up.eu_p->up.lu_p;

	/*
	 *  For each vertexuse on vertex list,
	 *  check to see if it points up to the this loop.
	 *  If so, then there is a duplicated vertex.
	 *  Ordinarily, the vertex list will be *very* short,
	 *  so this strategy is likely to be faster than
	 *  a table-based approach, for most cases.
	 */
	for( BU_LIST_FOR( tvu, vertexuse, &v->vu_hd ) )  {
		struct edgeuse		*teu;
		struct loopuse		*tlu;

		if( tvu == vu )  continue;
		if( *tvu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
		teu = tvu->up.eu_p;
		NMG_CK_EDGEUSE(teu);
		if( *teu->up.magic_p != NMG_LOOPUSE_MAGIC )  continue;
		tlu = teu->up.lu_p;
		NMG_CK_LOOPUSE(tlu);
		if( tlu != lu )  continue;
		/*
		 *  Repeated vertex exists.  Return (one) other use of it.
		 */
		return tvu;
	}
	return (struct vertexuse *)0;
}

/*
 *			N M G _ S P L I T _ T O U C H I N G L O O P S
 *
 *  Search through all the vertices in a loop.
 *  Whenever there are two distinct uses of one vertex in the loop,
 *  split off all the edges between them into a new loop.
 *
 *  Note that the call to nmg_split_lu_at_vu() will cause the split
 *  loopuses to be marked OT_UNSPEC.
 */
void
nmg_split_touchingloops( lu, tol )
struct loopuse		*lu;
CONST struct bn_tol	*tol;
{
	struct edgeuse		*eu;
	struct vertexuse	*vu;
	struct loopuse		*newlu;
	struct vertexuse	*vu_save;

	NMG_CK_LOOPUSE(lu);
	BN_CK_TOL(tol);
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_split_touchingloops( lu=x%x )\n", lu);
	}
top:
	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return;

	vu_save = (struct vertexuse *)NULL;

	/* For each edgeuse, get vertexuse and vertex */
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {

		struct edgeuse *other_eu;
		struct vertexuse *other_vu;
		int vu_is_part_of_crack=0;

		vu = eu->vu_p;
		NMG_CK_VERTEXUSE(vu);

#if 1
		if( !nmg_find_repeated_v_in_lu( vu ) )  continue;

		/* avoid splitting a crack if possible */
		for( BU_LIST_FOR( other_vu, vertexuse, &vu->v_p->vu_hd ) )
		{
			if( nmg_find_lu_of_vu( other_vu ) != lu )
				continue;
			if( *other_vu->up.magic_p != NMG_EDGEUSE_MAGIC )
				continue;
			other_eu = other_vu->up.eu_p;
			if( nmg_eu_is_part_of_crack( other_eu ) )
			{
				vu_save = other_vu;
				vu_is_part_of_crack = 1;
				break;
			}
		}

		if( vu_is_part_of_crack )
			continue;

		/*
		 *  Repeated vertex exists,
		 *  Split loop into two loops
		 */
		newlu = nmg_split_lu_at_vu( lu, vu );
		NMG_CK_LOOPUSE(newlu);
		NMG_CK_LOOP(newlu->l_p);
		nmg_loop_g(newlu->l_p, tol);

		/* Ensure there are no duplications in new loop */
		nmg_split_touchingloops(newlu, tol);

		/* There is no telling where we will be in the
		 * remainder of original loop, check 'em all.
		 */
		goto top;
	}

	if( vu_save )
	{
		/* split loop at crack */
		newlu = nmg_split_lu_at_vu( lu, vu_save );
		NMG_CK_LOOPUSE(newlu);
		NMG_CK_LOOP(newlu->l_p);
		nmg_loop_g(newlu->l_p, tol);

		/* Ensure there are no duplications in new loop */
		nmg_split_touchingloops(newlu, tol);

		/* There is no telling where we will be in the
		 * remainder of original loop, check 'em all.
		 */
		goto top;
	}

#else
		v = vu->v_p;
		NMG_CK_VERTEX(v);

		/*
		 *  For each vertexuse on vertex list,
		 *  check to see if it points up to the this loop.
		 *  If so, then there is a duplicated vertex.
		 *  Ordinarily, the vertex list will be *very* short,
		 *  so this strategy is likely to be faster than
		 *  a table-based approach, for most cases.
		 */
		for( BU_LIST_FOR( tvu, vertexuse, &v->vu_hd ) )  {
			struct edgeuse		*teu;
			struct loopuse		*tlu;

			if( tvu == vu )  continue;
			if( *tvu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
			teu = tvu->up.eu_p;
			NMG_CK_EDGEUSE(teu);
			if( *teu->up.magic_p != NMG_LOOPUSE_MAGIC )  continue;
			tlu = teu->up.lu_p;
			NMG_CK_LOOPUSE(tlu);
			if( tlu != lu )  continue;
			/*
			 *  Repeated vertex exists,
			 *  Split loop into two loops
			 */
			newlu = nmg_split_lu_at_vu( lu, vu );
			NMG_CK_LOOPUSE(newlu);
			NMG_CK_LOOP(newlu->l_p);
			nmg_loop_g(newlu->l_p, tol);

			/* Ensure there are no duplications in new loop */
			nmg_split_touchingloops(newlu, tol);

			/* There is no telling where we will be in the
			 * remainder of original loop, check 'em all.
			 */
			goto top;
		}
	}
#endif
}

/*
 *			N M G _ J O I N _ T O U C H I N G L O O P S
 *
 *  Search through all the vertices in a loopuse that belongs to a faceuse.
 *  Whenever another loopuse in the same faceuse refers to one of this
 *  loop's vertices, the two loops touch at (at least) that vertex.
 *  Join them together.
 *
 *  Return -
 *	count of loops joined (eliminated)
 */
int
nmg_join_touchingloops( lu )
struct loopuse	*lu;
{
	struct faceuse		*fu;
	struct edgeuse		*eu;
	struct vertexuse	*vu;
	struct vertex		*v;
	int			count = 0;

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_join_touchingloops( lu=x%x )\n", lu);
	}
	NMG_CK_LOOPUSE(lu);
	fu = lu->up.fu_p;
	NMG_CK_FACEUSE(fu);

top:
	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return count;

	/* For each edgeuse, get vertexuse and vertex */
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
		struct vertexuse	*tvu;

		vu = eu->vu_p;
		NMG_CK_VERTEXUSE(vu);
		v = vu->v_p;
		NMG_CK_VERTEX(v);

		/*
		 *  For each vertexuse on vertex list,
		 *  check to see if it points up to an edgeuse in a
		 *  different loopuse in the same faceuse.
		 *  If so, we touch.
		 */
		for( BU_LIST_FOR( tvu, vertexuse, &v->vu_hd ) )  {
			struct edgeuse		*teu;
			struct loopuse		*tlu;

			if( tvu == vu )  continue;
			if( *tvu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
			teu = tvu->up.eu_p;
			NMG_CK_EDGEUSE(teu);
			if( *teu->up.magic_p != NMG_LOOPUSE_MAGIC )  continue;
			tlu = teu->up.lu_p;
			NMG_CK_LOOPUSE(tlu);
			if( tlu == lu )  {
				/* We touch ourselves at another vu? */
#if 0
				bu_log("INFO: nmg_join_touchingloops() lu=x%x touches itself at vu1=x%x, vu2=x%x, skipping\n",
					lu, vu, tvu );
#endif
				continue;
			}
			if( *tlu->up.magic_p != NMG_FACEUSE_MAGIC )  continue;
			if( tlu->up.fu_p != fu )  continue;	/* wrong faceuse */
			/*
			 *  Loop 'lu' touches loop 'tlu' at this vertex,
			 *  join them.
			 *  XXX Is there any advantage to searching for
			 *  XXX a potential shared edge at this point?
			 *  XXX Call nmg_simplify_loop()?
			 */
			if (rt_g.NMG_debug & DEBUG_BASIC)  {
				bu_log("nmg_join_touchingloops(): lu=x%x, vu=x%x, tvu=x%x\n", lu, vu, tvu);
			}
			tvu = nmg_join_2loops( vu, tvu );
			NMG_CK_VERTEXUSE(tvu);
			count++;
			goto top;
		}
	}
	return count;
}

/* jaunt status flags used in the jaunt_status array */
#define		JS_UNKNOWN		0
#define		JS_SPLIT		1
#define		JS_JAUNT		2
#define		JS_TOUCHING_JAUNT	3

/*	N M G _ G E T _ T O U C H I N G _ J A U N T S
 *
 * Create a table of EU's. Each EU will be the first EU in 
 * a touching jaunt (edgeuses from vert A->B->A) where vertex B
 * appears elswhere in the loopuse lu.
 *
 * returns:
 *	count of touching jaunts found
 *
 * The passed pointer to an bu_ptbl structure may
 * not be initialized. If no touching jaunts are found,
 * it will still not be initialized upon return (to avoid
 * rt_malloc/rt_free pairs for loops with no touching
 * jaunts. The flag (need_init) lets this routine know
 * whether the ptbl structure has been initialized
 */

int
nmg_get_touching_jaunts( lu, tbl, need_init )
CONST struct loopuse *lu;
struct bu_ptbl *tbl;
int *need_init;
{
	struct edgeuse *eu;
	int count=0;

	NMG_CK_LOOPUSE( lu );

	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return( 0 );

	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct edgeuse *eu2,*eu3;

		eu2 = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
		eu3 = BU_LIST_PNEXT_CIRC(edgeuse, &eu2->l);

		/* If it's a 2 vertex crack, stop here */
		if( eu->vu_p == eu3->vu_p )  break;

		/* If not jaunt, move on */
		if( eu->vu_p->v_p != eu3->vu_p->v_p )  continue;

		/* It's a jaunt, see if tip touches same loop */
		if( nmg_find_repeated_v_in_lu(eu2->vu_p) == (struct vertexuse *)NULL )
			continue;

		/* This is a touching jaunt, add it to the list */
		if( *need_init )
		{
			bu_ptbl_init( tbl, 64, " tbl");
			*need_init = 0;
		}

		bu_ptbl_ins( tbl, (long *)eu );
		count++;
	}

	return( count );
}

/*	N M G _ C H E C K _ P R O P O S E D _ L O O P
 *
 * Support routine for nmg_loop_split_at_touching_jaunt().
 *
 * Follow the edgeuses in a proposed loop (that may later be created by nmg_split_lu_at_vu)
 * and predict the status of the jaunts in "jaunt_tbl". The jaunt_status array must be
 * initialized before this routine is called and this routine should be called twice
 * to account for both loops that will result from the application of nmg_split_lu_at_vu().
 *
 * input:
 *	start_eu - edgeuse that will be first in the proposed new loop
 *	jaunt_tbl - list of touching jaunts whose status in the resulting loops is desired.
 *	jaunt_no - which entry in the jaunt_tbl is being considered as a split location
 *	visit_count - array for counting the number of times a jaunt gets visited in the new loop
 *		This array just provides working space for the routine.
 *	which_loop - Flag:
 *		0 -> This is a proposed loop to be created  by nmg_split_lu_at_vu()
 *		1 -> This is the loop that will be remaining after nmg_split_lu_at_vu().
 *		when "which_loop" == 1, (*next_start_eu) must be set to the starting EU
 *		of the first loop (Used to find end of remaining loop).
 *
 * output:
 *	jaunt_status - status of each touching jaunt that appears in the jaunt_tbl
 *	next_start_eu - edgeuse that will be first in the next loop
 */

static void
nmg_check_proposed_loop( start_eu, next_start_eu, visit_count, jaunt_status, jaunt_tbl, jaunt_no, which_loop )
struct edgeuse *start_eu;
struct edgeuse **next_start_eu;
int visit_count[];
int jaunt_status[];
CONST struct bu_ptbl *jaunt_tbl;
CONST int jaunt_no;
CONST int which_loop;
{
	struct edgeuse *loop_eu;
	struct edgeuse *last_eu;
	int j;
	int done=0;

	NMG_CK_EDGEUSE( start_eu );
	BU_CK_PTBL( jaunt_tbl );

	/* Initialize the count */
	for( j=0 ; j< BU_PTBL_END( jaunt_tbl ) ; j++ )
		visit_count[j] = 0;

	/* walk through the proposed new loop, updating the visit_count and the jaunt status */
	last_eu = NULL;
	loop_eu = start_eu;
	while( !done )
	{
		for( j=0 ; j<BU_PTBL_END( jaunt_tbl ) ; j++ )
		{
			struct edgeuse *jaunt_eu;

			/* Don't worru about this jaunt */
			if( j == jaunt_no )
				continue;

			jaunt_eu = (struct edgeuse *)BU_PTBL_GET( jaunt_tbl, j );
			NMG_CK_EDGEUSE( jaunt_eu );

			/* if jaunt number j is included in this loop, update its status */
			if( last_eu && last_eu == jaunt_eu )
				jaunt_status[j] = JS_JAUNT;

			/* if this loop_eu has its vertex at the apex of one of
			 * the touching jaunts, increment the appropriate visit_count.
			 */
			if( loop_eu->vu_p->v_p == jaunt_eu->eumate_p->vu_p->v_p )
				visit_count[j]++;
		}
		last_eu = loop_eu;
		loop_eu = BU_LIST_PNEXT_CIRC(edgeuse, &loop_eu->l);

		/* if "which_loop" is 0, use the nmg_split_lu_at_vu() terminate condition,
		 * otherwise, continue until we find (*next_start_eu)
		 */
		if( !which_loop && loop_eu->vu_p->v_p == start_eu->vu_p->v_p )
			done = 1;
		else if( which_loop && loop_eu == (*next_start_eu) )
			done = 1;
	}
	*next_start_eu = loop_eu;


	/* check all jaunts to see if they are still touching jaunts in the proposed loop */
	for( j=0 ; j<BU_PTBL_END( jaunt_tbl ) ; j++ )
	{
		if( jaunt_status[j] == JS_JAUNT ) /* proposed loop contains this jaunt */
		{
			if( visit_count[j] > 1 )	/* it's still a touching jaunt */
				jaunt_status[j] = JS_TOUCHING_JAUNT;
		}
	}

	/* if the first or last eu in the proposed loop is a jaunt eu, the proposed
	 * loop will split the jaunt, so set its status to JS_SPLIT.
	 */
	for( j=0 ; j<BU_PTBL_END( jaunt_tbl ) ; j++ )
	{
		struct edgeuse *jaunt_eu;
		struct edgeuse *jaunt_eu2;

		jaunt_eu = (struct edgeuse *)BU_PTBL_GET( jaunt_tbl, j );
		jaunt_eu2 = BU_LIST_PNEXT_CIRC( edgeuse, &jaunt_eu->l );

		if( last_eu == jaunt_eu || start_eu == jaunt_eu2 )
			jaunt_status[j] = JS_SPLIT;
	}

	return;
}

void
nmg_kill_accordions( lu )
struct loopuse *lu;
{
	struct edgeuse *eu;
	struct edgeuse *jaunt_eu1;
	struct edgeuse *jaunt_eu2;
	struct edgeuse *jaunt_eu3;

	NMG_CK_LOOPUSE( lu );

	if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return;
top:
	/* first look for a jaunt */
	jaunt_eu1 = (struct edgeuse *)NULL;
	jaunt_eu3 = (struct edgeuse *)NULL;
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct edgeuse *eu2;

		eu2 = BU_LIST_PPREV_CIRC( edgeuse, &eu->l );
		if( (eu2->vu_p->v_p == eu->eumate_p->vu_p->v_p) && (eu != eu2) )
		{
			struct edgeuse *eu3;

			/* found a jaunt */
			jaunt_eu1 = eu;
			jaunt_eu2 = eu2;

			/* look for a third eu between the same vertices */
			jaunt_eu3 = (struct edgeuse *)NULL;
			for( BU_LIST_FOR( eu3, edgeuse, &lu->down_hd ) )
			{
				if( eu3 == jaunt_eu1 || eu3 == jaunt_eu2 )
					continue;

				if( NMG_ARE_EUS_ADJACENT( eu3, jaunt_eu1 ) )
				{
					jaunt_eu3 = eu3;
					break;
				}
			}
		}
		if( jaunt_eu3 )
			break;
	}

	if( !jaunt_eu3 )
		return;

	if( jaunt_eu2 != jaunt_eu1->eumate_p )
	{
		if ((rt_g.NMG_debug & DEBUG_BASIC) || (rt_g.NMG_debug & DEBUG_CUTLOOP))
			bu_log( "Killing jaunt in accordion eu's x%x and x%x\n", jaunt_eu1, jaunt_eu2 );
		(void)nmg_keu( jaunt_eu1 );
		(void)nmg_keu( jaunt_eu2 );
	}
	else
	{
		if ((rt_g.NMG_debug & DEBUG_BASIC) || (rt_g.NMG_debug & DEBUG_CUTLOOP))
			bu_log( "Killing jaunt in accordion eu x%x\n", jaunt_eu1 );
		(void)nmg_keu( jaunt_eu1 );
	}

	goto top;

}

/*
 *			N M G _ L O O P _ S P L I T _ A T _ T O U C H I N G _ J A U N T
 *
 *  If a loop makes a "jaunt" (edgeuses from verts A->B->A), where the
 *  tip of the jaunt touches the same loop at a different vertexuse,
 *  cut the loop into two.
 *
 *  This produces a much more reasonable loop split than
 *  nmg_split_touchingloops(), which tends to peel off 2-edge "cracks"
 *  as it unravels the loop.
 *
 *  Note that any loops so split will be marked OT_UNSPEC.
 */
int
nmg_loop_split_at_touching_jaunt(lu, tol)
struct loopuse		*lu;
CONST struct bn_tol	*tol;
{
	struct bu_ptbl		jaunt_tbl;
	struct loopuse		*new_lu;
	struct edgeuse		*eu;
	struct edgeuse		*eu2;
	int			count=0;
	int			jaunt_count;
	int			jaunt_no;
	int			need_init=1;
	int			*visit_count;
	int			*jaunt_status;
	int			i,j;

	NMG_CK_LOOPUSE(lu);
	BN_CK_TOL(tol);

	if ((rt_g.NMG_debug & DEBUG_BASIC) || (rt_g.NMG_debug & DEBUG_CUTLOOP))  {
		bu_log("nmg_loop_split_at_touching_jaunt( lu=x%x ) START\n", lu);
		nmg_pr_lu_briefly( lu, "" );
	}

	/* first kill any accordion pleats */
	nmg_kill_accordions( lu );

	visit_count = (int *)NULL;
	jaunt_status = (int *)NULL;

top:
	jaunt_count = nmg_get_touching_jaunts( lu, &jaunt_tbl, &need_init );
	if( rt_g.NMG_debug & DEBUG_CUTLOOP )
	{
		bu_log( "nmg_loop_split_at_touching_jaunt: found %d touching jaunts in lu x%x\n", jaunt_count, lu );
		if( jaunt_count )
		{
			for( i=0 ; i<BU_PTBL_END( &jaunt_tbl ) ; i++ )
			{
				eu = (struct edgeuse *)BU_PTBL_GET( &jaunt_tbl, i );
				bu_log( "\tx%x\n" , eu );
			}
		}
	}

	if( jaunt_count < 0 )
	{
		bu_log( "nmg_loop_split_at_touching_jaunt: nmg_get_touching_jaunts() returned %d for lu x%x\n", jaunt_count, lu );
		rt_bomb( "nmg_loop_split_at_touching_jaunt: bad jaunt count\n" );
	}

	if( jaunt_count == 0 )
	{
		if( visit_count )
			rt_free( (char *)visit_count, "nmg_loop_split_at_touching_jaunt: visit_count[]\n" );
		if( jaunt_status )
			rt_free( (char *)jaunt_status, "nmg_loop_split_at_touching_jaunt: jaunt_status[]\n" );
		if( !need_init )
			bu_ptbl_free( &jaunt_tbl);

		if ((rt_g.NMG_debug & DEBUG_BASIC) || (rt_g.NMG_debug & DEBUG_CUTLOOP))  {
			bu_log("nmg_loop_split_at_touching_jaunt( lu=x%x ) END count=%d\n",
				lu, count);
		}
		return( count );
	}

	if( jaunt_count == 1 )
	{
		/* just one jaunt, split it and return */
		BU_CK_PTBL( &jaunt_tbl );

		eu = (struct edgeuse *)BU_PTBL_GET( &jaunt_tbl, 0 );
		NMG_CK_EDGEUSE( eu );
		eu2 = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);

		new_lu = nmg_split_lu_at_vu( lu, eu2->vu_p );
		count++;
		NMG_CK_LOOPUSE(new_lu);
		NMG_CK_LOOP(new_lu->l_p);
		nmg_loop_g(new_lu->l_p, tol);

		bu_ptbl_free( &jaunt_tbl);
		if( visit_count )
			rt_free( (char *)visit_count, "nmg_loop_split_at_touching_jaunt: visit_count[]\n" );
		if( jaunt_status )
			rt_free( (char *)jaunt_status, "nmg_loop_split_at_touching_jaunt: jaunt_status[]\n" );

		if ((rt_g.NMG_debug & DEBUG_BASIC) || (rt_g.NMG_debug & DEBUG_CUTLOOP))  {
			bu_log("nmg_loop_split_at_touching_jaunt( lu=x%x ) END count=%d\n",
				lu, count);
		}
		return( count );
	}

	/* if we get here, there are at least two touching jaunts in the loop.
	 * We need to select which order to split them to avoid converting the
	 * touching jaunt into a crack. To do this, select one touching jaunt
	 * as the location for splitting the loop. Follow the EU's as nmg_split_lu_at_vu()
	 * would do and predict the status of ALL the touching jaunts (not just the one being
	 * split). Both the new loop and the remains of the current loop must be checked
	 * to determine the status of all the touching jaunts.
	 * The original touching jaunts must either remain as touching jaunts in
	 * one of the two resulting loops, or they must be split between the two
	 * resulting loops. If any of the touching jaunts are predicted to have a status
	 * other than split or still touching, then this is a bad choice for splitting
	 * the loop. For example, a touching jaunt may be converted to a non-touching
	 * jaunt by applying nmg_split_lu_at_vu() at the wrong vu and will later be
	 * converted to a two edgeuse loop by nmg_split_touching_loops.
	 */
	BU_CK_PTBL( &jaunt_tbl );

	if( visit_count )
		rt_free( (char *)visit_count, "nmg_loop_split_at_touching_jaunt: visit_count[]\n" );
	if( jaunt_status )
		rt_free( (char *)jaunt_status, "nmg_loop_split_at_touching_jaunt: jaunt_status[]\n" );

	visit_count = (int *)rt_calloc( BU_PTBL_END( &jaunt_tbl ), sizeof( int ),
			"nmg_loop_split_at_touching_jaunt: visit_count[]" );
	jaunt_status = (int *)rt_calloc( BU_PTBL_END( &jaunt_tbl ), sizeof( int ),
			"nmg_loop_split_at_touching_jaunt: jaunt_status[]" );

	/* consider each jaunt as a possible location for splitting the loop */
	for( jaunt_no=0 ; jaunt_no<BU_PTBL_END( &jaunt_tbl ) ; jaunt_no++ )
	{
		struct edgeuse *start_eu1;	/* EU that will start a new loop upon split */
		struct edgeuse *start_eu2;	/* EU that will start the remaining loop */
		int do_split=1;			/* flag: 1 -> this jaunt is a good choice for making the split */

		/* initialize the status of each jaunt to unknown */
		for( i=0 ; i<BU_PTBL_END( &jaunt_tbl ) ; i++ )
			jaunt_status[i] = JS_UNKNOWN;

		/* consider splitting lu at this touching jaunt */
		eu = (struct edgeuse *)BU_PTBL_GET( &jaunt_tbl, jaunt_no );
		NMG_CK_EDGEUSE( eu );

		/* get new loop starting edgeuse */
		start_eu1 = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);

		if( rt_g.NMG_debug & DEBUG_CUTLOOP )
		{
			bu_log( "\tConsider splitting lu x%x at vu=x%x\n", lu, start_eu1->vu_p );
			bu_log( "\t\t(jaunt number %d\n" , jaunt_no );	
		}

		/* determine status of jaunts in the proposed new loop */
		nmg_check_proposed_loop( start_eu1, &start_eu2, visit_count,
			jaunt_status, &jaunt_tbl, jaunt_no, 0 );

		/* Still need to check the loop that will be left if we do a split here */
		nmg_check_proposed_loop( start_eu2, &start_eu1, visit_count,
			jaunt_status, &jaunt_tbl, jaunt_no, 1 );

		if( rt_g.NMG_debug & DEBUG_CUTLOOP )
		{
			for( i=0 ; i<BU_PTBL_END( &jaunt_tbl ) ; i++ )
			{
				struct edgeuse *tmp_eu;

				tmp_eu = (struct edgeuse *)BU_PTBL_GET( &jaunt_tbl, i );
				bu_log( "\t\tpredicted status of jaunt at eu x%x is ", tmp_eu );
				switch( jaunt_status[i] )
				{
					case JS_UNKNOWN:
						bu_log( "unknown\n" );
						break;
					case JS_SPLIT:
						bu_log( "split\n" );
						break;
					case JS_JAUNT:
						bu_log( "a jaunt\n" );
						break;
					case JS_TOUCHING_JAUNT:
						bu_log( "still a touching jaunt\n" );
						break;
					default:
						bu_log( "unrecognized status\n" );
						break;
				}
			}
		}

		/* check predicted status of all the touching jaunts,
		 * every one must be either split or still a touching jaunt.
		 * Any other status will create loops of two edgeuses!!!
		 */
		for( i=0 ; i<BU_PTBL_END( &jaunt_tbl ) ; i++ )
		{
			if(jaunt_status[i] != JS_SPLIT && jaunt_status[i] != JS_TOUCHING_JAUNT )
			{
				do_split = 0;
				break;
			}
		}

		/* if splitting lu at this touching jaunt is not desirable, check the next one */
		if( !do_split )
		{
			if( rt_g.NMG_debug & DEBUG_CUTLOOP )
			{
				bu_log( "\t\tCannot split lu x%x at proposed vu\n" );
			}
			continue;
		}

		/* This touching jaunt passed all the tests, its reward is to be split */
		eu2 = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);

		new_lu = nmg_split_lu_at_vu( lu, eu2->vu_p );

		if( rt_g.NMG_debug & DEBUG_CUTLOOP )
		{
			bu_log( "\tnmg_loop_split_at_touching_jaunt: splitting lu x%x at vu x%x\n", lu, eu2->vu_p );
			bu_log( "\t\tnew_lu is x%x\n" , new_lu );
		}

		count++;
		NMG_CK_LOOPUSE(new_lu);
		NMG_CK_LOOP(new_lu->l_p);
		nmg_loop_g(new_lu->l_p, tol);
		bu_ptbl_reset( &jaunt_tbl);

		/* recurse on the new loop */
		count += nmg_loop_split_at_touching_jaunt( new_lu, tol );

		/* continue with the remains of the current loop */
		goto top;
	}

	/* If we ever get here, we have failed to find a way to split this loop!!!! */
	bu_log( "nmg_loop_split_at_touching_jaunt: Could not find a way to split lu x%x\n", lu );
	nmg_pr_lu_briefly( lu, " " );
	nmg_stash_model_to_file( "jaunt.g", nmg_find_model( &lu->l.magic ), "Can't split lu" );
	rt_bomb( "nmg_loop_split_at_touching_jaunt: Can't split lu\n" );

	/* This return will never execute, but the compilers like it */
	return( count );
}

/*			N M G _ S I M P L I F Y _ L O O P
 *
 *	combine adjacent loops within the same parent that touch along
 *	a common edge into a single loop, with the edge eliminated.
 */
void
nmg_simplify_loop(lu)
struct loopuse *lu;
{
	struct edgeuse *eu, *eu_r, *tmpeu;

	NMG_CK_LOOPUSE(lu);
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_simplify_loop(lu=x%x)\n", lu);
	}

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		return;

	eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	while (BU_LIST_NOT_HEAD(eu, &lu->down_hd) ) {

		NMG_CK_EDGEUSE(eu);

		eu_r = eu->radial_p;
		NMG_CK_EDGEUSE(eu_r);

		/* if the radial edge is part of a loop, and the loop of
		 * the other edge is a part of the same object (face)
		 * as the loop containing the current edge, and my
		 * edgeuse mate is radial to my radial`s edgeuse
		 * mate, and the radial edge is a part of a loop other
		 * than the one "eu" is a part of 
		 * then this is a "worthless" edge between these two loops.
		 */
		if (*eu_r->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    eu_r->up.lu_p->up.magic_p == lu->up.magic_p &&
		    eu->eumate_p->radial_p == eu->radial_p->eumate_p &&
		    eu_r->up.lu_p != lu) {

		    	if( eu_r->up.lu_p->orientation != lu->orientation &&
		    	   (lu->orientation != OT_SAME ||
			    eu_r->up.lu_p->orientation != OT_OPPOSITE) )  {
				/* Does not meet requirements of nmg_jl(),
				 * skip it.
				 */
			    	eu = BU_LIST_PNEXT(edgeuse, eu);
				continue;
			}

		    	/* save a pointer to where we've already been
		    	 * so that when eu becomes an invalid pointer, we
		    	 * still know where to pick up from.
		    	 */
		    	tmpeu = BU_LIST_PLAST(edgeuse, eu);

			nmg_jl(lu, eu);

		    	/* Since all the new edges will have been appended
		    	 * after tmpeu, we can pick up processing with the
		    	 * edgeuse immediately after tmpeu
		    	 */
		    	eu = tmpeu;

		    	if (rt_g.NMG_debug &(DEBUG_PLOTEM|DEBUG_PL_ANIM) &&
			    *lu->up.magic_p == NMG_FACEUSE_MAGIC ) {
		    	    	static int fno=0;

				nmg_pl_2fu("After_joinloop%d.pl", fno++,
				    lu->up.fu_p, lu->up.fu_p->fumate_p, 0);
					
		    	}
		}
		eu = BU_LIST_PNEXT(edgeuse, eu);
	}
}


/*
 *			N M G _ K I L L _ S N A K E S
 *
 *  Removes "snake" or "disconnected crack" edges from loopuse.
 *
 *  Returns -
 *	0	If all went well
 *	1	If the loopuse is now empty and needs to be killed.
 */
int
nmg_kill_snakes(lu)
struct loopuse *lu;
{
	struct edgeuse *eu, *eu_r;
	struct vertexuse *vu;
	struct vertex *v;

	NMG_CK_LOOPUSE(lu);
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_kill_snakes(lu=x%x)\n", lu);
	}
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		return 0;

	eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	while (BU_LIST_NOT_HEAD(eu, &lu->down_hd) ) {

		NMG_CK_EDGEUSE(eu);

		eu_r = eu->radial_p;
		NMG_CK_EDGEUSE(eu_r);

		/* if the radial edge is a part of the same loop, and
		 * this edge is not used by anyplace else, and the radial edge
		 * is also the next edge, this MAY be the tail of a snake!
		 */

		if (*eu_r->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    eu_r->up.lu_p == eu->up.lu_p &&
		    eu->eumate_p->radial_p == eu->radial_p->eumate_p &&
		    BU_LIST_PNEXT_CIRC(edgeuse, eu) == eu_r) {

		    	/* if there are no other uses of the vertex
		    	 * between these two edgeuses, then this is
		    	 * indeed the tail of a snake
		    	 */
			v = eu->eumate_p->vu_p->v_p;
			vu = BU_LIST_FIRST(vertexuse, &v->vu_hd);
			while (BU_LIST_NOT_HEAD(vu, &v->vu_hd) &&
			      (vu->up.eu_p == eu->eumate_p ||
			       vu->up.eu_p == eu_r) )
				vu = BU_LIST_PNEXT(vertexuse, vu);

			if (! BU_LIST_NOT_HEAD(vu, &v->vu_hd) ) {
				/* this is the tail of a snake! */
				(void)nmg_keu(eu_r);
				if( nmg_keu(eu) )  return 1; /* kill lu */
				if( BU_LIST_IS_EMPTY( &lu->down_hd ) )
					return 1;	/* loopuse is empty */
				eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);

			    	if (rt_g.NMG_debug &(DEBUG_PLOTEM|DEBUG_PL_ANIM) &&
				    *lu->up.magic_p == NMG_FACEUSE_MAGIC ) {
			    	    	static int fno=0;

					nmg_pl_2fu("After_joinloop%d.pl", fno++,
					    lu->up.fu_p, lu->up.fu_p->fumate_p, 0);

			    	}


			} else
				eu = BU_LIST_PNEXT(edgeuse, eu);
		} else
			eu = BU_LIST_PNEXT(edgeuse, eu);
	}
	return 0;	/* All is well, loop still has edges */
}

/*
 *			N M G _ M V _ L U _ B E T W E E N _ S H E L L S
 *
 *  Move a wire-loopuse from one shell to another.
 *  Note that this routine can not be used on loops in faces.
 */
void
nmg_mv_lu_between_shells( dest, src, lu )
struct shell		*dest;
register struct shell	*src;
register struct loopuse	*lu;
{
	register struct loopuse	*lumate;

	NMG_CK_LOOPUSE(lu);
	lumate = lu->lumate_p;
	NMG_CK_LOOPUSE(lumate);

	if( lu->up.s_p != src )  {
		bu_log("nmg_mv_lu_between_shells(dest=x%x, src=x%x, lu=x%x), lu->up.s_p=x%x isn't source shell\n",
			dest, src, lu, lu->up.s_p );
		rt_bomb("lu->up.s_p isn't source shell\n");
	}
	if( lumate->up.s_p != src )  {
		bu_log("nmg_mv_lu_between_shells(dest=x%x, src=x%x, lu=x%x), lumate->up.s_p=x%x isn't source shell\n",
			dest, src, lu, lumate->up.s_p );
		rt_bomb("lumate->up.s_p isn't source shell\n");
	}

	/* Remove lu from src shell */
	BU_LIST_DEQUEUE( &lu->l );
	if( BU_LIST_IS_EMPTY( &src->lu_hd ) )  {
		/* This was the last lu in the list */
		bu_log("nmg_mv_lu_between_shells(dest=x%x, src=x%x, lu=x%x), lumate=x%x not in src shell\n",
			dest, src, lu, lumate );
		rt_bomb("src shell emptied before finding lumate\n");
	}

	/* Remove lumate from src shell */
	BU_LIST_DEQUEUE( &lumate->l );

	/* Add lu and lumate to dest shell */
	BU_LIST_APPEND( &dest->lu_hd, &lu->l );
	BU_LIST_APPEND( &lu->l, &lumate->l );

	lu->up.s_p = dest;
	lumate->up.s_p = dest;

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_mv_lu_between_shells(dest=x%x, src=x%x, lu=x%x)\n",
			dest , src , lu);
	}
}

/*
 *			N M G _ M O V E L T O F
 *
 *	move first pair of shell wire loopuses out to become a genuine loop
 *	in an existing face.
 * XXX This routine is not used anywhere, and may not work.
 */
void nmg_moveltof(fu, s)
struct faceuse *fu;
struct shell *s;
{
	struct loopuse	*lu1, *lu2;

	NMG_CK_SHELL(s);
	NMG_CK_FACEUSE(fu);
	if (fu->s_p != s) {
		bu_log("nmg_moveltof() in %s at %d. Cannot move loop to face in another shell\n",
		    __FILE__, __LINE__);
	}
	lu1 = BU_LIST_FIRST(loopuse, &s->lu_hd);
	NMG_CK_LOOPUSE(lu1);
	BU_LIST_DEQUEUE( &lu1->l );

	lu2 = BU_LIST_FIRST(loopuse, &s->lu_hd);
	NMG_CK_LOOPUSE(lu2);
	BU_LIST_DEQUEUE( &lu2->l );

	BU_LIST_APPEND( &fu->lu_hd, &lu1->l );
	BU_LIST_APPEND( &fu->fumate_p->lu_hd, &lu2->l );
	/* XXX What about loopuse "up" pointers? */

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_moveltof(fu=x%x, s=x%x)\n",
			fu , s );
	}
}

/*
 *			N M G _ D U P _ L O O P
 *
 *  A support routine for nmg_dup_face()
 *
 *  trans_tbl may be NULL.
 */
struct loopuse *
nmg_dup_loop(lu, parent, trans_tbl)
struct loopuse *lu;
long	*parent;		/* fu or shell ptr */
long	**trans_tbl;
{
	struct loopuse *new_lu = (struct loopuse *)NULL;
	struct vertexuse *new_vu = (struct vertexuse *)NULL;
	struct vertexuse *old_vu = (struct vertexuse *)NULL;
	struct vertex *old_v = (struct vertex *)NULL;
	struct vertex	*new_v;
	struct edgeuse *new_eu = (struct edgeuse *)NULL;
	struct edgeuse *tbl_eu = (struct edgeuse *)NULL;
	struct edgeuse *eu = (struct edgeuse *)NULL;

	NMG_CK_LOOPUSE(lu);

	/* if loop is just a vertex, that's simple to duplicate */
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		old_vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
		old_v = old_vu->v_p;

		/* Obtain new duplicate of old vertex.  May be null 1st time. */
		if( trans_tbl )
			new_v = NMG_INDEX_GETP(vertex, trans_tbl, old_v);
		else
			new_v = (struct vertex *)NULL;
		new_lu = nmg_mlv(parent, new_v, lu->orientation);
		if (new_lu->orientation != lu->orientation) {
			bu_log("%s %d: I asked for a %s loop not a %s loop.\n",
				__FILE__, __LINE__,
				nmg_orientation(lu->orientation),
				nmg_orientation(new_lu->orientation));
			rt_bomb("bombing\n");
		}
		if( new_v )  {
			/* the new vertex already exists in the new model */
			bu_log("nmg_dup_loop() existing vertex in new model\n");
			return (struct loopuse *)NULL;
		}
		/* nmg_mlv made a new vertex */
		bu_log("nmg_dup_loop() new vertex in new model\n");

		new_vu = BU_LIST_FIRST(vertexuse, &new_lu->down_hd);
		new_v = new_vu->v_p;
		/* Give old_v entry a pointer to new_v */
		if( trans_tbl )
			NMG_INDEX_ASSIGN( trans_tbl, old_v, (long *)new_v );
		if (old_v->vg_p) {
			/* Build a different vertex_g with same coordinates */
			nmg_vertex_gv(new_v, old_v->vg_p->coord);
		}
		if (rt_g.NMG_debug & DEBUG_BASIC)  {
			bu_log("nmg_dup_loop(lu=x%x, parent=x%x, trans_tbl=x%x) new_lu=x%x\n",
				lu , parent , trans_tbl , new_lu );
		}
		return new_lu;
	}

	/* This loop is an edge-loop.  This is a little more work
	 * First order of business is to duplicate the vertex/edge makeup.
	 */
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {

		NMG_CK_EDGEUSE(eu);
		NMG_CK_VERTEXUSE(eu->vu_p);
		NMG_CK_VERTEX(eu->vu_p->v_p);
		old_v = eu->vu_p->v_p;

		/* Obtain new duplicate of old vertex.  May be null 1st time. */
		if( trans_tbl )
			new_v = NMG_INDEX_GETP(vertex, trans_tbl, old_v);
		else
			new_v = (struct vertex *)NULL;

		if (new_lu == (struct loopuse *)NULL) {
			/* this is the first edge in the new loop */
			new_lu = nmg_mlv(parent, new_v, lu->orientation);
			if (new_lu->orientation != lu->orientation) {
				bu_log("%s %d: I asked for a %s loop not a %s loop.\n",
					__FILE__, __LINE__,
					nmg_orientation(lu->orientation),
					nmg_orientation(new_lu->orientation));
				rt_bomb("bombing\n");
			}

			new_vu = BU_LIST_FIRST(vertexuse, &new_lu->down_hd);

			NMG_CK_VERTEXUSE(new_vu);
			NMG_CK_VERTEX(new_vu->v_p);

			if( !new_v && trans_tbl )  {
				/* Give old_v entry a pointer to new_v */
				NMG_INDEX_ASSIGN( trans_tbl, old_v,
					(long *)new_vu->v_p );
			}
			new_v = new_vu->v_p;

			new_eu = nmg_meonvu(new_vu);
		} else {
			/* not the first edge in new loop */
			new_eu = BU_LIST_LAST(edgeuse, &new_lu->down_hd);
			NMG_CK_EDGEUSE(new_eu);

			new_eu = nmg_eusplit(new_v, new_eu, 0);
			new_vu = new_eu->vu_p;

			if( !new_v && trans_tbl )  {
				/* Give old_v entry a pointer to new_v */
				NMG_INDEX_ASSIGN( trans_tbl, old_v,
					(long *)new_vu->v_p );
			}
			new_v = new_vu->v_p;
		}
		/* Build a different vertex_g with same coordinates */
		if (old_v->vg_p) {
			NMG_CK_VERTEX_G(old_v->vg_p);
			nmg_vertex_gv(new_v, old_v->vg_p->coord);
		}

		/* Prepare to glue edges */
		/* Use old_e as subscript, to get 1st new_eu (for new_e) */
		/* Use old_eu to get mapped new_eu */
		if( trans_tbl )
			tbl_eu = NMG_INDEX_GETP(edgeuse, trans_tbl, eu->e_p );
		else
			tbl_eu = (struct edgeuse *)NULL;
		if( !tbl_eu && trans_tbl )  {
			/* Establishes map from old edge to new edge(+use) */
			NMG_INDEX_ASSIGN( trans_tbl, eu->e_p, (long *)new_eu );
		}
		if( trans_tbl )
			NMG_INDEX_ASSIGN( trans_tbl, eu, (long *)new_eu );
	}
#if 0
/* XXX untested */
	if( trans_tbl )
	{
		/* All vertex structs are shared.  Make shared edges be shared */
		for(BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			/* Use old_e as subscript, to get 1st new_eu (for new_e) */
			/* Use old_eu to get mapped new_eu */
			tbl_eu = NMG_INDEX_GETP(edgeuse, trans_tbl, eu->e_p );
			new_eu = NMG_INDEX_GETP(edgeuse, trans_tbl, eu );
			if( tbl_eu->e_p == new_eu->e_p )  continue;

			/* new_eu isn't sharing edge with tbl_eu, join them */
			/* XXX Is radial relationship preserved (enough)? */
			nmg_je( tbl_eu, new_eu );
		}
	}
#endif

	/* Now that we've got all the right topology created and the
	 * vertex geometries are in place we can create the edge geometries.
	 * XXX This ought to be optional, as most callers will immediately
	 * XXX change the vertex geometry anyway (e.g. by extrusion dist).
	 */
	for(BU_LIST_FOR(new_eu, edgeuse, &new_lu->down_hd)) {
		NMG_CK_EDGEUSE(new_eu);
		NMG_CK_EDGE(new_eu->e_p);
		if( new_eu->g.magic_p )  continue;
		nmg_edge_g(new_eu);
	}

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_dup_loop(lu=x%x(%s), parent=x%x, trans_tbl=x%x) new_lu=x%x(%s)\n",
			lu, nmg_orientation(lu->orientation),
			parent , trans_tbl , new_lu,
			nmg_orientation(new_lu->orientation) );
	}
	return (new_lu);
}

/*
 *			N M G _ S E T _ L U _ O R I E N T A T I O N
 *
 *  Set this loopuse and mate's orientation to be SAME or OPPOSITE
 *  from the orientation of the faceuse they each reside in.
 */
void
nmg_set_lu_orientation( lu, is_opposite )
struct loopuse	*lu;
int		is_opposite;
{
	NMG_CK_LOOPUSE(lu);
	NMG_CK_LOOPUSE(lu->lumate_p);
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_set_lu_orientation(lu=x%x, %s)\n",
			lu, is_opposite?"OT_OPPOSITE":"OT_SAME");
	}
	if( is_opposite )  {
		/* Interior (crack) loop */
		lu->orientation = OT_OPPOSITE;
		lu->lumate_p->orientation = OT_OPPOSITE;
	} else {
		/* Exterior loop */
		lu->orientation = OT_SAME;
		lu->lumate_p->orientation = OT_SAME;
	}
}

/*
 *			N M G _ L U _ R E O R I E N T
 *
 *  Based upon a geometric calculation, reorient a loop and it's mate,
 *  if the stored orientation differs from the geometric one.
 *
 *  Note that the loopuse and it's mate have the same orientation;
 *  it's the faceuses that are normalward and anti-normalward.
 *  The loopuses either both agree with their faceuse, or both differ.
 */
void
nmg_lu_reorient( lu )
struct loopuse		*lu;
{
	struct faceuse	*fu;
	int	ccw;
	int	geom_orient;
	plane_t	norm;
	plane_t lu_pl;

	NMG_CK_LOOPUSE(lu);

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_lu_reorient(lu=x%x)\n", lu);
	}

	/* Don't harm the OT_BOOLPLACE self-loop marker vertices */
	if( lu->orientation == OT_BOOLPLACE &&
	    BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )
		return;

	fu = lu->up.fu_p;
	NMG_CK_FACEUSE(fu);
	if( fu->orientation != OT_SAME )  {
		lu = lu->lumate_p;
		NMG_CK_LOOPUSE(lu);
		fu = lu->up.fu_p;
		NMG_CK_FACEUSE(fu);
		if (rt_g.NMG_debug & DEBUG_BASIC)
			bu_log("nmg_lu_reorient() selecting other fu=x%x, lu=x%x\n", fu, lu);
		if( fu->orientation != OT_SAME )
			rt_bomb("nmg_lu_reorient() no OT_SAME fu?\n");
	}


	/* Get OT_SAME faceuse's normal */
	NMG_GET_FU_PLANE( norm, fu );
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		PLPRINT("\tfu peqn", norm);
	}

	nmg_loop_plane_newell( lu, lu_pl );

	if( lu->orientation == OT_OPPOSITE )
		HREVERSE( lu_pl, lu_pl );

	if( VDOT( lu_pl, norm ) < 0.0 )
		geom_orient = OT_OPPOSITE;
	else
		geom_orient = OT_SAME;

	if( lu->orientation == geom_orient )  return;
	if( rt_g.NMG_debug & DEBUG_BASIC )  {
		bu_log("nmg_lu_reorient(x%x):  changing orientation: %s to %s\n",
			lu, nmg_orientation(lu->orientation),
			nmg_orientation(geom_orient) );
	}

	lu->orientation = geom_orient;
	lu->lumate_p->orientation = geom_orient;
}

/************************************************************************
 *									*
 *				EDGE Routines				*
 *									*
 ************************************************************************/

/*
 *			N M G _ E U S P L I T
 *
 *	Split an edgeuse by inserting a vertex into middle of the edgeuse.
 *
 *	Make a new edge, and a vertex.  If v is non-null it is taken as a
 *	pointer to an existing vertex to use as the start of the new edge.
 *	If v is null, then a new vertex is created for the begining of the
 *	new edge.
 *
 *	In either case, the new edge will exist as the "next" edgeuse after
 *	the edgeuse passed as a parameter.
 *
 *	Upon return, the new edgeuses (eu1 and mate) will not refer to any
 *	geometry, unless argument "share_geom" was non-zero.
 *
 *  Explicit return -
 *	edgeuse of new edge "eu1", starting at V and going to B.
 *
 *  List on entry -
 *
 *		       oldeu
 *		  .------------->
 *		 /
 *		A =============== B (edge)
 *				 /
 *		  <-------------.
 *		      oldeumate
 *
 *  List on return -
 *
 *		     oldeu(cw)    eu1
 *		    .------->   .----->
 *		   /           /
 *	   (edge) A ========= V ~~~~~~~ B (new edge)
 *			     /         /
 *		    <-------.   <-----.
 *		       mate	 mate
 */
struct edgeuse *
nmg_eusplit(v, oldeu, share_geom)
struct vertex	*v;
struct edgeuse	*oldeu;
int		share_geom;
{
	struct edgeuse	*eu1,
			*eu2,
			*oldeumate;
	struct shell *s;
	struct loopuse	*lu;

	NMG_CK_EDGEUSE(oldeu);
	if (v) {
		NMG_CK_VERTEX(v);
	}
	oldeumate = oldeu->eumate_p;
	NMG_CK_EDGEUSE( oldeumate );

	/* if this edge has uses other than this edge and its mate, we must
	 * separate these two edgeuses from the existing edge, and create
	 * a new edge for them.  Then we can insert a new vertex in this
	 * new edge without fear of damaging some other object.
	 */
	if (oldeu->radial_p != oldeumate)
		nmg_unglueedge(oldeu);

	if (*oldeu->up.magic_p == NMG_SHELL_MAGIC) {
		s = oldeu->up.s_p;
		NMG_CK_SHELL(s);

		/*
		 *  Make an edge from the new vertex ("V") to vertex at
		 *  other end of the edge given ("B").
		 *  The new vertex "V" may be NULL, which will cause the
		 *  shell's lone vertex to be used, or a new one obtained.
		 *  New edges will be placed at head of shell's edge list.
		 */
		eu1 = nmg_me(v, oldeumate->vu_p->v_p, s);
		eu2 = eu1->eumate_p;

		/*
		 *  The situation is now:
		 *
		 *      eu1			       oldeu
		 *  .----------->		  .------------->
		 * /				 /
		 *V ~~~~~~~~~~~~~ B (new edge)	A =============== B (edge)
		 *		 /				 /
		 *  <-----------.		  <-------------.
		 *      eu2			      oldeumate
		 */

		/* Make oldeumate start at "V", not "B" */
		nmg_movevu(oldeumate->vu_p, eu1->vu_p->v_p);

		/*
		 *  Enforce rigid ordering in shell's edge list:
		 *	oldeu, oldeumate, eu1, eu2
		 *  This is to keep edges & mates "close to each other".
		 */
		if( BU_LIST_PNEXT(edgeuse, oldeu) != oldeumate )  {
			BU_LIST_DEQUEUE( &oldeumate->l );
			BU_LIST_APPEND( &oldeu->l, &oldeumate->l );
		}
		BU_LIST_DEQUEUE( &eu1->l );
		BU_LIST_DEQUEUE( &eu2->l );
		BU_LIST_APPEND( &oldeumate->l, &eu1->l );
		BU_LIST_APPEND( &eu1->l, &eu2->l );

		/*
		 *	     oldeu(cw)    eu1
		 *	    .------->   .----->
		 *	   /           /
		 * (edge) A ========= V ~~~~~~~ B (new edge)
		 *		     /         /
		 *	    <-------.   <-----.
		 *	    oldeumate     eu2
		 */
		if( share_geom )  {
			/* Make eu1 share geom with oldeu, eu2 with oldeumate */
			nmg_use_edge_g( eu1, oldeu->g.magic_p );
			nmg_use_edge_g( eu2, oldeumate->g.magic_p );
		} else {
			/* Make eu2 use same geometry as oldeu */
			nmg_use_edge_g( eu2, oldeu->g.magic_p );
			/* Now release geometry from oldeumate;  new edge has no geom */
			BU_LIST_DEQUEUE( &oldeumate->l2 );
			nmg_keg( oldeumate );
			BU_LIST_DEQUEUE( &eu1->l2 );
			nmg_keg( eu1 );
			BU_LIST_INIT( &oldeumate->l2 );
			BU_LIST_INIT( &eu1->l2 );
			oldeumate->l2.magic = NMG_EDGEUSE2_MAGIC;
			eu1->l2.magic = NMG_EDGEUSE2_MAGIC;
		}
		goto out;
	}
	else if (*oldeu->up.magic_p != NMG_LOOPUSE_MAGIC) {
		bu_log("nmg_eusplit() in %s at %d invalid edgeuse parent\n",
			__FILE__, __LINE__);
		rt_bomb("nmg_eusplit\n");
	}

	/* now we know we are in a loop */

	lu = oldeu->up.lu_p;
	NMG_CK_LOOPUSE(lu);

	/* get a parent shell pointer so we can make a new edge */
	if (*lu->up.magic_p == NMG_SHELL_MAGIC)
		s = lu->up.s_p;
	else if (*lu->up.magic_p == NMG_FACEUSE_MAGIC)
		s = lu->up.fu_p->s_p;
	else
		rt_bomb("nmg_eusplit() bad lu->up\n");
	NMG_CK_SHELL(s);

	/* Make a new wire edge in the shell */
	if (v) {
		/* An edge on the single vertex "V" */
		eu1 = nmg_me(v, v, s);
		eu2 = eu1->eumate_p;
	} else {
		/* Form a wire edge between two new vertices */
		eu1 = nmg_me((struct vertex *)NULL, (struct vertex *)NULL, s);
		eu2 = eu1->eumate_p;
		/* Make both ends of edge use same vertex.
		 * The second vertex is freed automaticly.
		 */
		nmg_movevu(eu2->vu_p, eu1->vu_p->v_p);
	}

	/*
	 *  The current situation is now:
	 *
	 *	      eu1				       oldeu
	 *	  .------------->			  .------------->
	 *	 /					 /
	 *	V ~~~~~~~~~~~~~~~ V (new edge)		A =============== B (edge)
	 *			 /					 /
	 *	  <-------------.			  <-------------.
	 *	      eu2				      oldeumate
	 *
	 *  Goals:
	 *  eu1 will become the mate to oldeumate on the existing edge.
	 *  eu2 will become the mate of oldeu on the new edge.
	 */
	BU_LIST_DEQUEUE( &eu1->l );
	BU_LIST_DEQUEUE( &eu2->l );
	BU_LIST_APPEND( &oldeu->l, &eu1->l );
	BU_LIST_APPEND( &oldeumate->l, &eu2->l );

	/*
	 *  The situation is now:
	 *
	 *		       oldeu      eu1			>>>loop>>>
	 *		    .------->   .----->
	 *		   /           /
	 *	   (edge) A ========= V ~~~~~~~ B (new edge)
	 *			     /         /
	 *		    <-------.   <-----.	
	 *		       eu2      oldeumate		<<<loop<<<
	 */

	/* Copy parentage (loop affiliation) and orientation */
	eu1->up.magic_p = oldeu->up.magic_p;
	eu1->orientation = oldeu->orientation;

	eu2->up.magic_p = oldeumate->up.magic_p;
	eu2->orientation = oldeumate->orientation;

	/* Build mate relationship */
	eu1->eumate_p = oldeumate;
	oldeumate->eumate_p = eu1;
	eu2->eumate_p = oldeu;
	oldeu->eumate_p = eu2;

	/*  Build radial relationship.
	 *  Simple only because this edge has no other uses.
	 */
	eu1->radial_p = oldeumate;
	oldeumate->radial_p = eu1;
	eu2->radial_p = oldeu;
	oldeu->radial_p = eu2;

	/* Associate oldeumate with new edge, and eu2 with old edge. */
	oldeumate->e_p = eu1->e_p;
	eu2->e_p = oldeu->e_p;

	/* Ensure that edge points up to one of the proper edgeuses. */
	oldeumate->e_p->eu_p = oldeumate;
	eu2->e_p->eu_p = eu2;

	if( share_geom )  {
		/* Make eu1 share same geometry as oldeu */
		nmg_use_edge_g( eu1, oldeu->g.magic_p );
		/* Make eu2 share same geometry as oldeumate */
		nmg_use_edge_g( eu2, oldeumate->g.magic_p );
	} else {
		/* Make eu2 use same geometry as oldeu */
		nmg_use_edge_g( eu2, oldeu->g.magic_p );
		/* Now release geometry from oldeumate;  new edge has no geom */
		BU_LIST_DEQUEUE( &oldeumate->l2 );
		nmg_keg( oldeumate );
		BU_LIST_DEQUEUE( &eu1->l2 );
		nmg_keg( eu1 );
		BU_LIST_INIT( &oldeumate->l2 );
		BU_LIST_INIT( &eu1->l2 );
		oldeumate->l2.magic = NMG_EDGEUSE2_MAGIC;
		eu1->l2.magic = NMG_EDGEUSE2_MAGIC;
	}
	if( oldeu->g.magic_p != oldeu->eumate_p->g.magic_p )  rt_bomb("nmg_eusplit() unshared geom\n");

out:
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_eusplit(v=x%x, eu=x%x, share=%d) new_eu=x%x, mate=x%x\n",
			v, oldeu, share_geom,
			eu1, eu1->eumate_p );
	}
	return(eu1);
}

/*
 *			N M G _ E S P L I T
 *
 *	Split an edge by inserting a vertex into middle of *all* of the
 *	uses of this edge, and combine the new edgeuses together onto the
 *	new edge.
 *	A more powerful version of nmg_eusplit(), which does only one use.
 *
 *	Makes a new edge, and a vertex.  If v is non-null it is taken as a
 *	pointer to an existing vertex to use as the start of the new edge.
 *	If v is null, then a new vertex is created for the begining of the
 *	new edge.
 *
 *	In either case, the new edgeuse will exist as the "next" edgeuse
 *	after the edgeuse passed as a parameter.
 *
 *	Note that eu->e_p changes value upon return, because the old
 *	edge is incrementally replaced by two new ones.
 *
 *	Geometry will be preserved on eu and it's mates (by nmg_eusplit),
 *	if any.  ret_eu and mates will share that geometry if share_geom
 *	is set non-zero, otherwise they will have null geom pointers.
 *
 *  Explicit return -
 *	Pointer to the edgeuse which starts at the newly created
 *	vertex (V), and runs to B.
 *
 *  Implicit returns -
 *	ret_eu->vu_p->v_p gives the new vertex ("v", if non-null), and
 *	ret_eu->e_p is the new edge that runs from V to B.
 *
 *	The new vertex created will also be eu->eumate_p->vu_p->v_p.
 *
 *  Edge on entry -
 *
 *			eu
 *		  .------------->
 *		 /
 *		A =============== B (edge)
 *				 /
 *		  <-------------.
 *		    eu->eumate_p
 *
 *  Edge on return -
 *
 *			eu	  ret_eu
 *		    .------->   .--------->
 *		   /           /
 *	(newedge) A ========= V ~~~~~~~~~~~ B (new edge)
 *			     /             /
 *		    <-------.   <---------.
 *
 *  Note: to replicate the behavior of this routine in BRL-CAD Relase 4.0,
 *  call with share_geom=0.
 */
struct edgeuse *
nmg_esplit(v, eu, share_geom)
struct vertex	*v;		/* New vertex, to go in middle */
struct edgeuse	*eu;
int		share_geom;
{
	struct edge	*e;	/* eu->e_p */
	struct edgeuse	*teuX,	/* radial edgeuse of eu */
			*teuY,	/* new edgeuse (next of teuX) */
			*neu1, *neu2; /* new (split) edgeuses */
	int 		notdone=1;
	struct vertex	*vA, *vB;	/* start and end of eu */

	neu1 = neu2 = (struct edgeuse *)NULL;

	NMG_CK_EDGEUSE(eu);
	e = eu->e_p;
	NMG_CK_EDGE(e);

	NMG_CK_VERTEXUSE(eu->vu_p);
	vA = eu->vu_p->v_p;
	NMG_CK_VERTEX(vA);

	NMG_CK_EDGEUSE(eu->eumate_p);
	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	vB = eu->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX(vB);

	if( v && ( v == vA || v == vB ) )  {
		bu_log("WARNING: nmg_esplit(v=x%x) vertex is already an edge vertex\n", v);
		rt_bomb("nmg_esplit() new vertex is already an edge vertex\n");
	}

	/* one at a time, we peel out & split an edgeuse pair of this edge.
	 * when we split an edge that didn't need to be peeled out, we know
	 * we've split the last edge
	 */
	do {
		/* Peel two temporary edgeuses off the original edge */
		teuX = eu->radial_p;
		/* teuX runs from vA to vB */
		teuY = nmg_eusplit(v, teuX, share_geom);
		/* Now, teuX runs from vA to v, teuY runs from v to vB */
		NMG_CK_EDGEUSE(teuX);
		NMG_CK_EDGEUSE(teuY);
		NMG_TEST_EDGEUSE(teuX);
		NMG_TEST_EDGEUSE(teuY);
		
		if (!v)  {
			/* If "v" parameter was NULL and this is the
			 * first time through, take note of "v" where
			 * "e" was just split at.
			 */
			v = teuY->vu_p->v_p;
			NMG_CK_VERTEX(v);
		}

		if (teuY->e_p == e || teuX->e_p == e) notdone = 0;
		
		/*  Are the two edgeuses going in same or opposite directions?
		 *  Join the newly created temporary edge (teuX, teuY)
		 *  with the new permanant edge (neu1, neu2).
		 *  On first pass, just take note of the new edge & edgeuses.
		 */
		NMG_CK_VERTEX(teuX->vu_p->v_p);
		if (teuX->vu_p->v_p == vA) {
			if (neu1) {
				nmg_je(neu1, teuX);
				nmg_je(neu2, teuY);
			}
			neu1 = teuX->eumate_p;
			neu2 = teuY->eumate_p;
		} else if (teuX->vu_p->v_p == vB) {
			if (neu1) {
				nmg_je(neu2, teuX);
				nmg_je(neu1, teuY);
			}
			neu2 = teuX->eumate_p;
			neu1 = teuY->eumate_p;
		} else {
			bu_log("nmg_esplit(v=x%x, e=x%x)\n", v, e);
			bu_log("nmg_esplit: teuX->vu_p->v_p=x%x, vA=x%x, vB=x%x\n", teuX->vu_p->v_p, vA, vB );
			rt_bomb("nmg_esplit() teuX->vu_p->v_p is neither vA nor vB\n");
		}
	} while (notdone);
	/* Here, "e" pointer is invalid -- it no longer exists */

	/* Find an edgeuse that runs from v to vB */
	if( neu2->vu_p->v_p == v && neu2->eumate_p->vu_p->v_p == vB )  {
		if (rt_g.NMG_debug & DEBUG_BASIC)  {
			bu_log("nmg_esplit(v=x%x, eu=x%x, share=%d) neu2=x%x\n",
				v, eu, share_geom, neu2);
		}
		return neu2;
	}
	else if( neu1->vu_p->v_p == v && neu1->eumate_p->vu_p->v_p == vB )  {
		if (rt_g.NMG_debug & DEBUG_BASIC)  {
			bu_log("nmg_esplit(v=x%x, eu=x%x, share=%d) neu1=x%x\n",
				v, eu, share_geom, neu1);
		}
		return neu1;
	}

	rt_bomb("nmg_esplit() unable to find eu starting at new v\n");
	/* NOTREACHED */
}

/*
 *			N M G _ E B R E A K
 *
 *  Like nmg_esplit(), split an edge into two parts.
 *  Ensure that both sets of edgeuses share the original edgeuse geometry.
 *  If the original edge had no edge geometry, then none is created here.
 *
 *  This is a simple compatability interface to nmg_esplit().
 *  The return is the return of nmg_esplit().
 */
struct edgeuse *
nmg_ebreak(v, eu)
struct vertex	*v;			/* May be NULL */
struct edgeuse	*eu;
{
	struct edgeuse	*new_eu;

	NMG_CK_EDGEUSE(eu);
	if( eu->g.magic_p )  {
		NMG_CK_EDGE_G_LSEG(eu->g.lseg_p);
	}

	new_eu = nmg_esplit(v, eu, 1);	/* Do the hard work */
	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGEUSE(new_eu);

	if( eu->e_p == new_eu->e_p )  rt_bomb("nmb_ebreak() same edges?\n");

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_ebreak( v=x%x, eu=x%x ) new_eu=x%x\n",
			v, eu, new_eu);
	}
	return new_eu;
}

/*
 *			N M G _ E B R E A K E R
 *
 *  Like nmg_ebreak(), but with edge radial sorting when sharing occurs.
 *
 *  Use nmg_ebreak() to break an existing edge on a vertex, preserving edge
 *  geometry on both new edges.
 *  If the edge was broken on an existing vertex,
 *  search both old and new edgeuses to see if they need to be joined
 *  with an existing edgeuse that shared the same vertices.
 */
struct edgeuse *
nmg_ebreaker(v, eu, tol)
struct vertex		*v;			/* May be NULL */
struct edgeuse		*eu;
CONST struct bn_tol	*tol;
{
	struct edgeuse	*new_eu;
	struct edgeuse	*oeu;

	NMG_CK_EDGEUSE(eu);
	BN_CK_TOL(tol);

nmg_eu_radial_check( eu, nmg_find_s_of_eu(eu), tol );
	new_eu = nmg_ebreak(v, eu);
	if( v )  {
		/*
		 *  This edge was broken on an existing vertex.
		 *  Search the whole model for other existing edges
		 *  that match the newly created edge fragments.
		 */
		for(;;)  {
			oeu = nmg_find_e( eu->vu_p->v_p,
				eu->eumate_p->vu_p->v_p,
				(struct shell *)NULL, eu->e_p );
			if( !oeu ) break;
			if (rt_g.NMG_debug & DEBUG_BASIC)  {
				bu_log("nmg_ebreaker() joining eu=x%x to oeu=x%x\n",
					eu, oeu );
			}
			nmg_radial_join_eu( eu, oeu, tol );
		}

		for(;;)  {
			oeu = nmg_find_e( new_eu->vu_p->v_p,
				new_eu->eumate_p->vu_p->v_p,
				(struct shell *)NULL, new_eu->e_p );
			if( !oeu )  break;
			if (rt_g.NMG_debug & DEBUG_BASIC)  {
				bu_log("nmg_ebreaker() joining new_eu=x%x to oeu=x%x\n",
					new_eu, oeu );
			}
			nmg_radial_join_eu( new_eu, oeu, tol );
		}
/* XXX Will this catch it? */
nmg_eu_radial_check( eu, nmg_find_s_of_eu(eu), tol );
nmg_eu_radial_check( new_eu, nmg_find_s_of_eu(new_eu), tol );
if( nmg_check_radial( eu, tol ) ) bu_log("ERROR ebreaker eu=x%x bad\n", eu);
if( nmg_check_radial( new_eu, tol ) ) bu_log("ERROR ebreaker new_eu=x%x bad\n", new_eu);
	}
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_ebreaker( v=x%x, eu=x%x ) new_eu=x%x\n", v, eu, new_eu);
	}
	return new_eu;
}

/*
 *			N M G _ E 2 B R E A K
 *
 *  Given two edges that are known to intersect someplace other than
 *  at any of their endpoints, break both of them and
 *  insert a shared vertex.
 *  Return a pointer to the new vertex.
 */
struct vertex *
nmg_e2break( eu1, eu2 )
struct edgeuse	*eu1;
struct edgeuse	*eu2;
{
	struct vertex		*v;
	struct edgeuse		*new_eu;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);

	new_eu = nmg_ebreak( NULL, eu1 );
	v = new_eu->vu_p->v_p;
	NMG_CK_VERTEX(v);
	(void)nmg_ebreak( v, eu2 );

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_e2break( eu1=x%x, eu2=x%x ) v=x%x\n", eu1, eu2, v);
	}
	return v;
}
/*
 *			N M G _ U N B R E A K _ E D G E
 *
 *  Undoes the effect of an unwanted nmg_ebreak().
 *
 *  Eliminate the vertex between this edgeuse and the next edge,
 *  on all edgeuses radial to this edgeuse's edge.
 *  The edge geometry must be shared, and all uses of the vertex
 *  to be disposed of must originate from this edge pair.
 *  Also, the "non-B" ends of all edgeuses around e1 and e2 must
 *  terminate at either A or B.
 *
 *  XXX for t-NURBS, this should probably be re-stated as
 *  saying that all the edgeuses must share the same 2 edges, and that
 *  every eu1 needs to share geom with it's corresponding eu2,
 *  and similarly for the two mates.
 *
 *		     eu1          eu2
 *		*----------->*----------->*
 *		A.....e1.....B.....e2.....C
 *		*<-----------*<-----------*
 *		    eu1mate      eu2mate
 *
 *  If successful, the vertex B, the edge e2, and all the edgeuses
 *  radial to eu2 (including eu2) will have all been killed.
 *  The radial ordering around e1 will not change.
 *
 *		     eu1
 *		*------------------------>*
 *		A.....e1..................C
 *		*<------------------------*
 *		    eu1mate
 *
 *
 *  No new topology structures are created by this operation.
 *
 *  Returns -
 *	0	OK, edge unbroken
 *	<0	failure, nothing changed
 */
int
nmg_unbreak_edge( eu1_first )
struct edgeuse	*eu1_first;
{
	struct edgeuse	*eu1;
	struct edgeuse	*eu2;
	struct edgeuse	*teu;
	struct edge	*e1;
	struct edge_g_lseg	*eg;
	struct vertexuse *vu;
	struct vertex	*vb = 0;
	struct vertex	*vc;
	struct vertex	*va;
	struct shell	*s1;
	int		ret = 0;

	NMG_CK_EDGEUSE( eu1_first );
	e1 = eu1_first->e_p;
	NMG_CK_EDGE( e1 );

	if( eu1_first->g.magic_p != eu1_first->eumate_p->g.magic_p )
		rt_bomb("nmg_unbreak_edge() eu and mate don't share geometry\n");

	eg = eu1_first->g.lseg_p;
	if( !eg )  {
		bu_log( "nmg_unbreak_edge: no geometry for edge1 x%x\n" , e1 );
		ret = -1;
		goto out;
	}
	NMG_CK_EDGE_G_LSEG(eg);

	/* If the edge geometry doesn't have at least four edgeuses, this
	 * is not a candidate for unbreaking */		
	if( bu_list_len( &eg->eu_hd2 ) < 2*2 )  {
		ret = -2;
		goto out;
	}

	s1 = nmg_find_s_of_eu(eu1_first);
	NMG_CK_SHELL(s1);

	eu1 = eu1_first;
	eu2 = BU_LIST_PNEXT_CIRC( edgeuse , eu1 );
	if( eu2->g.lseg_p != eg )  {
		bu_log( "nmg_unbreak_edge: second eu geometry x%x does not match geometry x%x of edge1 x%x\n" ,
			eu2->g.magic_p, eg, e1 );
		ret = -3;
		goto out;
	}

	va = eu1->vu_p->v_p;		/* start vertex (A) */
	vb = eu2->vu_p->v_p;		/* middle vertex (B) */
	vc = eu2->eumate_p->vu_p->v_p;	/* end vertex (C) */

	/* all uses of this vertex must be for this edge geometry, otherwise
	 * it is not a candidate for deletion */
	for( BU_LIST_FOR( vu , vertexuse , &vb->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if( *(vu->up.magic_p) != NMG_EDGEUSE_MAGIC )  {
			/* vertex is referred to by a self-loop */
			if( vu->up.lu_p->orientation == OT_BOOLPLACE )  {
				/* This kind is transient, and safe to ignore */
				continue;
			}
			ret = -4;
			goto out;
		}
		NMG_CK_EDGEUSE(vu->up.eu_p);
		if( vu->up.eu_p->g.lseg_p != eg )  {
			ret = -5;
			goto out;
		}
	}

	/* Visit all edgeuse pairs radial to eu1 (A--B) */
	teu = eu1;
	for(;;)  {
		register struct edgeuse	*teu2;
		NMG_CK_EDGEUSE(teu);
		if( teu->vu_p->v_p != va || teu->eumate_p->vu_p->v_p != vb )  {
			ret = -6;
			goto out;
		}
		/* We *may* encounter a teu2 not around eu2.  Seen in BigWedge */
		teu2 = BU_LIST_PNEXT_CIRC( edgeuse, teu );
		NMG_CK_EDGEUSE(teu2);
		if( teu2->vu_p->v_p != vb || teu2->eumate_p->vu_p->v_p != vc )  {
			ret = -7;
			goto out;
		}
		teu = teu->eumate_p->radial_p;
		if( teu == eu1 )  break;
	}

	/* Visit all edgeuse pairs radial to eu2 (B--C) */
	teu = eu2;
	for(;;)  {
		NMG_CK_EDGEUSE(teu);
		if( teu->vu_p->v_p != vb || teu->eumate_p->vu_p->v_p != vc )  {
			ret = -8;
			goto out;
		}
		teu = teu->eumate_p->radial_p;
		if( teu == eu2 )  break;
	}

	/* All preconditions are met, begin the unbreak operation */
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_unbreak_edge va=x%x, vb=x%x, vc=x%x\n",
			va, vb, vc );
		bu_log("nmg_unbreak_edge A:(%g, %g, %g), B:(%g, %g, %g), C:(%g, %g, %g)\n",
			V3ARGS( va->vg_p->coord ),
			V3ARGS( vb->vg_p->coord ),
			V3ARGS( vc->vg_p->coord ) );
	}

	if( va == vc )
	{
		/* bu_log( "nmg_unbreak_edge( eu=%x ): Trying to break a jaunt, va==vc (%x)\n", eu1_first, va ); */
		ret = -9;
		goto out;
	}
		

	/* visit all the edgeuse pairs radial to eu1 */
	for(;;)  {
		/* Recheck initial conditions */
		if( eu1->vu_p->v_p != va || eu1->eumate_p->vu_p->v_p != vb )  {
			bu_log( "nmg_unbreak_edge: eu1 does not got to/from correct vertices, x%x, %x\n", 
				eu1->vu_p->v_p, eu1->eumate_p->vu_p->v_p );
			nmg_pr_eu_briefly( eu1, " " );
			rt_bomb( "nmg_unbreak_edge 1\n" );
		}
		eu2 = BU_LIST_PNEXT_CIRC( edgeuse, eu1 );
		NMG_CK_EDGEUSE(eu2);
		if( eu2->g.lseg_p != eg )  {
			rt_bomb("nmg_unbreak_edge:  eu2 geometry is wrong\n");
		}
		if( eu2->vu_p->v_p != vb || eu2->eumate_p->vu_p->v_p != vc )  {
			bu_log( "nmg_unbreak_edge: about to kill eu2, but does not got to/from correct vertices, x%x, x%x\n",
				eu2->vu_p->v_p, eu2->eumate_p->vu_p->v_p );
			nmg_pr_eu_briefly( eu2, " " );
			rt_bomb( "nmg_unbreak_edge 3\n" );
		}

		/* revector eu1mate's start vertex from B to C */
		nmg_movevu( eu1->eumate_p->vu_p , vc );

		if( eu1->vu_p->v_p != va || eu1->eumate_p->vu_p->v_p != vc )  {
			bu_log( "nmg_unbreak_edge: extended eu1 does not got to/from correct vertices, x%x, x%x\n",
				eu1->vu_p->v_p, eu1->eumate_p->vu_p->v_p );
			nmg_pr_eu_briefly( eu1, " " );
			rt_bomb( "nmg_unbreak_edge 2\n" );
		}

		if( eu2 != BU_LIST_PNEXT_CIRC( edgeuse, eu1 ) )
			rt_bomb("nmg_unbreak_edge eu2 unexpected altered\n");

		/* Now kill off the unnecessary eu2 associated w/ cur eu1 */
		if( nmg_keu( eu2 ) )
			rt_bomb( "nmg_unbreak_edge: edgeuse parent is now empty!!\n" );

		if( eu1->vu_p->v_p != va || eu1->eumate_p->vu_p->v_p != vc )  {
			bu_log( "nmg_unbreak_edge: unbroken eu1 (after eu2 killed) does not got to/from correct vertices, x%x, x%x\n",
				eu1->vu_p->v_p, eu1->eumate_p->vu_p->v_p );
			nmg_pr_eu_briefly( eu1, " " );
			rt_bomb( "nmg_unbreak_edge 4\n" );
		}
		eu1 = eu1->eumate_p->radial_p;
		if( eu1 == eu1_first )  break;
	}
out:
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_unbreak_edge(eu=x%x, vb=x%x) ret = %d\n",
			eu1_first, vb, ret);
	}
	return ret;
}

/*
 *			N M G _ U N B R E A K _ S H E L L _ E D G E _ U N S A F E
 *
 *  Undoes the effect of an unwanted nmg_ebreak().
 *
 *	NOTE: THIS IS LIKELY TO PRODUCE AN ILLEGAL NMG STRUCTURE!!!!
 *	This routine is intended for use only when simplifying an NMG
 *	prior to output in another format (such as polygons). It will
 *	unbreak edges where nmg_unbreak_edge() will not!!!!!
 *
 *  Eliminate the vertex between this edgeuse and the next edge,
 *  on all edgeuses radial to this edgeuse's edge.
 *  The edge geometry must be shared, and all uses of the vertex, in the same shell,
 *  to be disposed of must originate from this edge pair.
 *  Also, the "non-B" ends of all edgeuses around e1 and e2 (in this shell) must
 *  terminate at either A or B.
 *
 *
 *		     eu1          eu2
 *		*----------->*----------->*
 *		A.....e1.....B.....e2.....C
 *		*<-----------*<-----------*
 *		    eu1mate      eu2mate
 *
 *  If successful, the vertex B, the edge e2, and all the edgeuses in the same shell
 *  radial to eu2 (including eu2) will have all been killed.
 *  The radial ordering around e1 will not change.
 *
 *		     eu1
 *		*------------------------>*
 *		A.....e1..................C
 *		*<------------------------*
 *		    eu1mate
 *
 *
 *  No new topology structures are created by this operation.
 *
 *  Returns -
 *	0	OK, edge unbroken
 *	<0	failure, nothing changed
 */
int
nmg_unbreak_shell_edge_unsafe( eu1_first )
struct edgeuse	*eu1_first;
{
	struct edgeuse	*eu1;
	struct edgeuse	*eu2;
	struct edgeuse	*teu;
	struct edge	*e1;
	struct edge_g_lseg	*eg;
	struct vertexuse *vu;
	struct vertex	*vb = 0;
	struct vertex	*vc;
	struct vertex	*va;
	struct shell	*s1;
	int		ret = 0;

	NMG_CK_EDGEUSE( eu1_first );
	e1 = eu1_first->e_p;
	NMG_CK_EDGE( e1 );

	if( eu1_first->g.magic_p != eu1_first->eumate_p->g.magic_p )
	{
		ret = -10;
		goto out;
	}

	eg = eu1_first->g.lseg_p;
	if( !eg )  {
		ret = -1;
		goto out;
	}
	NMG_CK_EDGE_G_LSEG(eg);

	s1 = nmg_find_s_of_eu(eu1_first);
	NMG_CK_SHELL(s1);

	eu1 = eu1_first;
	eu2 = BU_LIST_PNEXT_CIRC( edgeuse , eu1 );
	if( eu2->g.lseg_p != eg )  {
		bu_log( "nmg_unbreak_edge: second eu geometry x%x does not match geometry x%x of edge1 x%x\n" ,
			eu2->g.magic_p, eg, e1 );
		ret = -3;
		goto out;
	}

	va = eu1->vu_p->v_p;		/* start vertex (A) */
	vb = eu2->vu_p->v_p;		/* middle vertex (B) */
	vc = eu2->eumate_p->vu_p->v_p;	/* end vertex (C) */

	/* all uses of this vertex (in shell s1) must be for this edge geometry, otherwise
	 * it is not a candidate for deletion */
	for( BU_LIST_FOR( vu , vertexuse , &vb->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if( *(vu->up.magic_p) != NMG_EDGEUSE_MAGIC )  {
			/* vertex is referred to by a self-loop */
			if( vu->up.lu_p->orientation == OT_BOOLPLACE )  {
				/* This kind is transient, and safe to ignore */
				continue;
			}
			ret = -4;
			goto out;
		}
		if( nmg_find_s_of_vu( vu ) != s1 )
			continue;
		NMG_CK_EDGEUSE(vu->up.eu_p);
		if( vu->up.eu_p->g.lseg_p != eg )  {
			ret = -5;
			goto out;
		}
	}

	/* Visit all edgeuse pairs radial to eu1 (A--B) */
	teu = eu1;
	for(;;)  {
		register struct edgeuse	*teu2;
		NMG_CK_EDGEUSE(teu);
		if( teu->vu_p->v_p != va || teu->eumate_p->vu_p->v_p != vb )  {
			ret = -6;
			goto out;
		}
		/* We *may* encounter a teu2 not around eu2.  Seen in BigWedge */
		teu2 = BU_LIST_PNEXT_CIRC( edgeuse, teu );
		NMG_CK_EDGEUSE(teu2);
		if( teu2->vu_p->v_p != vb || teu2->eumate_p->vu_p->v_p != vc )  {
			ret = -7;
			goto out;
		}
		teu = teu->eumate_p->radial_p;
		if( teu == eu1 )  break;
	}

	/* Visit all edgeuse pairs radial to eu2 (B--C) */
	teu = eu2;
	for(;;)  {
		NMG_CK_EDGEUSE(teu);
		if( teu->vu_p->v_p != vb || teu->eumate_p->vu_p->v_p != vc )  {
			ret = -8;
			goto out;
		}
		teu = teu->eumate_p->radial_p;
		if( teu == eu2 )  break;
	}

	/* All preconditions are met, begin the unbreak operation */
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_unbreak_edge va=x%x, vb=x%x, vc=x%x\n",
			va, vb, vc );
		bu_log("nmg_unbreak_edge A:(%g, %g, %g), B:(%g, %g, %g), C:(%g, %g, %g)\n",
			V3ARGS( va->vg_p->coord ),
			V3ARGS( vb->vg_p->coord ),
			V3ARGS( vc->vg_p->coord ) );
	}

	if( va == vc )
	{
		/* bu_log( "nmg_unbreak_edge( eu=%x ): Trying to break a jaunt, va==vc (%x)\n", eu1_first, va ); */
		ret = -9;
		goto out;
	}
		

	/* visit all the edgeuse pairs radial to eu1 */
	for(;;)  {
		/* Recheck initial conditions */
		if( eu1->vu_p->v_p != va || eu1->eumate_p->vu_p->v_p != vb )  {
			bu_log( "nmg_unbreak_edge: eu1 does not got to/from correct vertices, x%x, %x\n", 
				eu1->vu_p->v_p, eu1->eumate_p->vu_p->v_p );
			nmg_pr_eu_briefly( eu1, " " );
			rt_bomb( "nmg_unbreak_edge 1\n" );
		}
		eu2 = BU_LIST_PNEXT_CIRC( edgeuse, eu1 );
		NMG_CK_EDGEUSE(eu2);
		if( eu2->g.lseg_p != eg )  {
			rt_bomb("nmg_unbreak_edge:  eu2 geometry is wrong\n");
		}
		if( eu2->vu_p->v_p != vb || eu2->eumate_p->vu_p->v_p != vc )  {
			bu_log( "nmg_unbreak_edge: about to kill eu2, but does not got to/from correct vertices, x%x, x%x\n",
				eu2->vu_p->v_p, eu2->eumate_p->vu_p->v_p );
			nmg_pr_eu_briefly( eu2, " " );
			rt_bomb( "nmg_unbreak_edge 3\n" );
		}

		/* revector eu1mate's start vertex from B to C */
		nmg_movevu( eu1->eumate_p->vu_p , vc );

		if( eu1->vu_p->v_p != va || eu1->eumate_p->vu_p->v_p != vc )  {
			bu_log( "nmg_unbreak_edge: extended eu1 does not got to/from correct vertices, x%x, x%x\n",
				eu1->vu_p->v_p, eu1->eumate_p->vu_p->v_p );
			nmg_pr_eu_briefly( eu1, " " );
			rt_bomb( "nmg_unbreak_edge 2\n" );
		}

		if( eu2 != BU_LIST_PNEXT_CIRC( edgeuse, eu1 ) )
			rt_bomb("nmg_unbreak_edge eu2 unexpected altered\n");

		/* Now kill off the unnecessary eu2 associated w/ cur eu1 */
		if( nmg_keu( eu2 ) )
			rt_bomb( "nmg_unbreak_edge: edgeuse parent is now empty!!\n" );

		if( eu1->vu_p->v_p != va || eu1->eumate_p->vu_p->v_p != vc )  {
			bu_log( "nmg_unbreak_edge: unbroken eu1 (after eu2 killed) does not got to/from correct vertices, x%x, x%x\n",
				eu1->vu_p->v_p, eu1->eumate_p->vu_p->v_p );
			nmg_pr_eu_briefly( eu1, " " );
			rt_bomb( "nmg_unbreak_edge 4\n" );
		}
		eu1 = eu1->eumate_p->radial_p;
		if( eu1 == eu1_first )  break;
	}
out:
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_unbreak_edge(eu=x%x, vb=x%x) ret = %d\n",
			eu1_first, vb, ret);
	}
	return ret;
}

/*
 *			N M G _ E I N S
 *
 *	Insert a new (zero length) edge at the begining of (ie, before)
 *	an existing edgeuse
 *	Perhaps this is what nmg_esplit and nmg_eusplit should have been like?
 *
 *	Before:
 *	.--A--> .--eu-->
 *		 \
 *		  >.
 *		 /
 *	  <-A'--. <-eu'-.
 *
 *
 *	After:
 *
 *               eu1     eu
 *	.--A--> .---> .--eu-->
 *		 \   /
 *		  >.<
 *		 /   \
 *	  <-A'--. <---. <-eu'--.
 *	          eu2     eumate
 */
struct edgeuse *
nmg_eins(eu)
struct edgeuse *eu;
{
	struct edgeuse	*eumate;
	struct edgeuse	*eu1, *eu2;
	struct shell	*s;

	NMG_CK_EDGEUSE(eu);
	eumate = eu->eumate_p;
	NMG_CK_EDGEUSE(eumate);

	if (*eu->up.magic_p == NMG_SHELL_MAGIC) {
		s = eu->up.s_p;
		NMG_CK_SHELL(s);
	}
	else {
		struct loopuse *lu;
		
		lu = eu->up.lu_p;
		NMG_CK_LOOPUSE(lu);
		if (*lu->up.magic_p == NMG_SHELL_MAGIC) {
			s = lu->up.s_p;
			NMG_CK_SHELL(s);
		} else {
			struct faceuse *fu;
			fu = lu->up.fu_p;
			NMG_CK_FACEUSE(fu);
			s = fu->s_p;
			NMG_CK_SHELL(s);
		}
	}

	eu1 = nmg_me(eu->vu_p->v_p, eu->vu_p->v_p, s);
	eu2 = eu1->eumate_p;

	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		BU_LIST_DEQUEUE( &eu1->l );
		BU_LIST_DEQUEUE( &eu2->l );

		BU_LIST_INSERT( &eu->l, &eu1->l );
		BU_LIST_APPEND( &eumate->l, &eu2->l );

		eu1->up.lu_p = eu->up.lu_p;
		eu2->up.lu_p = eumate->up.lu_p;
	}
	else {
		rt_bomb("nmg_eins() Cannot yet insert null edge in shell\n");
	}
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_eins(eu=x%x) eu1=x%x\n", eu, eu1);
	}
	return(eu1);
}

/*
 *			N M G _ M V _ E U _ B E T W E E N _ S H E L L S
 *
 *  Move a wire edgeuse and it's mate from one shell to another.
 */
void
nmg_mv_eu_between_shells( dest, src, eu )
struct shell		*dest;
register struct shell	*src;
register struct edgeuse	*eu;
{
	register struct edgeuse	*eumate;

	NMG_CK_EDGEUSE(eu);
	eumate = eu->eumate_p;
	NMG_CK_EDGEUSE(eumate);

	if (eu->up.s_p != src) {
		bu_log("nmg_mv_eu_between_shells(dest=x%x, src=x%x, eu=x%x), eu->up.s_p=x%x isnt src shell\n",
			dest, src, eu, eu->up.s_p );
		rt_bomb("eu->up.s_p isnt source shell\n");
	}
	if (eumate->up.s_p != src) {
		bu_log("nmg_mv_eu_between_shells(dest=x%x, src=x%x, eu=x%x), eumate->up.s_p=x%x isn't src shell\n",
			dest, src, eu, eumate->up.s_p );
		rt_bomb("eumate->up.s_p isnt source shell\n");
	}

	/* Remove eu from src shell */
	BU_LIST_DEQUEUE( &eu->l );
	if( BU_LIST_IS_EMPTY( &src->eu_hd ) )  {
		/* This was the last eu in the list, bad news */
		bu_log("nmg_mv_eu_between_shells(dest=x%x, src=x%x, eu=x%x), eumate=x%x not in src shell\n",
			dest, src, eu, eumate );
		rt_bomb("src shell emptied before finding eumate\n");
	}

	/* Remove eumate from src shell */
	BU_LIST_DEQUEUE( &eumate->l );

	/* Add eu and eumate to dest shell */
	BU_LIST_APPEND( &dest->eu_hd, &eu->l );
	BU_LIST_APPEND( &eu->l, &eumate->l );

	eu->up.s_p = dest;
	eumate->up.s_p = dest;

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_mv_eu_between_shells( dest=x%x, src=x%x, eu=x%x ) new_eu=x%x\n",
			dest, src, eu);
	}
}

/************************************************************************
 *									*
 *				VERTEX Routines				*
 *									*
 ************************************************************************/

/*
 *			N M G _ M V _ V U _ B E T W E E N _ S H E L L S
 *
 *  If this shell had a single vertexuse in it, move it to the other
 *  shell, but "promote" it to a "loop of a single vertex" along the way.
 */
void
nmg_mv_vu_between_shells( dest, src, vu )
struct shell		*dest;
register struct shell	*src;
register struct vertexuse	*vu;
{
	NMG_CK_VERTEXUSE( vu );
	NMG_CK_VERTEX( vu->v_p );

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_mv_vu_between_shells( dest_s=x%x, src_s=x%x, vu=x%x )\n",
			dest, src, vu);
	}
	(void) nmg_mlv( &(dest->l.magic), vu->v_p, OT_SAME );
	if (vu->v_p->vg_p) {
		NMG_CK_VERTEX_G(vu->v_p->vg_p);
	}
	nmg_kvu( vu );
}
