/*                      N M G _ E V A L . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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
/** @file primitives/nmg/nmg_eval.c
 *
 * Evaluate boolean operations on NMG objects.
 *
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"


struct nmg_bool_state {
    struct shell *bs_dest;
    struct shell *bs_src;
    int bs_isA;		/* true if A, else doing B */
    char **bs_classtab;
    const int *bs_actions;
    const struct bn_tol *bs_tol;
};


HIDDEN void nmg_eval_shell(struct shell *s, struct nmg_bool_state *bs);
HIDDEN void nmg_eval_plot(struct nmg_bool_state *bs, int num);


#define BACTION_KILL 1
#define BACTION_RETAIN 2
#define BACTION_RETAIN_AND_FLIP 3

static const char *nmg_baction_names[] = {
    "*undefined 0*",
    "BACTION_KILL",
    "BACTION_RETAIN",
    "BACTION_RETAIN_AND_FLIP",
    "*undefined 4*"
};


#define NMG_CLASS_BAD 8
static const char *nmg_class_names[] = {
    "onAinB",
    "onAonBshared",
    "onAonBanti",
    "onAoutB",
    "inAonB",
    "onAonBshared",
    "onAonBanti",
    "outAonB",
    "*BAD*CLASS*"
};


/**
 * N M G _ C K _ L U _ O R I E N T A T I O N
 *
 * Make sure that the lu and fu orientation flags are consistent with
 * the geometric arrangement of the vertices and the faceuse normal.
 */
void
nmg_ck_lu_orientation(struct loopuse *lu, const struct bn_tol *tolp)
{
    struct faceuse *fu;
    plane_t fu_peqn;
    plane_t lu_peqn;
    fastf_t dot;

    NMG_CK_LOOPUSE(lu);
    fu = lu->up.fu_p;		/* parent had better be faceuse */
    NMG_CK_FACEUSE(fu);

    NMG_GET_FU_PLANE(fu_peqn, fu);
    nmg_loop_plane_newell(lu, lu_peqn);

    dot = VDOT(fu_peqn, lu_peqn);

    if (NEAR_ZERO(dot, tolp->perp))
	return;		/* can't determine geometric orientation */


    if (dot < 0.0) {
	bu_log("nmg_ck_lu_orientation() lu=x%x, dot=%g, fu_orient=%s, lu_orient=%s\n", lu,
	       dot,
	       nmg_orientation(fu->orientation),
	       nmg_orientation(lu->orientation)
	    );
	bu_bomb("nmg_ck_lu_orientation() loop orientation flags do not match geometry\n");
    }
}


/**
 * N M G _ C L A S S _ N A M E
 *
 * Convert an NMG_CLASS_xxx token into a string name.
 */
const char *
nmg_class_name(int class)
{
    if (class == NMG_CLASS_Unknown) return "Unknown";
    if (class < 0 || class > NMG_CLASS_BAD) class = NMG_CLASS_BAD;
    return nmg_class_names[class];
}


/*
 * Action Table for Boolean Operations.
 *
 * Each table lists what actions are to be taken for topological elements
 * which have have each kind of classification.
 *
 * Actions are listed in this order:
 * (Aon) onAinB, onAonBshared, onAonBanti-shared, onAoutB,
 * (Bon) inAonB, onAonBshared, onAonBanti-shared, outAonB
 */
static const int subtraction_actions[8] = {
    BACTION_KILL,
    BACTION_KILL,		/* shared */
    BACTION_RETAIN,		/* anti-shared */
    BACTION_RETAIN,

    BACTION_RETAIN,		/* (formerly BACTION_RETAIN_AND_FLIP) */
    BACTION_KILL,
    BACTION_KILL,
    BACTION_KILL
};


static const int union_actions[8] = {
    BACTION_KILL,
    BACTION_RETAIN,		/* shared */
    BACTION_KILL,		/* anti-shared */
    BACTION_RETAIN,

    BACTION_KILL,
    BACTION_KILL,
    BACTION_KILL,
    BACTION_RETAIN
};


static const int intersect_actions[8] = {
    BACTION_RETAIN,
    BACTION_RETAIN,		/* shared */
    BACTION_KILL,		/* anti-shared ==> non-manifold result */
    BACTION_KILL,

    BACTION_RETAIN,
    BACTION_KILL,
    BACTION_KILL,
    BACTION_KILL
};


/**
 * N M G _ E V A L U A T E _ B O O L E A N
 *
 * Evaluate a boolean operation on the two shells "A" and "B", of the
 * form "answer = A op B".  As input, each element (loop-in-face, wire
 * loop, wire edge, vertex) in both A and B has been classified as
 * being "in", "on", or "out" of the other shell.  Using these
 * classifications, operate on the input shells.  At the end, shell A
 * contains the resultant object, and shell B is destroyed.
 *
 */
void
nmg_evaluate_boolean(struct shell *sA, struct shell *sB, int op, char **classlist, const struct bn_tol *tol)
{
    int const *actions;
    struct nmg_bool_state bool_state;

    NMG_CK_SHELL(sA);
    NMG_CK_SHELL(sB);
    BN_CK_TOL(tol);

    if (rt_g.NMG_debug & DEBUG_BOOLEVAL) {
	bu_log("nmg_evaluate_boolean(sA=x%x, sB=x%x, op=%d) START\n",
	       sA, sB, op);
    }

    switch (op) {
	case NMG_BOOL_SUB:
	    actions = subtraction_actions;
	    nmg_invert_shell(sB);	/* FLIP all faceuse normals */
	    break;
	case NMG_BOOL_ADD:
	    actions = union_actions;
	    break;
	case NMG_BOOL_ISECT:
	    actions = intersect_actions;
	    break;
	default:
	    actions = union_actions;	/* shut up lint */
	    bu_log("ERROR nmg_evaluate_boolean() op=%d.\n", op);
	    bu_bomb("bad boolean\n");
    }

    bool_state.bs_dest = sA;
    bool_state.bs_src = sB;
    bool_state.bs_classtab = classlist;
    bool_state.bs_actions = actions;
    bool_state.bs_tol = tol;

    bool_state.bs_isA = 1;
    nmg_eval_shell(sA, &bool_state);

    bool_state.bs_isA = 0;
    nmg_eval_shell(sB, &bool_state);

    if (rt_g.NMG_debug & DEBUG_BOOLEVAL) {
	bu_log("nmg_evaluate_boolean(sA=x%x, sB=x%x, op=%d), evaluations done\n",
	       sA, sB, op);
    }
    /* Write sA and sB into separate files, if wanted? */

    /* Move everything left in sB into sA.  sB is killed. */
    nmg_js(sA, sB, tol);

    /* Plot the result */
    if (rt_g.NMG_debug & DEBUG_BOOLEVAL && rt_g.NMG_debug & DEBUG_PLOTEM) {
	FILE *fp;

	if ((fp=fopen("bool_ans.pl", "wb")) == (FILE *)NULL) {
	    (void)perror("bool_ans.pl");
	    bu_bomb("unable to open bool_ans.pl for writing");
	}
	bu_log("plotting bool_ans.pl\n");
	nmg_pl_s(fp, sA);
	(void)fclose(fp);
    }

    /* Remove loops/edges/vertices that appear more than once in result */
    nmg_rm_redundancies(sA, tol);

    if (rt_g.NMG_debug & DEBUG_BOOLEVAL) {
	bu_log("nmg_evaluate_boolean(sA=x%x, sB=x%x, op=%d) END\n",
	       sA, sB, op);
    }
}


static int nmg_eval_count = 0;	/* debug -- plot file numbering */


/**
 * N M G _ E V A L _ A C T I O N
 *
 * Given a pointer to some NMG data structure, search the 4
 * classification lists to determine its classification.  (XXX In the
 * future, this should be done with one big array).  Then, return the
 * action code for an item of that classification.
 */
HIDDEN int
nmg_eval_action(unsigned long *ptr, register struct nmg_bool_state *bs)
{
    register int ret;
    register int class;
    int index;

    BN_CK_TOL(bs->bs_tol);

    index = nmg_index_of_struct(ptr);
    if (bs->bs_isA) {
	if (NMG_INDEX_VALUE(bs->bs_classtab[NMG_CLASS_AinB], index)) {
	    class = NMG_CLASS_AinB;
	    ret = bs->bs_actions[NMG_CLASS_AinB];
	    goto out;
	}
	if (NMG_INDEX_VALUE(bs->bs_classtab[NMG_CLASS_AonBshared], index)) {
	    class = NMG_CLASS_AonBshared;
	    ret = bs->bs_actions[NMG_CLASS_AonBshared];
	    goto out;
	}
	if (NMG_INDEX_VALUE(bs->bs_classtab[NMG_CLASS_AonBanti], index)) {
	    class = NMG_CLASS_AonBanti;
	    ret = bs->bs_actions[NMG_CLASS_AonBanti];
	    goto out;
	}
	if (NMG_INDEX_VALUE(bs->bs_classtab[NMG_CLASS_AoutB], index)) {
	    class = NMG_CLASS_AoutB;
	    ret = bs->bs_actions[NMG_CLASS_AoutB];
	    goto out;
	}
	bu_log("nmg_eval_action(ptr=x%x) %s has no A classification, retaining\n",
	       ptr, bu_identify_magic(*((long *)ptr)));
	class = NMG_CLASS_BAD;
	ret = BACTION_RETAIN;
	goto out;
    }

    /* is B */
    if (NMG_INDEX_VALUE(bs->bs_classtab[NMG_CLASS_BinA], index)) {
	class = NMG_CLASS_BinA;
	ret = bs->bs_actions[NMG_CLASS_BinA];
	goto out;
    }
    if (NMG_INDEX_VALUE(bs->bs_classtab[NMG_CLASS_BonAshared], index)) {
	class = NMG_CLASS_BonAshared;
	ret = bs->bs_actions[NMG_CLASS_BonAshared];
	goto out;
    }
    if (NMG_INDEX_VALUE(bs->bs_classtab[NMG_CLASS_BonAanti], index)) {
	class = NMG_CLASS_BonAanti;
	ret = bs->bs_actions[NMG_CLASS_BonAanti];
	goto out;
    }
    if (NMG_INDEX_VALUE(bs->bs_classtab[NMG_CLASS_BoutA], index)) {
	class = NMG_CLASS_BoutA;
	ret = bs->bs_actions[NMG_CLASS_BoutA];
	goto out;
    }
    bu_log("nmg_eval_action(ptr=x%x) %s has no B classification, retaining\n",
	   ptr, bu_identify_magic(*((long *)ptr)));
    class = NMG_CLASS_BAD;
    ret = BACTION_RETAIN;
out:
    if (rt_g.NMG_debug & DEBUG_BOOLEVAL) {
	bu_log("nmg_eval_action(ptr=x%x) index=%d %s %s %s %s\n",
	       ptr, index,
	       bs->bs_isA ? "A" : "B",
	       bu_identify_magic(*((long *)ptr)),
	       nmg_class_name(class),
	       nmg_baction_names[ret]);
    }
    return ret;
}


/**
 * N M G _ E V A L _ S H E L L
 *
 * Make a life-and-death decision on every element of a shell.
 * Descend the "great chain of being" from the face to loop to edge to
 * vertex, saving or demoting along the way.
 *
 * Note that there is no moving of items from one shell to another.
 */
HIDDEN void
nmg_eval_shell(register struct shell *s, struct nmg_bool_state *bs)
{
    struct faceuse *fu;
    struct faceuse *nextfu;
    struct loopuse *lu;
    struct loopuse *nextlu;
    struct edgeuse *eu;
    struct edgeuse *nexteu;
    struct vertexuse *vu;
    int loops_retained;

    NMG_CK_SHELL(s);
    BN_CK_TOL(bs->bs_tol);

    if (rt_g.NMG_debug & DEBUG_VERIFY)
	nmg_vshell(&s->r_p->s_hd, s->r_p);

    /*
     * For each face in the shell, process all the loops in the face,
     * and then handle the face and all loops as a unit.
     */
    nmg_eval_plot(bs, nmg_eval_count++);	/* debug */
    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
    while (BU_LIST_NOT_HEAD(fu, &s->fu_hd)) {
	NMG_CK_FACEUSE(fu);
	nextfu = BU_LIST_PNEXT(faceuse, fu);

	/* Faceuse mates will be handled at same time as OT_SAME fu */
	if (fu->orientation != OT_SAME) {
	    fu = nextfu;
	    continue;
	}
	if (fu->fumate_p == nextfu)
	    nextfu = BU_LIST_PNEXT(faceuse, nextfu);

	/* Consider this face */
	NMG_CK_FACE(fu->f_p);

	loops_retained = 0;
	lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	while (BU_LIST_NOT_HEAD(lu, &fu->lu_hd)) {
	    NMG_CK_LOOPUSE(lu);
	    nextlu = BU_LIST_PNEXT(loopuse, lu);
	    if (lu->lumate_p == nextlu)
		nextlu = BU_LIST_PNEXT(loopuse, nextlu);

	    NMG_CK_LOOP(lu->l_p);
	    nmg_ck_lu_orientation(lu, bs->bs_tol);
	    switch (nmg_eval_action(&lu->l_p->magic, bs)) {
		case BACTION_KILL:
		    /* Kill by demoting loop to edges */
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
			/* loop of single vertex */
			(void)nmg_klu(lu);
		    } else if (nmg_demote_lu(lu) == 0) {
			nmg_eval_plot(bs, nmg_eval_count++);	/* debug */
		    }
		    lu = nextlu;
		    continue;
		case BACTION_RETAIN:
		    loops_retained++;
		    break;
		default:
		    bu_bomb("nmg_eval_shell() bad BACTION\n");
	    }
	    lu = nextlu;
	}

	if (rt_g.NMG_debug & DEBUG_BOOLEVAL)
	    bu_log("faceuse x%x loops retained=%d\n",
		   fu, loops_retained);
	if (rt_g.NMG_debug & DEBUG_VERIFY)
	    nmg_vshell(&s->r_p->s_hd, s->r_p);

	/*
	 * Here, faceuse will have 0 or more loopuses still in it.
	 * Decide the fate of the face;  if the face dies,
	 * then any remaining loops, edges, etc, will die too.
	 */
	if (BU_LIST_IS_EMPTY(&fu->lu_hd)) {
	    if (loops_retained) bu_bomb("nmg_eval_shell() empty faceuse with retained loops?\n");
	    /* faceuse is empty, face & mate die */
	    if (rt_g.NMG_debug & DEBUG_BOOLEVAL)
		bu_log("faceuse x%x empty, kill\n", fu);
	    nmg_kfu(fu);	/* kill face & mate, dequeue from shell */
	    if (rt_g.NMG_debug & DEBUG_VERIFY)
		nmg_vshell(&s->r_p->s_hd, s->r_p);
	    nmg_eval_plot(bs, nmg_eval_count++);	/* debug */
	    fu = nextfu;
	    continue;
	}

	if (loops_retained <= 0) {
	    nmg_pr_fu(fu, (char *)NULL);
	    bu_bomb("nmg_eval_shell() non-empty faceuse, no loops retained?\n");
	}
	fu = nextfu;
    }
    if (rt_g.NMG_debug & DEBUG_VERIFY)
	nmg_vshell(&s->r_p->s_hd, s->r_p);

    /*
     * For each loop in the shell, process.
     * Each loop is either a wire-loop, or a vertex-with-self-loop.
     * Only consider wire loops here.
     */
    nmg_eval_plot(bs, nmg_eval_count++);	/* debug */
    lu = BU_LIST_FIRST(loopuse, &s->lu_hd);
    while (BU_LIST_NOT_HEAD(lu, &s->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	nextlu = BU_LIST_PNEXT(loopuse, lu);
	if (lu->lumate_p == nextlu)
	    nextlu = BU_LIST_PNEXT(loopuse, nextlu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	    /* ignore vertex-with-self-loop */
	    lu = nextlu;
	    continue;
	}
	NMG_CK_LOOP(lu->l_p);
	switch (nmg_eval_action(&lu->l_p->magic, bs)) {
	    case BACTION_KILL:
		/* Demote the loopuse into wire edges */
		/* kill loop & mate */
		if (nmg_demote_lu(lu) == 0)
		    nmg_eval_plot(bs, nmg_eval_count++);	/* debug */
		lu = nextlu;
		continue;
	    case BACTION_RETAIN:
		break;
	    default:
		bu_bomb("nmg_eval_shell() bad BACTION\n");
	}
	lu = nextlu;
    }
    if (rt_g.NMG_debug & DEBUG_VERIFY)
	nmg_vshell(&s->r_p->s_hd, s->r_p);

    /*
     * For each wire-edge in the shell, ...
     */
    nmg_eval_plot(bs, nmg_eval_count++);	/* debug */
    eu = BU_LIST_FIRST(edgeuse, &s->eu_hd);
    while (BU_LIST_NOT_HEAD(eu, &s->eu_hd)) {
	NMG_CK_EDGEUSE(eu);
	nexteu = BU_LIST_PNEXT(edgeuse, eu);	/* may be head */
	if (eu->eumate_p == nexteu)
	    nexteu = BU_LIST_PNEXT(edgeuse, nexteu);

	/* Consider this edge */
	NMG_CK_EDGE(eu->e_p);
	switch (nmg_eval_action(&eu->e_p->magic, bs)) {
	    case BACTION_KILL:
		/* Demote the edegeuse (and mate) into vertices */
		if (nmg_demote_eu(eu) == 0)
		    nmg_eval_plot(bs, nmg_eval_count++);	/* debug */
		eu = nexteu;
		continue;
	    case BACTION_RETAIN:
		break;
	    default:
		bu_bomb("nmg_eval_shell() bad BACTION\n");
	}
	eu = nexteu;
    }

    /*
     * For each lone vertex-with-self-loop, process.
     * Note that these are intermixed in the loop list.
     * Each loop is either a wire-loop, or a vertex-with-self-loop.
     * Only consider cases of vertex-with-self-loop here.
     *
     * This case has to be handled separately, because a wire-loop
     * may be demoted to a set of wire-edges above, some of which
     * may be retained.  The non-retained wire-edges may in turn
     * be demoted into vertex-with-self-loop objects above,
     * which will be processed here.
     */
    nmg_eval_plot(bs, nmg_eval_count++);	/* debug */
    lu = BU_LIST_FIRST(loopuse, &s->lu_hd);
    while (BU_LIST_NOT_HEAD(lu, &s->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	nextlu = BU_LIST_PNEXT(loopuse, lu);

	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_VERTEXUSE_MAGIC) {
	    /* ignore any remaining wire-loops */
	    lu = nextlu;
	    continue;
	}
	if (nextlu == lu->lumate_p)
	    nextlu = BU_LIST_PNEXT(loopuse, nextlu);
	vu = BU_LIST_PNEXT(vertexuse, &lu->down_hd);
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_VERTEX(vu->v_p);
	switch (nmg_eval_action(&vu->v_p->magic, bs)) {
	    case BACTION_KILL:
		/* Eliminate the loopuse, and mate */
		nmg_klu(lu);
		lu = nextlu;
		continue;
	    case BACTION_RETAIN:
		break;
	    default:
		bu_bomb("nmg_eval_shell() bad BACTION\n");
	}
	lu = nextlu;
    }
    if (rt_g.NMG_debug & DEBUG_VERIFY)
	nmg_vshell(&s->r_p->s_hd, s->r_p);

    /*
     * Final case:  shell of a single vertexuse
     */
    vu = s->vu_p;
    if (vu) {
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_VERTEX(vu->v_p);
	switch (nmg_eval_action(&vu->v_p->magic, bs)) {
	    case BACTION_KILL:
		nmg_kvu(vu);
		nmg_eval_plot(bs, nmg_eval_count++);	/* debug */
		s->vu_p = (struct vertexuse *)0;	/* sanity */
		break;
	    case BACTION_RETAIN:
		break;
	    default:
		bu_bomb("nmg_eval_shell() bad BACTION\n");
	}
    }
    if (rt_g.NMG_debug & DEBUG_VERIFY)
	nmg_vshell(&s->r_p->s_hd, s->r_p);
    nmg_eval_plot(bs, nmg_eval_count++);	/* debug */
}


/**
 * N M G _ E V A L _ P L O T
 *
 * Called from nmg_eval_shell
 *
 * Located here because definition of nmg_bool_state is local to this
 * module.
 */
HIDDEN void
nmg_eval_plot(struct nmg_bool_state *bs, int num)
{
    FILE *fp;
    char fname[128];
    int do_plot = 0;
    int do_anim = 0;

    if (rt_g.NMG_debug & DEBUG_BOOLEVAL && rt_g.NMG_debug & DEBUG_PLOTEM)
	do_plot = 1;
    if (rt_g.NMG_debug & DEBUG_PL_ANIM) do_anim = 1;

    if (!do_plot && !do_anim) return;

    BN_CK_TOL(bs->bs_tol);

    if (do_plot) {
	sprintf(fname, "nmg_eval%d.pl", num);
	if ((fp = fopen(fname, "wb")) == NULL) {
	    perror(fname);
	    return;
	}
	bu_log("Plotting %s\n", fname);

	nmg_pl_s(fp, bs->bs_dest);
	nmg_pl_s(fp, bs->bs_src);

	fclose(fp);
    }

    if (do_anim) {
	struct bn_vlblock *vbp;

	vbp = rt_vlblock_init();

	nmg_vlblock_s(vbp, bs->bs_dest, 0);
	nmg_vlblock_s(vbp, bs->bs_src, 0);

	/* Cause animation of boolean operation as it proceeds! */
	if (nmg_vlblock_anim_upcall) {
	    /* if requested, delay 1/4 second */
	    (*nmg_vlblock_anim_upcall)(vbp,
				       (rt_g.NMG_debug&DEBUG_PL_SLOW) ? 250000 : 0,
				       0);
	} else {
	    bu_log("null nmg_vlblock_anim_upcall, no animation\n");
	}
	rt_vlblock_free(vbp);
    }
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
