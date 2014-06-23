/*                      N M G _ B O O L . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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
/** @file primitives/nmg/nmg_bool.c
 *
 * Support for boolean operations on NMG objects.  Most of the
 * routines in here are static/local to this file.  The interfaces
 * here are the functions "nmg_do_bool" and "nmg_mesh_faces".  The
 * former does boolean operations on a pair of shells.  The latter is
 * a function to make edges shared between two faces whenever
 * possible.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "plot3.h"


extern int nmg_class_nothing_broken;

/* XXX Move to nmg_manif.c or nmg_ck.c */
struct dangling_faceuse_state {
    char *visited;
    const char *manifolds;
    int count;
};


int debug_file_count=0;


/**
 * Find open edges, if any, in NMG object pointed to by magic_p and
 * create a UNIX plot file containing these edges.
 *
 * The prefix string will be appended to the front of the file name
 * of the plot file.
 *
 * Returns -
 * 0  No open edges, no plot file created.
 * !0 Has open edges, plot file created.
 */
size_t
nmg_plot_open_edges(const uint32_t *magic_p, const char *prefix)
{
    struct loopuse *lu;
    struct edgeuse *eu;
    const struct edgeuse *eur;
    struct faceuse *newfu;
    struct bu_ptbl faces;
    struct face *fp;
    struct faceuse *fu, *fu1, *fu2;
    int done;
    const char *manifolds = NULL;
    point_t pt1, pt2;
    size_t i;
    FILE *plotfp = NULL;
    struct bu_vls plot_file_name = BU_VLS_INIT_ZERO;
    size_t cnt;

    bu_ptbl_init(&faces, 64, "faces buffer");
    nmg_face_tabulate(&faces, magic_p);

    cnt = 0;
    for (i = 0; i < (size_t)BU_PTBL_END(&faces) ; i++) {
	fp = (struct face *)BU_PTBL_GET(&faces, i);
	NMG_CK_FACE(fp);
	fu = fu1 = fp->fu_p;
	NMG_CK_FACEUSE(fu1);
	fu2 = fp->fu_p->fumate_p;
	NMG_CK_FACEUSE(fu2);
	done = 0;
	while (!done) {
	    NMG_CK_FACEUSE(fu);
	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);
			eur = nmg_radial_face_edge_in_shell(eu);
			newfu = eur->up.lu_p->up.fu_p;
			while (manifolds &&
			       NMG_MANIFOLDS(manifolds, newfu) &
			       NMG_2MANIFOLD &&
			       eur != eu->eumate_p) {
			    eur = nmg_radial_face_edge_in_shell(eur->eumate_p);
			    newfu = eur->up.lu_p->up.fu_p;
			}
			if (eur == eu->eumate_p) {
			    VMOVE(pt1, eu->vu_p->v_p->vg_p->coord);
			    VMOVE(pt2, eu->eumate_p->vu_p->v_p->vg_p->coord);
			    if (!plotfp) {
				bu_vls_sprintf(&plot_file_name, "%s.%p.pl", prefix, (void *)magic_p);
				if ((plotfp = fopen(bu_vls_addr(&plot_file_name), "wb")) == (FILE *)NULL) {
				    bu_log("nmg_plot_open_edges(): Unable to create plot file (%s)\n", bu_vls_addr(&plot_file_name));
				    bu_bomb("nmg_plot_open_edges(): Unable to create plot file.");
				}
			    }
			    pdv_3line(plotfp, pt1, pt2);
			    cnt++;
			}
		    }
		}
	    }
	    if (fu == fu1) fu = fu2;
	    if (fu == fu2) done = 1;
	};

    }

    if (plotfp) {
	(void)fclose(plotfp);
	bu_vls_free(&plot_file_name);
    }

    bu_ptbl_free(&faces);

    return cnt;
}


static void
nmg_dangling_handler(uint32_t *longp, genptr_t state, int UNUSED(unused))
{
    register struct faceuse *fu = (struct faceuse *)longp;
    register struct dangling_faceuse_state *sp =
	(struct dangling_faceuse_state *)state;

    NMG_CK_FACEUSE(fu);
    if (fu->orientation != OT_SAME) return;
    /* If this faceuse has been processed before, do nothing more */
    if (!NMG_INDEX_FIRST_TIME(sp->visited, fu)) return;

    if (nmg_dangling_face(fu, sp->manifolds)) {
	sp->count++;
    }
}


/**
 * Argument is expected to be model, region, shell, or faceuse
 * pointer.
 *
 * Returns -
 * 0 No dangling faces
 * !0 Has dangling faces
 */
int
nmg_has_dangling_faces(uint32_t *magic_p, const char *manifolds)
{
    struct shell *s;
    struct dangling_faceuse_state st;
    static const struct nmg_visit_handlers handlers = {NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, nmg_dangling_handler, NULL,
						       NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, NULL, NULL};
    /* handlers.bef_faceuse = nmg_dangling_handler; */

    s = nmg_find_shell(magic_p);
    NMG_CK_SHELL(s);
    st.visited = (char *)bu_calloc(s->maxindex+1, sizeof(char), "visited[]");
    st.manifolds = manifolds;
    st.count = 0;

    nmg_visit(magic_p, &handlers, (genptr_t)&st);

    bu_free((char *)st.visited, "visited[]");
    return st.count;
}


/**
 * Within a shell, show each loop as a separate display.  Pause after
 * displaying each one.
 *
 * Note that in "non-fancy" mode, show_broken_eu() draws just the
 * edge.
 */
void
nmg_show_each_loop(struct shell *s, char **classlist, int redraw, int fancy, const char *str)


/* non-zero means flush previous vlist */
/* non-zero means pause after the display */
{
    struct faceuse *fu;
    struct loopuse *lu;
    char buf[128];
    long save;

    NMG_CK_SHELL(s);
    save = RTG.NMG_debug;
    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	NMG_CK_FACEUSE(fu);
	if (fu->orientation == OT_OPPOSITE) continue;
	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	    NMG_CK_LOOPUSE(lu);
	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC)
		continue;
	    /* Display only OT_SAME, and OT_UNSPEC et.al.  */
	    if (lu->orientation == OT_OPPOSITE) continue;

	    snprintf(buf, 128, "%s=%p", str, (void *)lu);
	    nmg_show_broken_classifier_stuff(&lu->l.magic, classlist, redraw, fancy, buf);
	}
    }
    for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
	snprintf(buf, 128, "%s=%p (wire)", str, (void *)lu);
	nmg_show_broken_classifier_stuff(&lu->l.magic, classlist, redraw, fancy, buf);
    }
    RTG.NMG_debug = save;		/* restore it */
}


void
stash_shell(struct shell *s, char *file_name, char *title, const struct bn_tol *tol)
{
    struct shell *new_s;
    struct faceuse *fu;
    char counted_name[256];

    new_s = nmg_ms();

    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	if (fu->orientation != OT_SAME)
	    continue;

	(void)nmg_dup_face(fu, new_s);
    }

    nmg_rebound(s, tol);
    snprintf(counted_name, 256, "%s%d.g", file_name, debug_file_count);
    nmg_stash_shell_to_file(counted_name, new_s, title);
    nmg_ks(new_s);
}


void
nmg_kill_non_common_cracks(struct shell *sA, struct shell *sB)
{
    struct faceuse *fu;
    struct faceuse *fu_next;

    if (RTG.NMG_debug & DEBUG_BASIC)
	bu_log("nmg_kill_non_common_cracks(s=%p and %p)\n", (void *)sA, (void *)sB);

    NMG_CK_SHELL(sA);
    NMG_CK_SHELL(sB);

    fu = BU_LIST_FIRST(faceuse, &sA->fu_hd);
    while (BU_LIST_NOT_HEAD(fu, &sA->fu_hd)) {
	struct loopuse *lu;
	struct loopuse *lu_next;
	int empty_face=0;

	NMG_CK_FACEUSE(fu);

	fu_next = BU_LIST_PNEXT(faceuse, &fu->l);
	while (BU_LIST_NOT_HEAD(fu_next, &sA->fu_hd)
	       && fu_next == fu->fumate_p)
	    fu_next = BU_LIST_PNEXT(faceuse, &fu_next->l);

	lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	while (BU_LIST_NOT_HEAD(lu, &fu->lu_hd)) {
	    struct edgeuse *eu;
	    struct edgeuse *eu_next;
	    int empty_loop=0;

	    NMG_CK_LOOPUSE(lu);

	    lu_next = BU_LIST_PNEXT(loopuse, &lu->l);

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
		lu = lu_next;
		continue;
	    }

	crack_topA:
	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		NMG_CK_EDGEUSE(eu);

		eu_next = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
		NMG_CK_EDGEUSE(eu_next);

		/* check if eu and eu_next form a jaunt */
		if (eu->vu_p->v_p != eu_next->eumate_p->vu_p->v_p)
		    continue;

		/* check if vertex at apex is in other shell
		 * if so, we need this vertex, can't kill crack
		 */
		if (nmg_find_v_in_shell(eu_next->vu_p->v_p, sB, 0))
		    continue;

		if (nmg_keu(eu))
		    empty_loop = 1;
		else if (nmg_keu(eu_next))
		    empty_loop = 1;

		if (empty_loop)
		    break;

		goto crack_topA;
	    }
	    if (empty_loop) {
		if (nmg_klu(lu)) {
		    empty_face = 1;
		    break;
		}
	    }
	    lu = lu_next;
	}
	if (empty_face) {
	    if (nmg_kfu(fu)) {
		break;
	    }
	}
	fu = fu_next;
    }

    fu = BU_LIST_FIRST(faceuse, &sB->fu_hd);
    while (BU_LIST_NOT_HEAD(fu, &sB->fu_hd)) {
	struct loopuse *lu;
	struct loopuse *lu_next;
	int empty_face=0;

	NMG_CK_FACEUSE(fu);

	fu_next = BU_LIST_PNEXT(faceuse, &fu->l);
	while (BU_LIST_NOT_HEAD(fu_next, &sB->fu_hd)
	       && fu_next == fu->fumate_p)
	    fu_next = BU_LIST_PNEXT(faceuse, &fu_next->l);

	lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	while (BU_LIST_NOT_HEAD(lu, &fu->lu_hd)) {
	    struct edgeuse *eu;
	    struct edgeuse *eu_next;
	    int empty_loop=0;

	    NMG_CK_LOOPUSE(lu);

	    lu_next = BU_LIST_PNEXT(loopuse, &lu->l);

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
		lu = lu_next;
		continue;
	    }

	crack_top:
	    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		NMG_CK_EDGEUSE(eu);

		eu_next = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
		NMG_CK_EDGEUSE(eu_next);

		/* check if eu and eu_next form a jaunt */
		if (eu->vu_p->v_p != eu_next->eumate_p->vu_p->v_p)
		    continue;

		/* check if crack apex is in other shell */
		if (nmg_find_v_in_shell(eu_next->vu_p->v_p, sA, 0))
		    continue;

		if (nmg_keu(eu))
		    empty_loop = 1;
		else if (nmg_keu(eu_next))
		    empty_loop = 1;

		if (empty_loop)
		    break;

		goto crack_top;
	    }
	    if (empty_loop) {
		if (nmg_klu(lu)) {
		    empty_face = 1;
		    break;
		}
	    }
	    lu = lu_next;
	}
	if (empty_face) {
	    if (nmg_kfu(fu)) {
		break;
	    }
	}
	fu = fu_next;
    }
}


/**
 * Preprocessor routine for classifier to get all the easy shared
 * edges and vertices marked as shared.
 */

static void
nmg_classify_shared_edges_verts(struct shell *sA, struct shell *sB, char **classlist)
{
    struct bu_ptbl verts;
    struct bu_ptbl edges;
    int i;

    if (RTG.NMG_debug & DEBUG_CLASSIFY)
	bu_log("nmg_classify_shared_edges_verts(sA=%p, sB=%p)\n", (void *)sA, (void *)sB);

    NMG_CK_SHELL(sA);
    NMG_CK_SHELL(sB);

    nmg_vertex_tabulate(&verts, &sA->magic);
    for (i=0; i<BU_PTBL_END(&verts); i++) {
	struct vertex *v;
	struct vertexuse *vu;

	v = (struct vertex *)BU_PTBL_GET(&verts, i);
	NMG_CK_VERTEX(v);

	for (BU_LIST_FOR(vu, vertexuse, &v->vu_hd)) {
	    NMG_CK_VERTEXUSE(vu);

	    if (nmg_find_s_of_vu(vu) == sB) {
		/* set classification in both lists */
		NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], v);
		NMG_INDEX_SET(classlist[4 + NMG_CLASS_AonBshared], v);

		if (RTG.NMG_debug & DEBUG_CLASSIFY)
		    bu_log("nmg_classify_shared_edges_verts: v=%p is shared\n", (void *)v);

		break;
	    }
	}
    }
    bu_ptbl_free(&verts);

    nmg_edge_tabulate(&edges, &sA->magic);
    for (i=0; i<BU_PTBL_END(&edges); i++) {
	struct edge *e;
	struct edgeuse *eu;
	struct edgeuse *eu_start;

	e = (struct edge *)BU_PTBL_GET(&edges, i);
	NMG_CK_EDGE(e);

	eu_start = e->eu_p;
	NMG_CK_EDGEUSE(eu_start);

	eu = eu_start;
	do {
	    if (nmg_find_s_of_eu(eu) == sB) {
		NMG_INDEX_SET(classlist[NMG_CLASS_AonBshared], e);
		NMG_INDEX_SET(classlist[4 + NMG_CLASS_AonBshared], e);

		if (RTG.NMG_debug & DEBUG_CLASSIFY)
		    bu_log("nmg_classify_shared_edges_verts: e=%p is shared\n", (void *)e);

		break;
	    }

	    eu = eu->eumate_p->radial_p;
	    NMG_CK_EDGEUSE(eu);

	} while (eu != eu_start && eu->eumate_p != eu_start);
    }
    bu_ptbl_free(&edges);
}


/**
 * Look for same loop in opposite direction in shell "s", Kill them.
 */

void
nmg_kill_anti_loops(struct shell *s)
{
    struct bu_ptbl loops;
    struct faceuse *fu;
    struct loopuse *lu;
    register int i, j;

    bu_ptbl_init(&loops, 64, " &loops");

    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {

	if (fu->orientation != OT_SAME)
	    continue;

	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;

	    bu_ptbl_ins(&loops, (long *)lu);
	}
    }

    for (i=0; i < BU_PTBL_END(&loops); i++) {
	struct loopuse *lu1;
	struct edgeuse *eu1_start;
	struct vertex *v1;

	lu1 = (struct loopuse *)BU_PTBL_GET(&loops, i);

	eu1_start = BU_LIST_FIRST(edgeuse, &lu1->down_hd);
	v1 = eu1_start->vu_p->v_p;

	for (j=i+1; j<BU_PTBL_END(&loops); j++) {
	    register struct loopuse *lu2;
	    register struct edgeuse *eu1;
	    register struct edgeuse *eu2;
	    register struct vertexuse *vu2;
	    register struct faceuse *fu1, *fu2;
	    int anti=1;

	    lu2 = (struct loopuse *)BU_PTBL_GET(&loops, j);

	    /* look for v1 in lu2 */
	    vu2 = nmg_find_vertex_in_lu(v1, lu2);

	    if (!vu2)
		continue;

	    /* found common vertex, now look for the rest */
	    eu2 = vu2->up.eu_p;
	    eu1 = eu1_start;
	    do {
		eu2 = BU_LIST_PNEXT_CIRC(edgeuse, &eu2->l);
		eu1 = BU_LIST_PPREV_CIRC(edgeuse, &eu1->l);

		if (eu2->vu_p->v_p != eu1->vu_p->v_p) {
		    anti = 0;
		    break;
		}
	    } while (eu1 != eu1_start);

	    if (!anti)
		continue;

	    fu1 = lu1->up.fu_p;
	    fu2 = lu2->up.fu_p;

	    if (fu1 == fu2)
		continue;

	    /* remove from loops prior to kill so we don't pass around
	     * free'd pointers.  fine for ptbl, but misleading.
	     */
	    bu_ptbl_rm(&loops, (long *)lu1);
	    bu_ptbl_rm(&loops, (long *)lu2);

	    if (nmg_klu(lu1)) {
		if (nmg_kfu(fu1))
		    goto out;
	    }
	    if (nmg_klu(lu2)) {
		if (nmg_kfu(fu2))
		    goto out;
	    }

	    i--;
	    break;
	}
    }
out:
    bu_ptbl_free(&loops);
}


void
nmg_kill_wire_edges(struct shell *s)
{
    struct loopuse *lu;
    struct edgeuse *eu;

    while (BU_LIST_NON_EMPTY(&s->lu_hd)) {
	lu = BU_LIST_FIRST(loopuse, &s->lu_hd);
	nmg_klu(lu);
    }

    while (BU_LIST_NON_EMPTY(&s->eu_hd)) {
	eu = BU_LIST_FIRST(edgeuse, &s->eu_hd);
	nmg_keu(eu);
    }
}


/**
 * Perform boolean operations on a pair of shells.
 *
 * The return is an updated version of shell A.  Shell B is destroyed.
 *
 * XXX this probably should operate on regions, not shells.
 */
static struct shell * nmg_bool(struct shell *sA, struct shell *sB, const int oper, const struct bn_tol *tol)
{
    int i;
    long nelem;
    char *classlist[8];
    FILE *fd, *fp;

    NMG_CK_SHELL(sA);
    NMG_CK_SHELL(sB);

    /* for the simple case where shells sA and sB are disjoint by at
     * least distance tolerance, we can skip most of the steps to
     * perform the boolean operation
     */
    if (V3RPP_DISJOINT_TOL(sA->sa_p->min_pt, sA->sa_p->max_pt,
			   sB->sa_p->min_pt, sB->sa_p->max_pt, tol->dist)) {
	switch (oper) {
	    case NMG_BOOL_ADD: {
		struct faceuse *fu;
		vect_t s_min_pt;
		vect_t s_max_pt;

		/* find new sA shell bounding box which combines the
		 * bounding boxes of shells sA and sB.
		 */
		VSETALL(s_min_pt, MAX_FASTF);
		VSETALL(s_max_pt, -MAX_FASTF);
		VMIN(s_min_pt, sA->sa_p->min_pt);
		VMIN(s_min_pt, sB->sa_p->min_pt);
		VMIN(s_min_pt, sB->sa_p->max_pt);
		VMAX(s_max_pt, sA->sa_p->max_pt);
		VMAX(s_max_pt, sB->sa_p->min_pt);
		VMAX(s_max_pt, sB->sa_p->max_pt);

		/* move all the faceuse from shell sB to shell sA */
		for (BU_LIST_FOR(fu, faceuse, &sB->fu_hd)) {
		    fu->s_p = sA;
		}
		BU_LIST_APPEND_LIST(&(sA->fu_hd), &(sB->fu_hd));

		/* assign new bounding box to sA */
		VMOVE(sA->sa_p->min_pt, s_min_pt);
		VMOVE(sA->sa_p->max_pt, s_max_pt);

		/* kill shell sB */
		nmg_ks(sB);
		break;
	    }
	    case NMG_BOOL_SUB:
		/* kill shell sB */
		nmg_ks(sB);
		break;
	    case NMG_BOOL_ISECT:
		/* kill the contents of shell sA */
		while (BU_LIST_NON_EMPTY(&sA->fu_hd)) {
		    (void)nmg_kfu(BU_LIST_FIRST(faceuse, &sA->fu_hd));
		}
		while (BU_LIST_NON_EMPTY(&sA->lu_hd)) {
		    (void)nmg_klu(BU_LIST_FIRST(loopuse, &sA->lu_hd));
		}
		while (BU_LIST_NON_EMPTY(&sA->eu_hd)) {
		    (void)nmg_keu(BU_LIST_FIRST(edgeuse, &sA->eu_hd));
		}
		if (sA->vu_p) {
		    nmg_kvu(sA->vu_p);
		}
		/* kill shell sB */
		nmg_ks(sB);
		break;
	    default:
		bu_bomb("nmg_bool(): unknown operation\n");
	}
	return sA;
    }

    debug_file_count++;
    if (RTG.NMG_debug & DEBUG_VERIFY) {
	/* Sometimes the tessellations of non-participating regions
	 * are damaged during a boolean operation.  Check everything.
	 */
	nmg_vsshell(sA);
    }

    nmg_shell_coplanar_face_merge(sA, tol, 1);
    nmg_shell_coplanar_face_merge(sB, tol, 1);

    nmg_shell_fuse(sA, tol);
    nmg_shell_fuse(sB, tol);

    if (nmg_check_closed_shell(sA, tol)) {
	if (RTG.NMG_debug & DEBUG_BOOL &&
	    RTG.NMG_debug & DEBUG_PLOTEM) {
	    if ((fp=fopen("Unclosed.plot3", "wb")) != (FILE *)NULL) {
		bu_log("Plotting unclosed NMG shell\n");
		nmg_pl_s(fp, sA);
		fclose(fp);
	    }
	}
	if (RTG.NMG_debug & DEBUG_BOOL)
	    nmg_pr_s(sA);

	bu_log("nmg_bool: sA is unclosed, barging ahead\n");
    }

    if (nmg_check_closed_shell(sB, tol)) {
	if (RTG.NMG_debug & DEBUG_BOOL &&
	    RTG.NMG_debug & DEBUG_PLOTEM) {
	    if ((fp=fopen("Unclosed.plot3", "wb")) != (FILE *)NULL) {
		bu_log("Plotting unclosed NMG shell\n");
		nmg_pl_s(fp, sB);
		fclose(fp);
	    }
	}
	if (RTG.NMG_debug & DEBUG_BOOL)
	    nmg_pr_s(sB);
	bu_log("nmg_bool: sB is unclosed, barging ahead\n");
    }


    if (RTG.NMG_debug & DEBUG_BOOL && RTG.NMG_debug & DEBUG_PLOTEM) {
	if ((fp=fopen("shellA.plot3", "wb")) == (FILE*)NULL) {
	    (void)perror("shellA.plot3");
	    bu_bomb("unable to open shellA.plot3 for writing");
	}
	bu_log("plotting shellA.plot3\n");
	nmg_pl_s(fp, sA);
	fclose(fp);

	if ((fp=fopen("shellB.plot3", "wb")) == (FILE*)NULL) {
	    (void)perror("shellB.plot3");
	    bu_bomb("unable to open shellB.plot3 for writing");
	}
	bu_log("plotting shellB.plot3\n");
	nmg_pl_s(fp, sB);
	fclose(fp);
    }

    if (RTG.NMG_debug & DEBUG_VERIFY) {
	/* Sometimes the tessellations of non-participating regions
	 * are damaged during a boolean operation.  Check everything.
	 */
	nmg_vsshell(sA);
    }

    if (RTG.NMG_debug & DEBUG_BOOL) {
	char file_name[256];

	sprintf(file_name, "before%d.g", debug_file_count);
	nmg_stash_shell_to_file(file_name, sA, "Before crackshells");
    }

    /* Perform shell/shell intersections */
    nmg_crackshells(sA, sB, tol);

    if (RTG.NMG_debug & DEBUG_BOOL) {
	stash_shell(sA, "a1_", "sA", tol);
	stash_shell(sB, "b1_", "sB", tol);
	bu_log("Just After Crackshells:\nShell A:\n");
	nmg_pr_s_briefly(sA, 0);
	bu_log("Just After Crackshells:\nShell B:\n");
	nmg_pr_s_briefly(sB, 0);
    }

    (void)nmg_vertex_fuse(&sA->magic, tol);
    (void)nmg_vertex_fuse(&sB->magic, tol);

    (void)nmg_kill_anti_loops(sA);
    (void)nmg_kill_anti_loops(sB);

    /* clean things up now that the intersections have been built */
    nmg_sanitize_s_lv(sA, OT_BOOLPLACE);
    nmg_sanitize_s_lv(sB, OT_BOOLPLACE);

    /* Separate any touching loops, so classifier does not have any
     * really complex loops to do.
     * In particular, it is necessary for (r410) to make
     * interior (touching) loop segments into true interior loops
     * that are separate from the exterior loop,
     * so the classifier can assess each separately.
     */
    nmg_s_split_touchingloops(sA, tol);
    nmg_s_split_touchingloops(sB, tol);

    (void)nmg_kill_cracks(sA);
    (void)nmg_kill_cracks(sB);

    /* eliminate unnecessary breaks in edges */
    (void)nmg_simplify_shell_edges(sA, tol);
    (void)nmg_simplify_shell_edges(sB, tol);

    (void)nmg_break_e_on_v(&sA->magic, tol);
    (void)nmg_break_e_on_v(&sB->magic, tol);

    (void)nmg_edge_fuse(&sA->magic, tol);
    (void)nmg_edge_fuse(&sB->magic, tol);

    if (RTG.NMG_debug & DEBUG_VERIFY) {
	/* Sometimes the tessellations of non-participating regions
	 * are damaged during a boolean operation.  Check everything.
	 */
	nmg_vsshell(sA);
	if ((i = nmg_shell_fuse(sA, tol)) > 0) {
	    bu_log("NOTICE: nmg_bool: fused %d entities while cracking shells\n", i);
	    bu_bomb("nmg_bool() entities unfused after nmg_crackshells()\n");
	}
    }

    if (RTG.NMG_debug & DEBUG_BOOL) {
	int dangle_error = 0;
	if (nmg_has_dangling_faces((uint32_t *)sA, (char *)NULL)) {
	    dangle_error = 1;
	    bu_log("nmg_bool(): Dangling faces detected in sA before classification\n");
	}
	if (nmg_has_dangling_faces((uint32_t *)sB, (char *)NULL)) {
	    dangle_error = 1;
	    bu_log("nmg_bool(): Dangling faces detected in sB before classification\n");
	}
	if (dangle_error) {
	    nmg_stash_shell_to_file("dangle.g", sA, "After Boolean");
	    bu_bomb("nmg_bool(): Dangling faces detected before classification\n");
	}
    }

    if (RTG.NMG_debug & DEBUG_VERIFY) {
	/* Sometimes the tessellations of non-participating regions
	 * are damaged during a boolean operation.  Check everything.
	 */
	nmg_vsshell(sA);
	nmg_vsshell(sB);
    }

    /*
     * Before splitting, join up small loop fragments into large
     * ones, so that maximal splits will be possible.
     * This is essential for cutting holes in faces, e.g. Test3.r
     */
    if (RTG.NMG_debug & DEBUG_BOOL) {
	char file_name[256];

	sprintf(file_name, "notjoined%d.g", debug_file_count);
	nmg_stash_shell_to_file(file_name, sA, "Before s_join_touchingloops");
    }

    /* Re-build bounding boxes, edge geometry, as needed. */
    nmg_shell_a(sA, tol);
    nmg_shell_a(sB, tol);

    if (RTG.NMG_debug & DEBUG_BOOL) {
	stash_shell(sA, "a", "sA", tol);
	stash_shell(sB, "b", "sB", tol);

	bu_log("sA:\n");
	nmg_pr_s_briefly(sA, 0);
	bu_log("sB:\n");
	nmg_pr_s_briefly(sB, 0);
    }

    if (RTG.NMG_debug & DEBUG_BOOL) {
	char file_name[256];

	sprintf(file_name, "after%d.g", debug_file_count);
	nmg_stash_shell_to_file(file_name, sA, "After crackshells");
    }

    if (RTG.NMG_debug & DEBUG_BOOL) {
	if (RTG.NMG_debug & DEBUG_PLOTEM) {
	    if ((fd = fopen("Cracked_Shells.plot3", "wb")) == (FILE *)NULL) {
		(void)perror("Cracked_Shells");
		bu_bomb("unable to open Cracked_Shells.plot3 for writing");
	    }
	    bu_log("plotting Cracked_Shells.plot3\n");

	    nmg_pl_s(fd, sA);
	    nmg_pl_s(fd, sB);
	    (void)fclose(fd);

	    nmg_pl_isect("isectA.plot3", sA, tol);
	    nmg_pl_isect("isectB.plot3", sB, tol);
	}

	bu_log("check 2\n");
    }

    if (nmg_ck_closed_surf(sA, tol))
	bu_log("nmg_bool() WARNING: sA unclosed before classification.  Boldly pressing on.\n");
    if (nmg_ck_closed_surf(sB, tol))
	bu_log("nmg_bool() WARNING: sB unclosed before classification.  Boldly pressing on.\n");

    if (RTG.NMG_debug & DEBUG_VERIFY) {
	/* Sometimes the tessellations of non-participating regions
	 * are damaged during a boolean operation.  Check everything.
	 */
	nmg_vsshell(sA);
	nmg_vsshell(sB);
    }

    nmg_s_reindex(sA, 0);
    nmg_s_reindex(sB, 0);

    /* Allocate storage for classlist[]. Allocate each of the 8 class
     * lists one at a time. This will assist with debugging to
     * determine if each array read/write is within its allocated space.
     */
    nelem = sA->maxindex + sB->maxindex;
    for (i = 0; i < 8; i++) {
	classlist[i] = (char *)bu_calloc(nelem, sizeof(char), "nmg_bool classlist");
    }

    nmg_classify_shared_edges_verts(sA, sB, classlist);

    nmg_class_nothing_broken = 1;
    if (RTG.NMG_debug & (DEBUG_GRAPHCL|DEBUG_PL_LOOP)) {
	nmg_show_broken_classifier_stuff((uint32_t *)sA, &classlist[0], nmg_class_nothing_broken, 1, "unclassed sA");
	nmg_show_broken_classifier_stuff((uint32_t *)sB, &classlist[4], 1, 1, "unclassed sB");
    }

 //   if (sA->manifolds) {
	//bu_free((char *)sA->manifolds, "free manifolds table");
	//sA->manifolds = (char *)NULL;
 //   }

 //   sA->manifolds = nmg_manifolds(sA);

 //   if (sB->manifolds) {
	//bu_free((char *)sB->manifolds, "free manifolds table");
	//sB->manifolds = (char *)NULL;
 //   }

 //   sB->manifolds = nmg_manifolds(sB);


    /*
     * Classify A -vs- B, then B -vs- A.
     * Carry onAonBshared and onAonBanti classifications forward
     * from first step to second step.
     * A -vs- B live in classlist[0..3], B -vs- A live in classlist[4..7].
     */
    nmg_class_shells(sA, sB, &classlist[0], tol);
    memcpy((char *)classlist[4+NMG_CLASS_AonBshared],
	   (char *)classlist[0+NMG_CLASS_AonBshared],
	   nelem*sizeof(char));
    memcpy((char *)classlist[4+NMG_CLASS_AonBanti],
	   (char *)classlist[0+NMG_CLASS_AonBanti],
	   nelem*sizeof(char));
    memcpy((char *)classlist[4+NMG_CLASS_AoutB],
	   (char *)classlist[0+NMG_CLASS_AoutB],
	   nelem*sizeof(char));
    nmg_class_shells(sB, sA, &classlist[4], tol);

    if (sA->manifolds) {
	bu_free((char *)sA->manifolds, "free manifolds table");
	sA->manifolds = (char *)NULL;
    }

    if (sB->manifolds) {
	bu_free((char *)sB->manifolds, "free manifolds table");
	sB->manifolds = (char *)NULL;
    }

    if (RTG.NMG_debug & (DEBUG_GRAPHCL|DEBUG_PL_LOOP)) {
	nmg_class_nothing_broken = 1;

	/* Show each loop, one at a time, non-fancy */
	/* XXX Should have its own bit, or combination -- not always wanted */
	nmg_show_each_loop(sA, &classlist[0], 1, 0, "sA lu");
	nmg_show_each_loop(sB, &classlist[4], 1, 0, "sB lu");

	/* Show each shell as a whole */
	nmg_show_broken_classifier_stuff((uint32_t *)sA, &classlist[0], 1, 0, "sA classed");
	nmg_show_broken_classifier_stuff((uint32_t *)sB, &classlist[4], 1, 0, "sB classed");
    }

    if (RTG.NMG_debug & DEBUG_BOOL) {
	bu_log("Just before nmg_evaluate_boolean:\nShell A:\n");
	nmg_pr_s_briefly(sA, 0);
	bu_log("Shell B:\n");
	nmg_pr_s_briefly(sB, 0);
    }

    nmg_s_radial_check(sA, tol);
    nmg_s_radial_check(sB, tol);
    nmg_evaluate_boolean(sA, sB, oper, classlist, tol);
    sB = NULL; /* sanity, killed during boolean eval */

    if (RTG.NMG_debug & DEBUG_BOOL) {
	bu_log("Just after nmg_evaluate_boolean:\nShell A:\n");
	nmg_pr_s_briefly(sA, 0);
	bu_log("Shell B:\nFreed.");
    }

    if (RTG.NMG_debug & DEBUG_VERIFY) {
	nmg_vsshell(sA);
	if ((i = nmg_shell_fuse(sA, tol)) > 0) {
	    bu_log("ERROR: nmg_bool: fused %d entities after BOOLEAN.  Isect bug.\n", i);
	    bu_bomb("nmg_bool() entities unfused after nmg_evaluate_boolean()\n");
	}
    }

    /*
     * nmg_evaluate_boolean() may return an invalid shell, i.e., one
     * that has absolutely nothing in it.  This is an indication that
     * the shell should be deleted from the region, an operation which
     * can not be accomplished this far down in the subroutine tree.
     */
    if (!nmg_shell_is_empty(sA)) {

	nmg_s_radial_check(sA, tol);

	if (RTG.NMG_debug & DEBUG_BOOL) {
	    int dangle_error = 0;
	    if (nmg_has_dangling_faces((uint32_t *)sA, (char *)NULL)) {
		dangle_error = 1;
		bu_log("nmg_bool(): Dangling faces detected in sA after boolean\n");
	    }
	    if (nmg_has_dangling_faces((uint32_t *)sB, (char *)NULL)) {
		dangle_error = 1;
		bu_log("nmg_bool(): Dangling faces detected in sB after boolean\n");
	    }
	    if (dangle_error) {
		nmg_stash_shell_to_file("dangle.g", sA, "After Boolean");
		bu_bomb("nmg_bool(): Dangling faces detected after boolean\n");
	    }
	} else {
	    if (nmg_has_dangling_faces((uint32_t *)sA, (char *)NULL)) {
		(void)nmg_plot_open_edges((const uint32_t *)sA, "open_edges");
		bu_bomb("nmg_bool(): Dangling faces detected in sA after boolean\n");
	    }
	}

	/* Do this before table size changes */
	if (RTG.NMG_debug & (DEBUG_GRAPHCL|DEBUG_PL_LOOP)) {
	    nmg_class_nothing_broken = 1;

	    /* Show final result of the boolean */
	    nmg_show_broken_classifier_stuff((uint32_t *)sA, &classlist[0], 1, 0, "sA result");
	}

	/* Go back and combine loops of faces together wherever
	 * possible to reduce the loop/edge count.
	 */
	nmg_simplify_shell(sA);
	if (RTG.NMG_debug & DEBUG_VERIFY)
	    nmg_vsshell(sA);

	(void) nmg_unbreak_shell_edges(&sA->magic);

	if (RTG.NMG_debug & DEBUG_BOOL) {
	    bu_log("Just after nmg_simplify_shell:\nShell A:\n");
	    nmg_pr_s_briefly(sA, 0);
	}

	/* Bounding boxes may have changed */
	nmg_shell_a(sA, tol);

	if (nmg_ck_closed_surf(sA, tol)) {
	    if (RTG.NMG_debug)
		bu_log("nmg_bool() WARNING: sA unclosed at return, barging on.\n");
	    else
		bu_bomb("nmg_bool() sA unclosed at return, aborting.\n");
	}
	nmg_s_radial_check(sA, tol);

	if (RTG.NMG_debug & DEBUG_BOOL) {
	    char tmp_name[256];
	    sprintf(tmp_name, "after_bool_%d.g", debug_file_count);
	    nmg_stash_shell_to_file(tmp_name, sA, "After Boolean");
	}
    }

    for (i = 0; i < 8; i++) {
	bu_free((char *)classlist[i], "nmg_bool classlist");
    }

    if (RTG.NMG_debug & DEBUG_BOOL) {
	bu_log("Returning from NMG_BOOL\n");
    }
    if (RTG.NMG_debug & DEBUG_VERIFY) {
	/* Sometimes the tessellations of non-participating regions
	 * are damaged during a boolean operation.  Check everything.
	 */
	nmg_vsshell(sA);
    }

    nmg_kill_wire_edges(sA);

    return sA;
}


/**
 * BUG: we assume only one shell per region
 */
struct shell *
nmg_do_bool(struct shell *sA, struct shell *sB, const int oper, const struct bn_tol *tol)
{
    struct shell *s;

    NMG_CK_SHELL(sA);
    NMG_CK_SHELL(sB);

    nmg_shell_v_unique(sA, tol);
    nmg_shell_v_unique(sB, tol);

    s = nmg_bool(sA, sB, oper, tol);

    /* shell B was destroyed, need to eliminate region B */
    nmg_ks(sB);

    NMG_CK_SHELL(s);

    /* If shell A became empty, eliminate it from the returned region */
    if (nmg_shell_is_empty(s)) {
	nmg_ks(s);
	return (struct shell *)NULL;
    }

    return s;
}


/* XXX move to ??? Everything from here on down needs to go into another module */


/**
 * Called from db_walk_tree() each time a tree leaf is encountered.
 * The primitive solid, in external format, is provided in 'ep', and
 * the type of that solid (e.g. ID_ELL) is in 'id'.  The full tree
 * state including the accumulated transformation matrix and the
 * current tolerancing is in 'tsp', and the full path from root to
 * leaf is in 'pathp'.
 *
 * Import the solid, tessellate it into an NMG, stash a pointer to the
 * tessellation in a new tree structure (union), and return a pointer
 * to that.
 *
 * Usually given as an argument to, and called from db_walk_tree().
 *
 * This routine must be prepared to run in parallel.
 */
union tree *
nmg_booltree_leaf_tess(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, genptr_t UNUSED(client_data))
{
    struct shell *s;
    union tree *curtree;
    struct directory *dp;

    if (!tsp || !pathp || !ip)
	return TREE_NULL;

    RT_CK_DB_INTERNAL(ip);
    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);
    RT_CK_DIR(dp);

    if (!ip->idb_meth || !ip->idb_meth->ft_tessellate) {
	bu_log("ERROR(%s): tessellation support not available\n", dp->d_namep);
	return TREE_NULL;
    }

    NMG_CK_SHELL(*tsp->ts_s);
    BN_CK_TOL(tsp->ts_tol);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    RT_CK_RESOURCE(tsp->ts_resp);

    s = nmg_ms();

    if (ip->idb_meth->ft_tessellate(&s, ip, tsp->ts_ttol, tsp->ts_tol) < 0) {
	bu_log("ERROR(%s): tessellation failure\n", dp->d_namep);
	return TREE_NULL;
    }

    if (RTG.NMG_debug & DEBUG_VERIFY) {
	nmg_vsshell(s);
    }

    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NMG_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_s = s;

    if (RT_G_DEBUG&DEBUG_TREEWALK)
	bu_log("nmg_booltree_leaf_tess(%s) OK\n", dp->d_namep);

    return curtree;
}


/**
 * Called from db_walk_tree() each time a tree leaf is encountered.
 * The primitive solid, in external format, is provided in 'ep', and
 * the type of that solid (e.g. ID_ELL) is in 'id'.  The full tree
 * state including the accumulated transformation matrix and the
 * current tolerancing is in 'tsp', and the full path from root to
 * leaf is in 'pathp'.
 *
 * Import the solid, convert it into an NMG using t-NURBS, stash a
 * pointer in a new tree structure (union), and return a pointer to
 * that.
 *
 * Usually given as an argument to, and called from db_walk_tree().
 *
 * This routine must be prepared to run in parallel.
 */
union tree *
nmg_booltree_leaf_tnurb(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, genptr_t UNUSED(client_data))
{
    union tree *curtree;
    struct directory *dp;

    NMG_CK_SHELL(*tsp->ts_s);
    BN_CK_TOL(tsp->ts_tol);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    RT_CK_DB_INTERNAL(ip);
    RT_CK_RESOURCE(tsp->ts_resp);

    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);
    RT_CK_DIR(dp);

    if (ip->idb_meth->ft_tnurb(
	    tsp->ts_s, ip, tsp->ts_tol) < 0) {
	bu_log("nmg_booltree_leaf_tnurb(%s): CSG to t-NURB conversation failure\n", dp->d_namep);
	return TREE_NULL;
    }

    if (RTG.NMG_debug & DEBUG_VERIFY) {
	nmg_vsshell(*tsp->ts_s);
    }

    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NMG_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_s = *tsp->ts_s;

    if (RT_G_DEBUG&DEBUG_TREEWALK)
	bu_log("nmg_booltree_leaf_tnurb(%s) OK\n", dp->d_namep);

    return curtree;
}


/* quell the output of nmg_booltree_evaluate() to bu_log. */
int nmg_bool_eval_silent=0;

/**
 * Given a tree of leaf nodes tessellated earlier by
 * nmg_booltree_leaf_tess(), use recursion to do a depth-first
 * traversal of the tree, evaluating each pair of boolean operations
 * and reducing that result to a single nmgregion.
 *
 * Usually called from a do_region_end() handler from db_walk_tree().
 * For an example of several, see mged/dodraw.c.
 *
 * Returns an OP_NMG_TESS union tree node, which will contain the
 * resulting region and its name, as a dynamic string.  The caller is
 * responsible for releasing the string, and the node, by calling
 * db_free_tree() on the node.
 *
 * It is *essential* that the caller call nmg_model_fuse() before
 * calling this subroutine.
 *
 * Returns NULL if there is no geometry to return.
 *
 * Typical calls will be of this form:
 * (void)nmg_model_fuse(m, tol);
 * curtree = nmg_booltree_evaluate(curtree, tol);
 */
union tree *
nmg_booltree_evaluate(register union tree *tp, const struct bn_tol *tol, struct resource *resp)
{
    union tree *tl;
    union tree *tr;
    struct shell *reg;
    int op = NMG_BOOL_ADD;      /* default value */
    const char *op_str = " u "; /* default value */
    size_t rem;
    char *name;

    RT_CK_TREE(tp);
    BN_CK_TOL(tol);
    RT_CK_RESOURCE(resp);

    switch (tp->tr_op) {
	case OP_NOP:
	    return TREE_NULL;
	case OP_NMG_TESS:
	    /* Hit a tree leaf */
	    if (RTG.NMG_debug & DEBUG_VERIFY) {
		nmg_vsshell(tp->tr_d.td_s);
	    }
	    return tp;
	case OP_UNION:
	    op = NMG_BOOL_ADD;
	    op_str = " u ";
	    break;
	case OP_INTERSECT:
	    op = NMG_BOOL_ISECT;
	    op_str = " + ";
	    break;
	case OP_SUBTRACT:
	    op = NMG_BOOL_SUB;
	    op_str = " - ";
	    break;
	default:
	    bu_bomb("nmg_booltree_evaluate(): bad op\n");
    }

    /* Handle a boolean operation node.  First get its leaves. */
    tl = nmg_booltree_evaluate(tp->tr_b.tb_left, tol, resp);
    tr = nmg_booltree_evaluate(tp->tr_b.tb_right, tol, resp);

    if (tl) {
	RT_CK_TREE(tl);
	if (tl != tp->tr_b.tb_left) {
	    bu_bomb("nmg_booltree_evaluate(): tl != tp->tr_b.tb_left\n");
	}
    }
    if (tr) {
	RT_CK_TREE(tr);
	if (tr != tp->tr_b.tb_right) {
	    bu_bomb("nmg_booltree_evaluate(): tr != tp->tr_b.tb_right\n");
	}
    }

    if (!tl && !tr) {
	/* left-r == null && right-r == null */
	RT_CK_TREE(tp);
	db_free_tree(tp->tr_b.tb_left, resp);
	db_free_tree(tp->tr_b.tb_right, resp);
	tp->tr_op = OP_NOP;
	return TREE_NULL;
    }

    if (tl && !tr) {
	/* left-r != null && right-r == null */
	RT_CK_TREE(tp);
	db_free_tree(tp->tr_b.tb_right, resp);
	if (op == NMG_BOOL_ISECT) {
	    /* OP_INTERSECT '+' */
	    RT_CK_TREE(tp);
	    db_free_tree(tl, resp);
	    tp->tr_op = OP_NOP;
	    return TREE_NULL;
	} else {
	    /* copy everything from tl to tp no matter which union type
	     * could probably have done a mem-copy
	     */
	    tp->tr_op = tl->tr_op;
	    tp->tr_b.tb_regionp = tl->tr_b.tb_regionp;
	    tp->tr_b.tb_left = tl->tr_b.tb_left;
	    tp->tr_b.tb_right = tl->tr_b.tb_right;

	    /* null data from tl so only to free this node */
	    tl->tr_b.tb_regionp = (struct region *)NULL;
	    tl->tr_b.tb_left = TREE_NULL;
	    tl->tr_b.tb_right = TREE_NULL;

	    db_free_tree(tl, resp);
	    return tp;
	}
    }

    if (!tl && tr) {
	/* left-r == null && right-r != null */
	RT_CK_TREE(tp);
	db_free_tree(tp->tr_b.tb_left, resp);
	if (op == NMG_BOOL_ADD) {
	    /* OP_UNION 'u' */
	    /* copy everything from tr to tp no matter which union type
	     * could probably have done a mem-copy
	     */
	    tp->tr_op = tr->tr_op;
	    tp->tr_b.tb_regionp = tr->tr_b.tb_regionp;
	    tp->tr_b.tb_left = tr->tr_b.tb_left;
	    tp->tr_b.tb_right = tr->tr_b.tb_right;

	    /* null data from tr so only to free this node */
	    tr->tr_b.tb_regionp = (struct region *)NULL;
	    tr->tr_b.tb_left = TREE_NULL;
	    tr->tr_b.tb_right = TREE_NULL;

	    db_free_tree(tr, resp);
	    return tp;

	} else if ((op == NMG_BOOL_SUB) || (op == NMG_BOOL_ISECT)) {
	    /* for sub and intersect, if left-hand-side is null, result is null */
	    RT_CK_TREE(tp);
	    db_free_tree(tr, resp);
	    tp->tr_op = OP_NOP;
	    return TREE_NULL;

	} else {
	    bu_bomb("nmg_booltree_evaluate(): error, unknown operation\n");
	}
    }

    if (tl->tr_op != OP_NMG_TESS) {
	bu_bomb("nmg_booltree_evaluate(): bad left tree\n");
    }
    if (tr->tr_op != OP_NMG_TESS) {
	bu_bomb("nmg_booltree_evaluate(): bad right tree\n");
    }

    if (!nmg_bool_eval_silent) {
	bu_log(" {%s}%s{%s}\n", tl->tr_d.td_name, op_str, tr->tr_d.td_name);
    }

    NMG_CK_SHELL(tr->tr_d.td_s);
    NMG_CK_SHELL(tl->tr_d.td_s);

    if (nmg_ck_closed_surf(tr->tr_d.td_s, tol)) {
	bu_bomb("nmg_booltree_evaluate(): ERROR, non-closed shell (r)\n");
    }
    if (nmg_ck_closed_surf(tl->tr_d.td_s, tol)) {
	bu_bomb("nmg_booltree_evaluate(): ERROR, non-closed shell (l)\n");
    }

    nmg_s_radial_check(tr->tr_d.td_s, tol);
    nmg_s_radial_check(tl->tr_d.td_s, tol);

    if (RTG.NMG_debug & DEBUG_BOOL) {
	bu_log("Before model fuse\nShell A:\n");
	nmg_pr_s_briefly(tl->tr_d.td_s, "");
	bu_log("Shell B:\n");
	nmg_pr_s_briefly(tl->tr_d.td_s, "");
    }

    /* move operands into the same model */
    //nmg_merge_shells(tl->tr_d.td_s, tr->tr_d.td_s);

    /* input s1 and s2 are destroyed, output is new shell */
    reg = nmg_do_bool(tl->tr_d.td_s, tr->tr_d.td_s, op, tol);

    /* build string of result name */
    rem = strlen(tl->tr_d.td_name) + 3 + strlen(tr->tr_d.td_name) + 2 + 1;
    name = (char *)bu_calloc(rem, sizeof(char), "nmg_booltree_evaluate name");
    snprintf(name, rem, "(%s%s%s)", tl->tr_d.td_name, op_str, tr->tr_d.td_name);

    /* clean up child tree nodes */
    tl->tr_d.td_s = (struct shell *)NULL;
    tr->tr_d.td_s = (struct shell *)NULL;
    db_free_tree(tl, resp);
    db_free_tree(tr, resp);


    if (reg) {
	/* convert argument binary node into a result node */
	NMG_CK_SHELL(reg);
	nmg_s_radial_check(reg, tol);
	tp->tr_op = OP_NMG_TESS;
	tp->tr_d.td_s = reg;
	tp->tr_d.td_name = name;

	if (RTG.NMG_debug & DEBUG_VERIFY) {
	    nmg_vsshell(reg);
	}
	return tp;

    } else {
	/* resulting region was null */
	tp->tr_op = OP_NOP;
	return TREE_NULL;
    }

}


/**
 * This is the main application interface to the NMG Boolean
 * Evaluator.
 *
 * This routine has the opportunity to do "once only" operations
 * before and after the boolean tree is walked.
 *
 * Returns -
 * 0 Boolean went OK.  Result region is in tp->tr_d.td_r
 * !0 Boolean produced null result.
 *
 * The caller is responsible for freeing 'tp' in either case,
 * typically with db_free_tree(tp);
 */
int
nmg_boolean(union tree *tp, struct shell *s, const struct bn_tol *tol, struct resource *resp)
{
    union tree *result;
    int ret;

    RT_CK_TREE(tp);
    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);
    RT_CK_RESOURCE(resp);

    if (RTG.NMG_debug & (DEBUG_BOOL|DEBUG_BASIC)) {
	bu_log("\n\nnmg_boolean(tp=%p, s=%p) START\n",
	       (void *)tp, (void *)s);
    }

    /* The nmg_model_fuse function was removed from this point in the
     * boolean process since not all geometry that is to be processed is
     * always within the single 'm' nmg model structure passed into this
     * function. In some cases the geometry resides in multiple nmg model
     * structures within the 'tp' tree that is passed into this function.
     * Running nmg_model_fuse is still required but is done later, i.e.
     * within the nmg_booltree_evaluate function just before the nmg_do_bool
     * function is called which is when the geometry, in which the boolean
     * to be performed, is always in a single nmg model structure.
     */

    /*
     * Evaluate the nodes of the boolean tree one at a time, until
     * only a single region remains.
     */
    result = nmg_booltree_evaluate(tp, tol, resp);

    if (result == TREE_NULL) {
	bu_log("nmg_boolean(): result of nmg_booltree_evaluate() is NULL\n");
	rt_pr_tree(tp, 0);
	ret = 1;
	goto out;
    }

    if (result != tp) {
	bu_bomb("nmg_boolean(): result of nmg_booltree_evaluate() isn't tp\n");
    }

    RT_CK_TREE(result);

    if (tp->tr_op != OP_NMG_TESS) {
	bu_log("nmg_boolean(): result of nmg_booltree_evaluate() op != OP_NMG_TESS\n");
	rt_pr_tree(tp, 0);
	ret = 1;
	goto out;
    }

    if (tp->tr_d.td_s == (struct shell *)NULL) {
	/* Pointers are all OK, but boolean produced null set */
	ret = 1;
	goto out;
    }

    /* move result into correct model */
    //nmg_merge_shells(s, tp->tr_d.td_s);
    ret = 0;

out:
    if (RTG.NMG_debug & (DEBUG_BOOL|DEBUG_BASIC)) {
	bu_log("nmg_boolean(tp=%p, s=%p) END, ret=%d\n\n",
	       (void *)tp, (void *)s, ret);
    }

    return ret;
}
/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
