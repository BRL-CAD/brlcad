/*
 *			N M G _ M A N I F . C
 *
 *  Routines for assessing the manifold dimension of an object.
 *
 *  Author -
 *	Lee A. Butler
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
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "nmg.h"
#include "raytrace.h"

#define PAINT_INTERIOR 1
#define PAINT_EXTERIOR 0

/*	N M G _ D A N G L I N G _ F A C E
 *
 *	Determine if a face has any "dangling" edges.
 *
 *	Return
 *	1	face has dangling edge
 *	0	face does not have a dangling edge
 */
int 
nmg_dangling_face(fu, manifolds)
CONST struct faceuse	*fu;
register CONST char	*manifolds;
{
	struct loopuse *lu;
	struct edgeuse *eu;
	CONST struct edgeuse *eur;
	struct faceuse *newfu;

	NMG_CK_FACEUSE(fu);

	for(RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	    NMG_CK_LOOPUSE(lu);

	    if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
	        /* go looking around each edge for a face of the same
	         * shell which isn't us and isn't our mate.  If we
	         * find us or our mate before another face of this
	         * shell, we are non-3-manifold.
	         *
	         */

	    	for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {

		    eur = nmg_radial_face_edge_in_shell(eu);
		    newfu = eur->up.lu_p->up.fu_p;

	    	    if (manifolds) {

		    	    /* skip any known dangling-edge faces or 
		    	     * faces known to be 2manifolds.
			     */
			    while (NMG_MANIFOLDS(manifolds,newfu) & NMG_2MANIFOLD &&
				eur != eu->eumate_p) {
					eur = nmg_radial_face_edge_in_shell(
						eur->eumate_p);
					newfu = eur->up.lu_p->up.fu_p;
			    }
	    	    }

		    if (eur == eu->eumate_p) {
			return(1);
		    }
		}
	    }
	}

	return(0);
}

/*
 *	"Paint" the elements of a face with a meaning.  For example
 *	mark everything in a face as being part of a 2-manifold
 */
static void paint_face(fu, paint_table, paint_color, paint_meaning, tbl)
struct faceuse *fu;
char *paint_table, *paint_meaning, *tbl;
int paint_color;
{
	struct faceuse *newfu;
	struct loopuse *lu;
	struct edgeuse *eu;
	CONST struct edgeuse *eur;

	if (NMG_INDEX_VALUE(paint_table, fu->index) != 0)
		return;

	NMG_INDEX_ASSIGN(paint_table, fu, paint_color);

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
			continue;

		for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);
			NMG_CK_EDGEUSE(eu->eumate_p);
			eur = nmg_radial_face_edge_in_shell(eu);
			NMG_CK_EDGEUSE(eur);
			NMG_CK_FACEUSE(eur->up.lu_p->up.fu_p);
			newfu = eur->up.lu_p->up.fu_p;


			while (NMG_MANIFOLDS(tbl, newfu) & NMG_2MANIFOLD &&
			    eur != eu->eumate_p) {
				eur = nmg_radial_face_edge_in_shell(
							eur->eumate_p);
				newfu = eur->up.lu_p->up.fu_p;
			}

			if (newfu->orientation == OT_SAME)
				paint_face(newfu,paint_table,paint_color,
					paint_meaning,tbl);
			else {
				/* mark this group as being interior */
				paint_meaning[paint_color] = PAINT_INTERIOR;
			}
		}
	}
}

static void set_edge_sub_manifold(tbl, eu_p, manifold)
char *tbl;
struct edgeuse *eu_p;
char manifold;
{
	
	NMG_CK_EDGEUSE(eu_p);
	NMG_CK_EDGE(eu_p->e_p);
	NMG_CK_VERTEXUSE(eu_p->vu_p);
	NMG_CK_VERTEX(eu_p->vu_p->v_p);

	NMG_SET_MANIFOLD(tbl, eu_p, manifold);
	NMG_SET_MANIFOLD(tbl, eu_p->e_p, manifold);

	NMG_SET_MANIFOLD(tbl, eu_p->vu_p, manifold);
	NMG_SET_MANIFOLD(tbl, eu_p->vu_p->v_p, manifold);
}


static void set_loop_sub_manifold(tbl, lu_p, manifold)
char *tbl;
struct loopuse *lu_p;
char manifold;
{
	struct edgeuse *eu_p;
	struct vertexuse *vu_p;

	NMG_CK_LOOPUSE(lu_p);

	NMG_SET_MANIFOLD(tbl, lu_p, manifold);
	NMG_SET_MANIFOLD(tbl, lu_p->l_p, manifold);
	if (RT_LIST_FIRST_MAGIC(&lu_p->down_hd) == NMG_VERTEXUSE_MAGIC) {
		vu_p = RT_LIST_FIRST(vertexuse, &lu_p->down_hd);
		NMG_SET_MANIFOLD(tbl, vu_p, manifold);
		NMG_SET_MANIFOLD(tbl, vu_p->v_p, manifold);
	} else if (RT_LIST_FIRST_MAGIC(&lu_p->down_hd) == NMG_EDGEUSE_MAGIC) {
		for (RT_LIST_FOR(eu_p, edgeuse, &lu_p->down_hd)) {
			set_edge_sub_manifold(tbl, eu_p, manifold);
		}
	} else
		rt_bomb("bad child of loopuse\n");
}
static void set_face_sub_manifold(tbl, fu_p, manifold)
char *tbl;
struct faceuse *fu_p;
char manifold;
{
	struct loopuse *lu_p;
	
	NMG_CK_FACEUSE(fu_p);
	NMG_CK_FACE(fu_p->f_p);

	NMG_SET_MANIFOLD(tbl, fu_p, manifold);
	NMG_SET_MANIFOLD(tbl, fu_p->f_p, manifold);
	for (RT_LIST_FOR(lu_p, loopuse, &fu_p->lu_hd)) {
		set_loop_sub_manifold(tbl, lu_p, manifold);
	}
}



char *
nmg_shell_manifolds(sp, tbl)
struct shell *sp;
char *tbl;
{
	struct edgeuse *eu_p;
	struct loopuse *lu_p;
	struct faceuse *fu_p;
	char *paint_meaning, *paint_table, paint_color;


	NMG_CK_SHELL(sp);

	if (tbl == (char *)NULL)
		tbl = rt_calloc(sp->r_p->m_p->maxindex, 1, "manifold table");

	/*
	 * points in shells form 0-manifold objects.
	 */
	if (sp->vu_p) {
		NMG_CK_VERTEXUSE(sp->vu_p);
		NMG_SET_MANIFOLD(tbl, sp, NMG_0MANIFOLD);
		NMG_SET_MANIFOLD(tbl, sp->vu_p, NMG_0MANIFOLD);
		NMG_SET_MANIFOLD(tbl, sp->vu_p->v_p, NMG_0MANIFOLD);
	}

	/* edges in shells are (components of)
	 * 1-manifold objects.
	 */
	if (RT_LIST_NON_EMPTY(&sp->eu_hd)) {
		for (RT_LIST_FOR(eu_p, edgeuse, &sp->eu_hd)) {
			set_edge_sub_manifold(tbl, eu_p, NMG_1MANIFOLD);
		}
		NMG_SET_MANIFOLD(tbl, sp, NMG_1MANIFOLD);
	}

	/* loops in shells are (components of)
	 * 1-manifold objects.
	 */
	if (RT_LIST_NON_EMPTY(&sp->lu_hd)) {
		for (RT_LIST_FOR(lu_p, loopuse, &sp->lu_hd)) {
			set_loop_sub_manifold(tbl, lu_p, NMG_1MANIFOLD);
		}
		NMG_SET_MANIFOLD(tbl, sp, NMG_1MANIFOLD);
	}

	if (rt_g.NMG_debug & DEBUG_NMGRT)
		rt_log("starting manifold classification on shell faces\n");

	/*
	 * faces may be either 2 or 3 manifold components.
	 */
	if (RT_LIST_NON_EMPTY(&sp->fu_hd)) {
		register int found;

		/* mark all externally dangling faces and faces
		 * with or adjacent to dangling edges.
		 */

		do {
			found = 0;
			for (RT_LIST_FOR(fu_p, faceuse, &sp->fu_hd)) {
				if (nmg_dangling_face(fu_p, tbl)) {
					found = 1;

					NMG_SET_MANIFOLD(tbl, fu_p, 
							NMG_2MANIFOLD);

					if (rt_g.NMG_debug & DEBUG_NMGRT)
						rt_log("found dangling faces\n");
				}
			}
		} while (found);

		/* paint the exterior faces to discover what the
		 * actual enclosing shell is
		 */

		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("starting to paint non-dangling faces\n");

		paint_meaning = rt_calloc(256, 1, "paint meaning table");
		paint_table = rt_calloc(sp->r_p->m_p->maxindex, 1, "paint table");
		paint_color = 1;

		for (RT_LIST_FOR(fu_p, faceuse, &sp->fu_hd)) {
			if (fu_p->orientation != OT_SAME ||
			    NMG_INDEX_VALUE(paint_table, fu_p->index) != 0)
				continue;

			paint_face(fu_p, paint_table, paint_color, paint_meaning, tbl);
			paint_color++;
		}

		
		if (rt_g.NMG_debug & DEBUG_NMGRT)
			rt_log("painting done, looking at colors\n");

		
		/* all the faces painted with "interior" paint are 2manifolds
		 * those faces still painted with "exterior" paint are
		 * 3manifolds, ie. part of the enclosing surface
		 */
		for (RT_LIST_FOR(fu_p, faceuse, &sp->fu_hd)) {
			paint_color = NMG_INDEX_VALUE(paint_table,
						fu_p->index);

			if (NMG_INDEX_VALUE(paint_meaning, paint_color) ==
			    PAINT_INTERIOR) {
			    	set_face_sub_manifold(tbl, fu_p, NMG_2MANIFOLD);
			} else if (NMG_INDEX_VALUE(paint_meaning, paint_color)
			    == PAINT_EXTERIOR) {
			    	set_face_sub_manifold(tbl, fu_p, NMG_3MANIFOLD);
			}

		}

		rt_free(paint_meaning, "paint meaning table");
		rt_free(paint_table, "paint table");

		for (RT_LIST_FOR(fu_p, faceuse, &sp->fu_hd)) {
			if (fu_p->orientation != OT_SAME)
				NMG_CP_MANIFOLD(tbl, fu_p, fu_p->fumate_p);
		}
	}

	return(tbl);
}


char *
nmg_manifolds(m)
struct model *m;
{
	struct nmgregion *rp;
	struct shell *sp;
	char *tbl;

	NMG_CK_MODEL(m);

	tbl = rt_calloc(m->maxindex, 1, "manifold table");

	for (RT_LIST_FOR(rp, nmgregion, &m->r_hd)) {
		NMG_CK_REGION(rp);

		for (RT_LIST_FOR(sp, shell, &rp->s_hd)) {
			nmg_shell_manifolds(sp, tbl);

			/* make sure the region manifold bits are a superset
			 * of the shell manifold bits
			 */
			NMG_CP_MANIFOLD(tbl, rp, sp);
		}
	}

	return(tbl);
}

