/*                     N M G _ M A N I F . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @addtogroup nmg */
/** @{ */
/** @file primitives/nmg/nmg_manif.c
 *
 * Routines for assessing the manifold dimension of an object.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

#define PAINT_INTERIOR 1
#define PAINT_EXTERIOR 0

#define BU_LIST_LINK_CHECK(p) \
	if (BU_LIST_PNEXT_PLAST(bu_list, p) != p || \
	    BU_LIST_PLAST_PNEXT(bu_list, p) != p) { \
		bu_log("%s[%d]: linked list integrity check failed\n", \
				__FILE__, __LINE__); \
		bu_log("%p->forw(%p)->back = %p\n", \
		       (void *)(p), (void *)(p)->forw, (void *)(p)->forw->back); \
		bu_log("%p->back(%p)->forw = %p\n", \
		       (void *)(p), (void *)(p)->back, (void *)(p)->back->forw); \
		bu_bomb("Goodbye\n"); \
	}


/**
 * Determine if a face has any "dangling" edges.
 *
 * Return
 * 1 face has dangling edge
 * 0 face does not have a dangling edge
 */
int
nmg_dangling_face(const struct faceuse *fu, register const char *manifolds)
{
    struct loopuse *lu;
    struct edgeuse *eu;
    const struct edgeuse *eur;
    struct faceuse *newfu;

    NMG_CK_FACEUSE(fu);

    if (RTG.NMG_debug & DEBUG_MANIF)
	bu_log("nmg_dangling_face(%p %s)\n", (void *)fu, manifolds);

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	BU_LIST_LINK_CHECK(&lu->l);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
	    /* go looking around each edge for a face of the same
	     * shell which isn't us and isn't our mate.  If we
	     * find us or our mate before another face of this
	     * shell, we are non-3-manifold.
	     */

	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {

		NMG_CK_EDGEUSE(eu);
		BU_LIST_LINK_CHECK(&eu->l);

		eur = nmg_radial_face_edge_in_shell(eu);
		newfu = eur->up.lu_p->up.fu_p;

		/* skip any known dangling-edge faces or
		 * faces known to be 2manifolds.
		 */
		while (manifolds &&
		       NMG_MANIFOLDS(manifolds, newfu) & NMG_2MANIFOLD &&
		       eur != eu->eumate_p) {
		    eur = nmg_radial_face_edge_in_shell(
			eur->eumate_p);
		    newfu = eur->up.lu_p->up.fu_p;
		}

		if (eur == eu->eumate_p) {
		    goto out;
		}
	    }
	}
    }
    eur = (const struct edgeuse *)NULL;

 out:
    if (RTG.NMG_debug & DEBUG_BASIC) {
	struct bn_tol tol;	/* HACK */
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 1;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-5;
	tol.para = 1 - tol.perp;

	bu_log("nmg_dangling_face(fu=%p, manifolds=%s) dangling_eu=%p\n", (void *)fu, manifolds, (void *)eur);
	if (eur) nmg_pr_fu_around_eu(eur, &tol);
    }
    if ((RTG.NMG_debug & DEBUG_MANIF) && (eur != (const struct edgeuse *)NULL))
	bu_log("\tdangling eu %p\n", (void *)eur);

    return eur != (const struct edgeuse *)NULL;
}


/**
 * "Paint" the elements of a face with a meaning.  For example
 * mark everything in a face as being part of a 2-manifold
 */
static void paint_face(struct faceuse *fu, long *paint_table, long paint_color, char *paint_meaning, char *tbl)
{
    struct faceuse *newfu;
    struct loopuse *lu;
    struct edgeuse *eu;
    const struct edgeuse *eur;

    if (RTG.NMG_debug & DEBUG_MANIF)
	bu_log("nmg_paint_face(%p, %ld)\n", (void *)fu, paint_color);

    if (NMG_INDEX_VALUE(paint_table, fu->index) != 0)
	return;

    NMG_INDEX_ASSIGN(paint_table, fu, paint_color);

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {

	if (RTG.NMG_debug & DEBUG_MANIF)
	    bu_log("\tlu=%p\n", (void *)lu);

	NMG_CK_LOOPUSE(lu);
	BU_LIST_LINK_CHECK(&lu->l);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    if (RTG.NMG_debug & DEBUG_MANIF)
		bu_log("\t\teu=%p\n", (void *)eu);
	    NMG_CK_EDGEUSE(eu);
	    NMG_CK_EDGEUSE(eu->eumate_p);
	    eur = nmg_radial_face_edge_in_shell(eu);
	    NMG_CK_EDGEUSE(eur);
	    NMG_CK_FACEUSE(eur->up.lu_p->up.fu_p);
	    newfu = eur->up.lu_p->up.fu_p;

	    if (RTG.NMG_debug & DEBUG_MANIF)
		bu_log("\t\t\teur=%p, newfu=%p\n", (void *)eur, (void *)newfu);

	    BU_LIST_LINK_CHECK(&eu->l);
	    BU_LIST_LINK_CHECK(&eur->l);

	    while (NMG_MANIFOLDS(tbl, newfu) & NMG_2MANIFOLD &&
		   eur != eu->eumate_p) {
		eur = nmg_radial_face_edge_in_shell(
		    eur->eumate_p);
		newfu = eur->up.lu_p->up.fu_p;

		if (RTG.NMG_debug & DEBUG_MANIF)
		    bu_log("\t\t\teur=%p, newfu=%p\n", (void *)eur, (void *)newfu);
	    }

	    if (newfu == fu->fumate_p)
		continue;
	    else if (newfu->orientation == OT_SAME)
		paint_face(newfu, paint_table, paint_color,
			   paint_meaning, tbl);
	    else {
		/* mark this group as being interior */
		paint_meaning[paint_color] = PAINT_INTERIOR;

		if (RTG.NMG_debug & DEBUG_MANIF)
		    bu_log("\t---- Painting fu %p as interior, new_fu = %p, eu=%p, eur=%p\n",
			   (void *)fu, (void *)newfu, (void *)eu, (void *)eur);
	    }
	}
    }
}


static void set_edge_sub_manifold(char *tbl, struct edgeuse *eu_p, char manifold)
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


static void set_loop_sub_manifold(char *tbl, struct loopuse *lu_p, char manifold)
{
    struct edgeuse *eu_p;
    struct vertexuse *vu_p;

    NMG_CK_LOOPUSE(lu_p);

    NMG_SET_MANIFOLD(tbl, lu_p, manifold);
    NMG_SET_MANIFOLD(tbl, lu_p->l_p, manifold);
    if (BU_LIST_FIRST_MAGIC(&lu_p->down_hd) == NMG_VERTEXUSE_MAGIC) {
	vu_p = BU_LIST_FIRST(vertexuse, &lu_p->down_hd);
	NMG_SET_MANIFOLD(tbl, vu_p, manifold);
	NMG_SET_MANIFOLD(tbl, vu_p->v_p, manifold);
    } else if (BU_LIST_FIRST_MAGIC(&lu_p->down_hd) == NMG_EDGEUSE_MAGIC) {
	for (BU_LIST_FOR(eu_p, edgeuse, &lu_p->down_hd)) {
	    BU_LIST_LINK_CHECK(&eu_p->l);
	    set_edge_sub_manifold(tbl, eu_p, manifold);
	}
    } else
	bu_bomb("bad child of loopuse\n");
}
static void set_face_sub_manifold(char *tbl, struct faceuse *fu_p, char manifold)
{
    struct loopuse *lu_p;

    NMG_CK_FACEUSE(fu_p);
    NMG_CK_FACE(fu_p->f_p);

    NMG_SET_MANIFOLD(tbl, fu_p, manifold);
    NMG_SET_MANIFOLD(tbl, fu_p->f_p, manifold);
    for (BU_LIST_FOR(lu_p, loopuse, &fu_p->lu_hd)) {
	BU_LIST_LINK_CHECK(&lu_p->l);
	set_loop_sub_manifold(tbl, lu_p, manifold);
    }
}


char *
nmg_shell_manifolds(struct shell *sp, char *tbl)
{
    struct edgeuse *eu_p;
    struct loopuse *lu_p;
    struct faceuse *fu_p;
    char *paint_meaning;
    long *paint_table;
    long paint_color;
    int found;

    if (RTG.NMG_debug & DEBUG_MANIF)
	bu_log("nmg_shell_manifolds(%p)\n", (void *)sp);

    NMG_CK_SHELL(sp);

    if (tbl == (char *)NULL)
	tbl = (char *)bu_calloc(sp->r_p->m_p->maxindex, 1, "manifold table");

    /*
     * points in shells form 0-manifold objects.
     */
    if (sp->vu_p) {
	NMG_CK_VERTEXUSE(sp->vu_p);
	NMG_SET_MANIFOLD(tbl, sp, NMG_0MANIFOLD);
	NMG_SET_MANIFOLD(tbl, sp->vu_p, NMG_0MANIFOLD);
	NMG_SET_MANIFOLD(tbl, sp->vu_p->v_p, NMG_0MANIFOLD);
	BU_LIST_LINK_CHECK(&sp->vu_p->l);
    }

    /* edges in shells are (components of)
     * 1-manifold objects.
     */
    if (BU_LIST_NON_EMPTY(&sp->eu_hd)) {

	for (BU_LIST_FOR(eu_p, edgeuse, &sp->eu_hd)) {
	    BU_LIST_LINK_CHECK(&eu_p->l);
	    set_edge_sub_manifold(tbl, eu_p, NMG_1MANIFOLD);
	}
	NMG_SET_MANIFOLD(tbl, sp, NMG_1MANIFOLD);
    }

    /* loops in shells are (components of)
     * 1-manifold objects.
     */
    if (BU_LIST_NON_EMPTY(&sp->lu_hd)) {

	for (BU_LIST_FOR(lu_p, loopuse, &sp->lu_hd)) {
	    BU_LIST_LINK_CHECK(&lu_p->l);

	    set_loop_sub_manifold(tbl, lu_p, NMG_1MANIFOLD);
	}
	NMG_SET_MANIFOLD(tbl, sp, NMG_1MANIFOLD);
    }

    if (RTG.NMG_debug & DEBUG_MANIF)
	bu_log("starting manifold classification on shell faces\n");

    /*
     * faces may be either 2 or 3 manifold components.
     */
    if (BU_LIST_IS_EMPTY(&sp->fu_hd))
	return tbl;


    /* mark all externally dangling faces and faces
     * with or adjacent to dangling edges.
     */
    do {
	found = 0;
	for (BU_LIST_FOR(fu_p, faceuse, &sp->fu_hd)) {
	    NMG_CK_FACEUSE(fu_p);
	    BU_LIST_LINK_CHECK(&fu_p->l);

	    /* if this has already been marked as a 2-manifold
	     * then we don't need to check it again
	     */
	    if (NMG_MANIFOLDS(tbl, fu_p) & NMG_2MANIFOLD)
		continue;

	    if (nmg_dangling_face(fu_p, tbl)) {
		found = 1;

		NMG_SET_MANIFOLD(tbl, fu_p, NMG_2MANIFOLD);

		if (RTG.NMG_debug & DEBUG_MANIF)
		    bu_log("found dangling face\n");
	    }
	}
    } while (found);

    /* paint the exterior faces to discover what the
     * actual enclosing shell is
     */

    if (RTG.NMG_debug & DEBUG_MANIF)
	bu_log("starting to paint non-dangling faces\n");

    paint_meaning = (char *)bu_calloc(sp->r_p->m_p->maxindex, sizeof(char), "paint meaning table");
    paint_table = (long *)bu_calloc(sp->r_p->m_p->maxindex, sizeof(long), "paint table");
    paint_color = 1;

    for (BU_LIST_FOR(fu_p, faceuse, &sp->fu_hd)) {
	BU_LIST_LINK_CHECK(&fu_p->l);

	if (fu_p->orientation != OT_SAME ||
	    NMG_INDEX_VALUE(paint_table, fu_p->index) != 0)
	    continue;

	paint_face(fu_p, paint_table, paint_color, paint_meaning, tbl);
	paint_color++;
    }

    if (RTG.NMG_debug & DEBUG_MANIF)
	bu_log("painting done, looking at colors\n");

    /* all the faces painted with "interior" paint are 2manifolds
     * those faces still painted with "exterior" paint are
     * 3manifolds, i.e. part of the enclosing surface
     */
    for (BU_LIST_FOR(fu_p, faceuse, &sp->fu_hd)) {
	BU_LIST_LINK_CHECK(&fu_p->l);
	paint_color = NMG_INDEX_VALUE((long *)paint_table, fu_p->index);

	/* this should never trigger. */
	/* it is easier to compare against the max model index */
	if (paint_color > sp->r_p->m_p->maxindex - 1) {
	    bu_log("nmg_shell_manifolds(): ERROR, color index out of range (%ld > %ld)\n",
		   paint_color, sp->r_p->m_p->maxindex - 1);
	    bu_bomb("nmg_shell_manifolds(): ERROR, color index out of range\n");
	    break;
	}

	if (NMG_INDEX_VALUE(paint_meaning, paint_color) == PAINT_INTERIOR) {
	    set_face_sub_manifold(tbl, fu_p, NMG_2MANIFOLD);
	} else if (NMG_INDEX_VALUE(paint_meaning, paint_color) == PAINT_EXTERIOR) {
	    set_face_sub_manifold(tbl, fu_p, NMG_3MANIFOLD);
	}
    }

    bu_free(paint_meaning, "paint meaning table");
    bu_free(paint_table, "paint table");

    for (BU_LIST_FOR(fu_p, faceuse, &sp->fu_hd)) {
	BU_LIST_LINK_CHECK(&fu_p->l);

	if (fu_p->orientation != OT_SAME)
	    NMG_CP_MANIFOLD(tbl, fu_p, fu_p->fumate_p);
    }

    return tbl;
}


char *
nmg_manifolds(struct model *m)
{
    struct nmgregion *rp;
    struct shell *sp;
    char *tbl;


    NMG_CK_MODEL(m);
    if (RTG.NMG_debug & DEBUG_MANIF)
	bu_log("nmg_manifolds(%p)\n", (void *)m);

    tbl = (char *)bu_calloc(m->maxindex, 1, "manifold table");


    for (BU_LIST_FOR(rp, nmgregion, &m->r_hd)) {
	NMG_CK_REGION(rp);
	BU_LIST_LINK_CHECK(&rp->l);

	for (BU_LIST_FOR(sp, shell, &rp->s_hd)) {

	    NMG_CK_SHELL(sp);
	    BU_LIST_LINK_CHECK(&sp->l);

	    nmg_shell_manifolds(sp, tbl);

	    /* make sure the region manifold bits are a superset
	     * of the shell manifold bits
	     */
	    NMG_CP_MANIFOLD(tbl, rp, sp);
	}
    }

    return tbl;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
