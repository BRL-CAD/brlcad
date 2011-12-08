/*                      N M G _ B O O L . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2011 United States Government as represented by
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


extern int nmg_class_nothing_broken;

/* XXX Move to nmg_manif.c or nmg_ck.c */
struct dangling_faceuse_state {
    char *visited;
    const char *manifolds;
    int count;
};


int debug_file_count=0;

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
 * N M G _ H A S _ D A N G L I N G _ F A C E S
 *
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
    struct model *m;
    struct dangling_faceuse_state st;
    static const struct nmg_visit_handlers handlers = {NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, nmg_dangling_handler, NULL,
						       NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, NULL, NULL};
    /* handlers.bef_faceuse = nmg_dangling_handler; */

    m = nmg_find_model(magic_p);
    NMG_CK_MODEL(m);
    st.visited = (char *)bu_calloc(m->maxindex+1, sizeof(char), "visited[]");
    st.manifolds = manifolds;
    st.count = 0;

    nmg_visit(magic_p, &handlers, (genptr_t)&st);

    bu_free((char *)st.visited, "visited[]");
    return st.count;
}


/**
 * N M G _ S H O W _ E A C H _ L O O P
 *
 * Within a shell, show each loop as a separate display.  Pause after
 * displaying each one.
 *
 * Note that in "non-fancy" mode, show_broken_eu() draws just the
 * edge.
 */
void
nmg_show_each_loop(struct shell *s, char **classlist, int new, int fancy, const char *str)


    /* non-zero means flush previous vlist */
    /* non-zero means pause after the display */
{
    struct faceuse *fu;
    struct loopuse *lu;
    char buf[128];
    long save;

    NMG_CK_SHELL(s);
    save = rt_g.NMG_debug;
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
	    nmg_show_broken_classifier_stuff(&lu->l.magic, classlist, new, fancy, buf);
	}
    }
    for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
	snprintf(buf, 128, "%s=%p (wire)", str, (void *)lu);
	nmg_show_broken_classifier_stuff(&lu->l.magic, classlist, new, fancy, buf);
    }
    rt_g.NMG_debug = save;		/* restore it */
}


void
stash_shell(struct shell *s, char *file_name, char *title, const struct bn_tol *tol)
{
    struct model *m;
    struct nmgregion *r;
    struct shell *new_s;
    struct faceuse *fu;
    char counted_name[256];

    m = nmg_mm();
    r = nmg_mrsv(m);
    new_s = BU_LIST_FIRST(shell, &r->s_hd);

    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	if (fu->orientation != OT_SAME)
	    continue;

	(void)nmg_dup_face(fu, new_s);
    }

    nmg_rebound(m, tol);
    snprintf(counted_name, 256, "%s%d.g", file_name, debug_file_count);
    nmg_stash_model_to_file(counted_name, m, title);
    nmg_km(m);
}


void
nmg_kill_non_common_cracks(struct shell *sA, struct shell *sB)
{
    struct faceuse *fu;
    struct faceuse *fu_next;

    if (rt_g.NMG_debug & DEBUG_BASIC)
	bu_log("nmg_kill_non_common_cracks(s=%x and %x)\n", sA, sB);

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
 * N M G _ C L A S S I F Y _ S H A R E D _ E D G E S _ V E R T S
 *
 * Preprocessor routine for classifier to get all the easy shared
 * edges and vertices marked as shared.
 */

static void
nmg_classify_shared_edges_verts(struct shell *sA, struct shell *sB, char **classlist)
{
    struct bu_ptbl verts;
    struct bu_ptbl edges;
    int i;

    if (rt_g.NMG_debug & DEBUG_CLASSIFY)
	bu_log("nmg_classify_shared_edges_verts(sA=x%x, sB=x%x)\n", sA, sB);

    NMG_CK_SHELL(sA);
    NMG_CK_SHELL(sB);

    nmg_vertex_tabulate(&verts, &sA->l.magic);
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

		if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		    bu_log("nmg_classify_shared_edges_verts: v=x%x is shared\n", v);

		break;
	    }
	}
    }
    bu_ptbl_free(&verts);

    nmg_edge_tabulate(&edges, &sA->l.magic);
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

		if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		    bu_log("nmg_classify_shared_edges_verts: e=x%x is shared\n", e);

		break;
	    }

	    eu = eu->eumate_p->radial_p;
	    NMG_CK_EDGEUSE(eu);

	} while (eu != eu_start && eu->eumate_p != eu_start);
    }
    bu_ptbl_free(&edges);
}


/**
 * N M G _ K I L L _ A N T I _ L O O P S
 *
 * Look for same loop in opposite direction in shell "s", Kill them.
 */

void
nmg_kill_anti_loops(struct shell *s, const struct bn_tol *tol)
{
    struct bu_ptbl loops;
    struct faceuse *fu;
    struct loopuse *lu;
    int i, j;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    bu_ptbl_init(&loops, 64, " &loops");

    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	NMG_CK_FACEUSE(fu);

	if (fu->orientation != OT_SAME)
	    continue;

	for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	    NMG_CK_LOOPUSE(lu);

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
	NMG_CK_LOOPUSE(lu1);

	eu1_start = BU_LIST_FIRST(edgeuse, &lu1->down_hd);
	NMG_CK_EDGEUSE(eu1_start);
	v1 = eu1_start->vu_p->v_p;
	NMG_CK_VERTEX(v1);

	for (j=i+1; j<BU_PTBL_END(&loops); j++) {
	    struct loopuse *lu2;
	    struct edgeuse *eu1;
	    struct edgeuse *eu2;
	    struct vertexuse *vu2;
	    struct faceuse *fu1, *fu2;
	    int anti=1;

	    lu2 = (struct loopuse *)BU_PTBL_GET(&loops, j);
	    NMG_CK_LOOPUSE(lu2);

	    /* look for v1 in lu2 */
	    vu2 = nmg_find_vertex_in_lu(v1, lu2);

	    if (!vu2)
		continue;

	    /* found common vertex, now look for the rest */
	    eu2 = vu2->up.eu_p;
	    NMG_CK_EDGEUSE(eu2);
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

	    if (nmg_klu(lu1)) {
		if (nmg_kfu(fu1))
		    goto out;
	    }
	    if (nmg_klu(lu2)) {
		if (nmg_kfu(fu2))
		    goto out;
	    }

	    bu_ptbl_rm(&loops, (long *)lu1);
	    bu_ptbl_rm(&loops, (long *)lu2);
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
 * N M G _ B O O L
 *
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
    struct model *m;
    struct nmgregion *rA;
    struct nmgregion *rB;

    NMG_CK_SHELL(sA);
    NMG_CK_SHELL(sB);
    rA = sA->r_p;
    rB = sB->r_p;
    NMG_CK_REGION(rA);
    NMG_CK_REGION(rB);
    m = rA->m_p;
    NMG_CK_MODEL(m);

    if (sA->r_p->m_p != sB->r_p->m_p) {
        bu_bomb("nmg_bool(): internal error, both shells are not in the same nmg model\n");
    }

    /* for the simple case where shells sA and sB do not overlap, we
     * can skip most of the steps to perform the boolean operation
     */
    if (!V3RPP_OVERLAP_TOL(sA->sa_p->min_pt, sA->sa_p->max_pt, 
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
    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	/* Sometimes the tessllations of non-participating regions
	 * are damaged during a boolean operation.  Check everything.
	 */
	nmg_vmodel(m);
    }

    nmg_shell_coplanar_face_merge(sA, tol, 1);
    nmg_shell_coplanar_face_merge(sB, tol, 1);

    nmg_model_fuse(m, tol);

    if (nmg_check_closed_shell(sA, tol)) {
	if (rt_g.NMG_debug & DEBUG_BOOL &&
	    rt_g.NMG_debug & DEBUG_PLOTEM) {
	    if ((fp=fopen("Unclosed.pl", "wb")) != (FILE *)NULL) {
		bu_log("Plotting unclosed NMG shell\n");
		nmg_pl_s(fp, sA);
		fclose(fp);
	    }
	}
	if (rt_g.NMG_debug & DEBUG_BOOL)
	    nmg_pr_s(sA, "");

	bu_log("nmg_bool: sA is unclosed, barging ahead\n");
    }

    if (nmg_check_closed_shell(sB, tol)) {
	if (rt_g.NMG_debug & DEBUG_BOOL &&
	    rt_g.NMG_debug & DEBUG_PLOTEM) {
	    if ((fp=fopen("Unclosed.pl", "wb")) != (FILE *)NULL) {
		bu_log("Plotting unclosed NMG shell\n");
		nmg_pl_s(fp, sB);
		fclose(fp);
	    }
	}
	if (rt_g.NMG_debug & DEBUG_BOOL)
	    nmg_pr_s(sB, "");
	bu_log("nmg_bool: sB is unclosed, barging ahead\n");
    }


    if (rt_g.NMG_debug & DEBUG_BOOL && rt_g.NMG_debug & DEBUG_PLOTEM) {
	if ((fp=fopen("shellA.pl", "wb")) == (FILE*)NULL) {
	    (void)perror("shellA.pl");
	    bu_bomb("unable to open shellA.pl for writing");
	}
	bu_log("plotting shellA.pl\n");
	nmg_pl_s(fp, sA);
	fclose(fp);

	if ((fp=fopen("shellB.pl", "wb")) == (FILE*)NULL) {
	    (void)perror("shellB.pl");
	    bu_bomb("unable to open shellB.pl for writing");
	}
	bu_log("plotting shellB.pl\n");
	nmg_pl_s(fp, sB);
	fclose(fp);
    }

    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	/* Sometimes the tessllations of non-participating regions
	 * are damaged during a boolean operation.  Check everything.
	 */
	nmg_vmodel(m);
    }

    if (rt_g.NMG_debug & DEBUG_BOOL) {
	char file_name[256];

	sprintf(file_name, "before%d.g", debug_file_count);
	nmg_stash_model_to_file(file_name, m, "Before crackshells");
    }

    /* Perform shell/shell intersections */
    nmg_crackshells(sA, sB, tol);

    if (rt_g.NMG_debug & DEBUG_BOOL) {
	stash_shell(sA, "a1_", "sA", tol);
	stash_shell(sB, "b1_", "sB", tol);
	bu_log("Just After Crackshells:\nShell A:\n");
	nmg_pr_s_briefly(sA, 0);
	bu_log("Just After Crackshells:\nShell B:\n");
	nmg_pr_s_briefly(sB, 0);
    }

    (void)nmg_model_vertex_fuse(m, tol);

    (void)nmg_kill_anti_loops(sA, tol);
    (void)nmg_kill_anti_loops(sB, tol);

    nmg_m_reindex(m, 0);

    /* Allocate storage for classlist[]. Allocate each of the 8 class
     * lists one at a time. This will assist with debugging to
     * determine if each array read/write is within its allocated space.
     */

    nelem = m->maxindex;
    for (i = 0; i < 8; i++) {
        classlist[i] = (char *)bu_calloc(nelem, sizeof(char), "nmg_bool classlist");
    }

    nmg_classify_shared_edges_verts(sA, sB, classlist);

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

    /* eliminate unecessary breaks in edges */
    (void)nmg_simplify_shell_edges(sA, tol);
    (void)nmg_simplify_shell_edges(sB, tol);

    (void)nmg_model_break_e_on_v(m, tol);

    (void)nmg_model_edge_fuse(m, tol);

    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	/* Sometimes the tessllations of non-participating regions
	 * are damaged during a boolean operation.  Check everything.
	 */
	nmg_vmodel(m);
	if ((i = nmg_model_fuse(m, tol)) > 0) {
	    bu_log("NOTICE: nmg_bool: fused %d entities while cracking shells\n", i);
	    bu_bomb("nmg_bool() entities unfused after nmg_crackshells()\n");
	}
    }

    /* Temporary search */
    if (nmg_has_dangling_faces((uint32_t *)rA, (char *)NULL))
	bu_log("Dangling faces detected in rA before classification\n");
    if (nmg_has_dangling_faces((uint32_t *)rB, (char *)NULL))
	bu_log("Dangling faces detected in rB before classification\n");
    if (nmg_has_dangling_faces((uint32_t *)m, (char *)NULL))
	bu_log("Dangling faces detected in model before classification\n");

    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	/* Sometimes the tessllations of non-participating regions
	 * are damaged during a boolean operation.  Check everything.
	 */
	nmg_vmodel(m);

    }

    /*
     * Before splitting, join up small loop fragments into large
     * ones, so that maximal splits will be possible.
     * This is essential for cutting holes in faces, e.g. Test3.r
     */
    if (rt_g.NMG_debug & DEBUG_BOOL) {
	char file_name[256];

	sprintf(file_name, "notjoined%d.g", debug_file_count);
	nmg_stash_model_to_file(file_name, m, "Before s_join_touchingloops");
    }

    /* Re-build bounding boxes, edge geometry, as needed. */
    nmg_shell_a(sA, tol);
    nmg_shell_a(sB, tol);

    if (rt_g.NMG_debug & DEBUG_BOOL) {
	stash_shell(sA, "a", "sA", tol);
	stash_shell(sB, "b", "sB", tol);

	bu_log("sA:\n");
	nmg_pr_s_briefly(sA, 0);
	bu_log("sB:\n");
	nmg_pr_s_briefly(sB, 0);
    }

    if (rt_g.NMG_debug & DEBUG_BOOL) {
	char file_name[256];

	sprintf(file_name, "after%d.g", debug_file_count);
	nmg_stash_model_to_file(file_name, m, "After crackshells");
    }

    if (rt_g.NMG_debug & DEBUG_BOOL) {
	if (rt_g.NMG_debug & DEBUG_PLOTEM) {
	    if ((fd = fopen("Cracked_Shells.pl", "wb")) == (FILE *)NULL) {
		(void)perror("Cracked_Shells");
		bu_bomb("unable to open Cracked_Shells.pl for writing");
	    }
	    bu_log("plotting Cracked_Shells.pl\n");

	    nmg_pl_s(fd, sA);
	    nmg_pl_s(fd, sB);
	    (void)fclose(fd);

	    nmg_pl_isect("isectA.pl", sA, tol);
	    nmg_pl_isect("isectB.pl", sB, tol);
	}

	bu_log("check 2\n");
    }

    if (nmg_ck_closed_surf(sA, tol))
	bu_log("nmg_bool() WARNING: sA unclosed before classification.  Boldly pressing on.\n");
    if (nmg_ck_closed_surf(sB, tol))
	bu_log("nmg_bool() WARNING: sB unclosed before classification.  Boldly pressing on.\n");

    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	/* Sometimes the tessllations of non-participating regions
	 * are damaged during a boolean operation.  Check everything.
	 */
	nmg_vmodel(m);
    }

    nmg_class_nothing_broken = 1;
    if (rt_g.NMG_debug & (DEBUG_GRAPHCL|DEBUG_PL_LOOP)) {
	nmg_show_broken_classifier_stuff((uint32_t *)sA, &classlist[0], nmg_class_nothing_broken, 1, "unclassed sA");
	nmg_show_broken_classifier_stuff((uint32_t *)sB, &classlist[4], 1, 1, "unclassed sB");
    }

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

    if (rt_g.NMG_debug & (DEBUG_GRAPHCL|DEBUG_PL_LOOP)) {
	nmg_class_nothing_broken = 1;

	/* Show each loop, one at a time, non-fancy */
	/* XXX Should have its own bit, or combination -- not always wanted */
	nmg_show_each_loop(sA, &classlist[0], 1, 0, "sA lu");
	nmg_show_each_loop(sB, &classlist[4], 1, 0, "sB lu");

	/* Show each shell as a whole */
	nmg_show_broken_classifier_stuff((uint32_t *)sA, &classlist[0], 1, 0, "sA classed");
	nmg_show_broken_classifier_stuff((uint32_t *)sB, &classlist[4], 1, 0, "sB classed");
    }

    if (rt_g.NMG_debug & DEBUG_BOOL) {
	bu_log("Just before nmg_evaluate_boolean:\nShell A:\n");
	nmg_pr_s_briefly(sA, 0);
	bu_log("Shell B:\n");
	nmg_pr_s_briefly(sB, 0);
    }

    nmg_s_radial_check(sA, tol);
    nmg_s_radial_check(sB, tol);
    nmg_evaluate_boolean(sA, sB, oper, classlist, tol);

    if (rt_g.NMG_debug & DEBUG_BOOL) {
	bu_log("Just after nmg_evaluate_boolean:\nShell A:\n");
	nmg_pr_s_briefly(sA, 0);
	bu_log("Shell B:\n");
	nmg_pr_s_briefly(sB, 0);
    }

    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_vmodel(m);
	if ((i = nmg_model_fuse(m, tol)) > 0) {
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
	/* Temporary search */
	if (nmg_has_dangling_faces((uint32_t *)rA, (char *)NULL))
	    bu_log("Dangling faces detected in rA after boolean\n");
	if (nmg_has_dangling_faces((uint32_t *)rB, (char *)NULL))
	    bu_log("Dangling faces detected in rB after boolean\n");
	if (nmg_has_dangling_faces((uint32_t *)m, (char *)NULL)) {
	    if (rt_g.NMG_debug)
		nmg_stash_model_to_file("dangle.g", m, "After Boolean");
	    bu_bomb("nmg_bool() Dangling faces detected after boolean\n");
	}

	/* Do this before table size changes */
	if (rt_g.NMG_debug & (DEBUG_GRAPHCL|DEBUG_PL_LOOP)) {
	    nmg_class_nothing_broken = 1;

	    /* Show final result of the boolean */
	    nmg_show_broken_classifier_stuff((uint32_t *)sA, &classlist[0], 1, 0, "sA result");
	}

	/* Go back and combine loops of faces together wherever
	 * possible to reduce the loop/edge count.
	 */
	nmg_simplify_shell(sA);
	if (rt_g.NMG_debug & DEBUG_VERIFY)
	    nmg_vshell(&rA->s_hd, rA);

	(void) nmg_unbreak_region_edges(&sA->l.magic);

	if (rt_g.NMG_debug & DEBUG_BOOL) {
	    bu_log("Just after nmg_simplify_shell:\nShell A:\n");
	    nmg_pr_s_briefly(sA, 0);
	    bu_log("Shell B:\n");
	    nmg_pr_s_briefly(sB, 0);
	}

	/* Bounding boxes may have changed */
	nmg_shell_a(sA, tol);

	if (nmg_ck_closed_surf(sA, tol)) {
	    if (rt_g.NMG_debug)
		bu_log("nmg_bool() WARNING: sA unclosed at return, barging on.\n");
	    else
		bu_bomb("nmg_bool() sA unclosed at return, aborting.\n");
	}
	nmg_s_radial_check(sA, tol);

	if (rt_g.NMG_debug & DEBUG_BOOL) {
	    char tmp_name[256];
	    sprintf(tmp_name, "after_bool_%d.g", debug_file_count);
	    nmg_stash_model_to_file(tmp_name, m, "After Boolean");
	}
    }

    for (i = 0; i < 8; i++) {
        bu_free((char *)classlist[i], "nmg_bool classlist");
    }

    if (rt_g.NMG_debug & DEBUG_BOOL) {
	bu_log("Returning from NMG_BOOL\n");
    }
    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	/* Sometimes the tessllations of non-participating regions
	 * are damaged during a boolean operation.  Check everything.
	 */
	nmg_vmodel(m);
    }

    nmg_kill_wire_edges(sA);

    return sA;
}


/**
 * N M G _ D O _ B O O L
 *
 * BUG: we assume only one shell per region
 */
struct nmgregion *
nmg_do_bool(struct nmgregion *rA, struct nmgregion *rB, const int oper, const struct bn_tol *tol)
{
    struct shell *s;
    struct nmgregion *r;

    NMG_CK_REGION(rA);
    NMG_CK_REGION(rB);

    nmg_region_v_unique(rA, tol);
    nmg_region_v_unique(rB, tol);

    s = nmg_bool(BU_LIST_FIRST(shell, &rA->s_hd),
		 BU_LIST_FIRST(shell, &rB->s_hd),
		 oper, tol);
    r = s->r_p;

    /* shell B was destroyed, need to eliminate region B */
    nmg_kr(rB);

    NMG_CK_SHELL(s);
    NMG_CK_REGION(r);

    /* If shell A became empty, eliminate it from the returned region */
    if (nmg_shell_is_empty(s)) {
	nmg_ks(s);
	if (BU_LIST_NON_EMPTY(&r->s_hd)) {
	    bu_bomb("nmg_do_bool: Result of Boolean is an empty shell, but region is not empty!!!\n");
	}
	nmg_kr(r);
	return (struct nmgregion *)NULL;
    }

    return r;
}


/* XXX move to ??? Everything from here on down needs to go into another module */


/**
 * N M G _ B O O L T R E E _ L E A F _ T E S S
 *
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
    struct model *m;
    struct nmgregion *r1;
    union tree *curtree;
    struct directory *dp;

    NMG_CK_MODEL(*tsp->ts_m);
    BN_CK_TOL(tsp->ts_tol);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    RT_CK_DB_INTERNAL(ip);
    RT_CK_RESOURCE(tsp->ts_resp);

    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);
    RT_CK_DIR(dp);

    m = nmg_mm();

    if (ip->idb_meth && ip->idb_meth->ft_tessellate && ip->idb_meth->ft_tessellate(&r1, m, ip, tsp->ts_ttol, tsp->ts_tol) < 0) {
	bu_log("nmg_booltree_leaf_tess(%s): tessellation failure\n", dp->d_namep);
	return TREE_NULL;
    }

    NMG_CK_REGION(r1);
    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_vshell(&r1->s_hd, r1);
    }

    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NMG_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_r = r1;

    if (RT_G_DEBUG&DEBUG_TREEWALK)
	bu_log("nmg_booltree_leaf_tess(%s) OK\n", dp->d_namep);

    return curtree;
}


/**
 * N M G _ B O O L T R E E _ L E A F _ T N U R B
 *
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
    struct nmgregion *r1;
    union tree *curtree;
    struct directory *dp;

    NMG_CK_MODEL(*tsp->ts_m);
    BN_CK_TOL(tsp->ts_tol);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    RT_CK_DB_INTERNAL(ip);
    RT_CK_RESOURCE(tsp->ts_resp);

    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);
    RT_CK_DIR(dp);

    if (ip->idb_meth->ft_tnurb(
	    &r1, *tsp->ts_m, ip, tsp->ts_tol) < 0) {
	bu_log("nmg_booltree_leaf_tnurb(%s): CSG to t-NURB conversation failure\n", dp->d_namep);
	return TREE_NULL;
    }

    NMG_CK_REGION(r1);
    if (rt_g.NMG_debug & DEBUG_VERIFY) {
	nmg_vshell(&r1->s_hd, r1);
    }

    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NMG_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_r = r1;

    if (RT_G_DEBUG&DEBUG_TREEWALK)
	bu_log("nmg_booltree_leaf_tnurb(%s) OK\n", dp->d_namep);

    return curtree;
}


/* quell the output of nmg_booltree_evaluate() to bu_log. */
int nmg_bool_eval_silent=0;

/**
 * N M G _ B O O L T R E E _ E V A L U A T E
 *
 * Given a tree of leaf nodes tesselated earlier by
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
    struct nmgregion *reg;
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
	    if (rt_g.NMG_debug & DEBUG_VERIFY) {
		nmg_vshell(&tp->tr_d.td_r->s_hd, tp->tr_d.td_r);
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
        if ( op == NMG_BOOL_ISECT ) {
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
        if ( op == NMG_BOOL_ADD ) {
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

    NMG_CK_REGION(tr->tr_d.td_r);
    NMG_CK_REGION(tl->tr_d.td_r);

    if (nmg_ck_closed_region(tr->tr_d.td_r, tol)) {
	bu_bomb("nmg_booltree_evaluate(): ERROR, non-closed shell (r)\n");
    }
    if (nmg_ck_closed_region(tl->tr_d.td_r, tol)) {
	bu_bomb("nmg_booltree_evaluate(): ERROR, non-closed shell (l)\n");
    }

    nmg_r_radial_check(tr->tr_d.td_r, tol);
    nmg_r_radial_check(tl->tr_d.td_r, tol);

    if (rt_g.NMG_debug & DEBUG_BOOL) {
	bu_log("Before model fuse\nShell A:\n");
	nmg_pr_s_briefly(BU_LIST_FIRST(shell, &tl->tr_d.td_r->s_hd), "");
	bu_log("Shell B:\n");
	nmg_pr_s_briefly(BU_LIST_FIRST(shell, &tr->tr_d.td_r->s_hd), "");
    }

    /* move operands into the same model */
    if (tr->tr_d.td_r->m_p != tl->tr_d.td_r->m_p) {
	nmg_merge_models(tl->tr_d.td_r->m_p, tr->tr_d.td_r->m_p);
    }

    /* input r1 and r2 are destroyed, output is new region */
    reg = nmg_do_bool(tl->tr_d.td_r, tr->tr_d.td_r, op, tol);

    /* build string of result name */
    rem = strlen(tl->tr_d.td_name) + 3 + strlen(tr->tr_d.td_name) + 2 + 1;
    name = (char *)bu_calloc(rem, sizeof(char), "nmg_booltree_evaluate name");
    snprintf(name, rem, "(%s%s%s)", tl->tr_d.td_name, op_str, tr->tr_d.td_name);

    /* clean up child tree nodes */
    tl->tr_d.td_r = (struct nmgregion *)NULL;
    tr->tr_d.td_r = (struct nmgregion *)NULL;
    db_free_tree(tl, resp);
    db_free_tree(tr, resp);


    if (reg) {
        /* convert argument binary node into a result node */
	NMG_CK_REGION(reg);
	nmg_r_radial_check(reg, tol);
        tp->tr_op = OP_NMG_TESS;
        tp->tr_d.td_r = reg;
        tp->tr_d.td_name = name;

	if (rt_g.NMG_debug & DEBUG_VERIFY) {
	    nmg_vshell(&reg->s_hd, reg);
	}
        return tp;

    } else {
        /* resulting region was null */
        tp->tr_op = OP_NOP;
        return TREE_NULL;
    }

}


/**
 * N M G _ B O O L E A N
 *
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
nmg_boolean(union tree *tp, struct model *m, const struct bn_tol *tol, struct resource *resp)
{
    union tree *result;
    int ret;

    RT_CK_TREE(tp);
    NMG_CK_MODEL(m);
    BN_CK_TOL(tol);
    RT_CK_RESOURCE(resp);

    if (rt_g.NMG_debug & (DEBUG_BOOL|DEBUG_BASIC)) {
	bu_log("\n\nnmg_boolean(tp=x%x, m=x%x) START\n",
	       tp, m);
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

    if (tp->tr_d.td_r == (struct nmgregion *)NULL) {
	/* Pointers are all OK, but boolean produced null set */
	ret = 1;
	goto out;
    }

    /* move result into correct model */
    nmg_merge_models(m, tp->tr_d.td_r->m_p);
    ret = 0;

 out:
    if (rt_g.NMG_debug & (DEBUG_BOOL|DEBUG_BASIC)) {
	bu_log("nmg_boolean(tp=x%x, m=x%x) END, ret=%d\n\n",
	       tp, m, ret);
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




