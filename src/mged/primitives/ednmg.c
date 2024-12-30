/*                        E D N M G . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/primitives/ednmg.c
 *
 * Attempt to collect all the MGED logic specific to dealing with
 * NMG primitives here.
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "ged/view/ged_view_tmp.h"
#include "../mged.h"
#include "../sedit.h"
#include "./ednmg.h"

extern int es_edflag;		/* type of editing for this solid */
extern int es_mvalid;           /* es_mparam valid.  inpara must = 0 */
extern vect_t es_mparam;        /* mouse input param.  Only when es_mvalid set */
extern vect_t es_para; /* keyboard input param. Only when inpara set.  */
extern int inpara;              /* es_para valid.  es_mvalid must = 0 */
extern mat_t es_invmat;         /* inverse of es_mat KAA */

struct edgeuse *es_eu=(struct edgeuse *)NULL;   /* Currently selected NMG edgeuse */
struct loopuse *lu_copy=(struct loopuse*)NULL;  /* copy of loop to be extruded */
plane_t lu_pl;  /* plane equation for loop to be extruded */
struct shell *es_s=(struct shell *)NULL;        /* Shell where extrusion is to end up */
point_t lu_keypoint;    /* keypoint of lu_copy for extrusion */

/*
 * Handler for events in the NMG menu.
 * Mostly just set appropriate state flags to prepare us for user's
 * next event.
 */
/*ARGSUSED*/
static void
nmg_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    switch (arg) {
	default:
	    Tcl_AppendResult(s->interp, "nmg_ed: undefined menu event?\n", (char *)NULL);
	    return;
	case ECMD_NMG_EPICK:
	case ECMD_NMG_EMOVE:
	case ECMD_NMG_ESPLIT:
	case ECMD_NMG_EKILL:
	    break;
	case ECMD_NMG_EDEBUG:
	    if (!es_eu) {
		Tcl_AppendResult(s->interp, "nmg_ed: no edge selected yet\n", (char *)NULL);
		return;
	    }

	    nmg_pr_fu_around_eu(es_eu, &s->tol.tol);

	    nmg_plot_eu(s->gedp, es_eu, &s->tol.tol, s->vlfree);

	    if (*es_eu->up.magic_p == NMG_LOOPUSE_MAGIC)
		nmg_veu(&es_eu->up.lu_p->down_hd, es_eu->up.magic_p);
	    /* no change of state or es_edflag */
	    view_state->vs_flag = 1;
	    return;
	case ECMD_NMG_FORW:
	    if (!es_eu) {
		Tcl_AppendResult(s->interp, "nmg_ed: no edge selected yet\n", (char *)NULL);
		return;
	    }
	    NMG_CK_EDGEUSE(es_eu);
	    es_eu = BU_LIST_PNEXT_CIRC(edgeuse, es_eu);

	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		bu_vls_printf(&tmp_vls, "edgeuse selected = %p (%g %g %g) <-> (%g %g %g)\n",
			      (void *)es_eu, V3ARGS(es_eu->vu_p->v_p->vg_p->coord),
			      V3ARGS(es_eu->eumate_p->vu_p->v_p->vg_p->coord));
		Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
	    }

	    sedit(s);
	    return;
	case ECMD_NMG_BACK:
	    if (!es_eu) {
		bu_log("nmg_ed: no edge selected yet\n");
		return;
	    }
	    NMG_CK_EDGEUSE(es_eu);
	    es_eu = BU_LIST_PPREV_CIRC(edgeuse, es_eu);

	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		bu_vls_printf(&tmp_vls, "edgeuse selected = %p (%g %g %g) <-> (%g %g %g)\n",
			      (void *)es_eu, V3ARGS(es_eu->vu_p->v_p->vg_p->coord),
			      V3ARGS(es_eu->eumate_p->vu_p->v_p->vg_p->coord));
		Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
	    }

	    sedit(s);
	    return;
	case ECMD_NMG_RADIAL:
	    if (!es_eu) {
		bu_log("nmg_ed: no edge selected yet\n");
		return;
	    }
	    NMG_CK_EDGEUSE(es_eu);
	    es_eu = es_eu->eumate_p->radial_p;

	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		bu_vls_printf(&tmp_vls, "edgeuse selected = %p (%g %g %g) <-> (%g %g %g)\n",
			      (void *)es_eu, V3ARGS(es_eu->vu_p->v_p->vg_p->coord),
			      V3ARGS(es_eu->eumate_p->vu_p->v_p->vg_p->coord));
		Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
	    }

	    sedit(s);
	    return;
	case ECMD_NMG_LEXTRU:
	    {
		struct model *m, *m_tmp;
		struct nmgregion *r, *r_tmp;
		struct shell *sh, *s_tmp;
		struct loopuse *lu=(struct loopuse *)NULL;
		struct loopuse *lu_tmp;
		struct edgeuse *eu;
		fastf_t area;
		int wire_loop_count = 0;

		m = (struct model *)s->edit_state.es_int.idb_ptr;
		NMG_CK_MODEL(m);

		/* look for wire loops */
		for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
		    NMG_CK_REGION(r);
		    for (BU_LIST_FOR(sh, shell, &r->s_hd)) {
			if (BU_LIST_IS_EMPTY(&sh->lu_hd))
			    continue;

			for (BU_LIST_FOR(lu_tmp, loopuse, &sh->lu_hd)) {
			    if (!lu)
				lu = lu_tmp;
			    else if (lu_tmp == lu->lumate_p)
				continue;

			    wire_loop_count++;
			}
		    }
		}

		if (!wire_loop_count) {
		    Tcl_AppendResult(s->interp, "No sketch (wire loop) to extrude\n",
				     (char *)NULL);
		    return;
		}

		if (wire_loop_count > 1) {
		    Tcl_AppendResult(s->interp, "Too many wire loops!  Don't know which to extrude!\n", (char *)NULL);
		    return;
		}

		if (!lu || *lu->up.magic_p != NMG_SHELL_MAGIC) {
		    /* This should never happen */
		    bu_exit(EXIT_FAILURE,  "Cannot find wire loop!\n");
		}

		/* Make sure loop is not a crack */
		area = nmg_loop_plane_area(lu, lu_pl);

		if (area < 0.0) {
		    Tcl_AppendResult(s->interp, "Cannot extrude loop with no area\n",
				     (char *)NULL);
		    return;
		}

		/* Check if loop crosses itself */
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    struct edgeuse *eu2;
		    struct vertex *v1;
		    vect_t edge1;

		    NMG_CK_EDGEUSE(eu);

		    v1 = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v1);
		    VSUB2(edge1, eu->eumate_p->vu_p->v_p->vg_p->coord, v1->vg_p->coord);

		    for (eu2 = BU_LIST_PNEXT(edgeuse, &eu->l); BU_LIST_NOT_HEAD(eu2, &lu->down_hd); eu2=BU_LIST_PNEXT(edgeuse, &eu2->l)) {
			struct vertex *v2;
			vect_t edge2;
			fastf_t dist[2];
			int ret_val;

			NMG_CK_EDGEUSE(eu2);

			if (eu2 == eu)
			    continue;
			if (eu2 == BU_LIST_PNEXT_CIRC(edgeuse,  &eu->l))
			    continue;
			if (eu2 == BU_LIST_PPREV_CIRC(edgeuse, &eu->l))
			    continue;

			v2 = eu2->vu_p->v_p;
			NMG_CK_VERTEX(v2);
			VSUB2(edge2, eu2->eumate_p->vu_p->v_p->vg_p->coord, v2->vg_p->coord);

			if ((ret_val = bg_isect_lseg3_lseg3(dist, v1->vg_p->coord, edge1,
							    v2->vg_p->coord, edge2, &s->tol.tol)) > (-1))
			{
			    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

			    bu_vls_printf(&tmp_vls,
					  "Loop crosses itself, cannot extrude\n");
			    bu_vls_printf(&tmp_vls,
					  "edge1: pt=(%g %g %g), dir=(%g %g %g)\n",
					  V3ARGS(v1->vg_p->coord), V3ARGS(edge1));
			    bu_vls_printf(&tmp_vls,
					  "edge2: pt=(%g %g %g), dir=(%g %g %g)\n",
					  V3ARGS(v2->vg_p->coord), V3ARGS(edge2));
			    if (ret_val == 0) {
				bu_vls_printf(&tmp_vls,
					      "edges are collinear and overlap\n");
			    } else {
				point_t isect_pt;

				VJOIN1(isect_pt, v1->vg_p->coord, dist[0], edge1);
				bu_vls_printf(&tmp_vls,
					      "edges intersect at (%g %g %g)\n",
					      V3ARGS(isect_pt));
			    }

			    Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls),
					     (char *)NULL);
			    bu_vls_free(&tmp_vls);
			    return;
			}
		    }
		}

		/* Create a temporary model to store the basis loop */
		m_tmp = nmg_mm();
		r_tmp = nmg_mrsv(m_tmp);
		s_tmp = BU_LIST_FIRST(shell, &r_tmp->s_hd);
		lu_copy = nmg_dup_loop(lu, &s_tmp->l.magic, (long **)0);
		if (!lu_copy) {
		    Tcl_AppendResult(s->interp, "Failed to make copy of loop\n", (char *)NULL);
		    nmg_km(m_tmp);
		    return;
		}

		/* Get the first vertex in the loop as the basis for extrusion */
		eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
		VMOVE(lu_keypoint, eu->vu_p->v_p->vg_p->coord);

		sh = lu->up.s_p;

		if (BU_LIST_NON_EMPTY(&sh->fu_hd)) {
		    /* make a new shell to hold the extruded solid */

		    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
		    NMG_CK_REGION(r);
		    es_s = nmg_msv(r);
		} else {
		    es_s = sh;
		}

	    }
	    break;
    }
    /* For example, this will set es_edflag = ECMD_NMG_EPICK */
    es_edflag = arg;
    sedit(s);
}
struct menu_item nmg_menu[] = {
    { "NMG MENU", NULL, 0 },
    { "Pick Edge", nmg_ed, ECMD_NMG_EPICK },
    { "Move Edge", nmg_ed, ECMD_NMG_EMOVE },
    { "Split Edge", nmg_ed, ECMD_NMG_ESPLIT },
    { "Delete Edge", nmg_ed, ECMD_NMG_EKILL },
    { "Next EU", nmg_ed, ECMD_NMG_FORW },
    { "Prev EU", nmg_ed, ECMD_NMG_BACK },
    { "Radial EU", nmg_ed, ECMD_NMG_RADIAL },
    { "Extrude Loop", nmg_ed, ECMD_NMG_LEXTRU },
    { "Eebug Edge", nmg_ed, ECMD_NMG_EDEBUG },
    { "", NULL, 0 }
};



const char *
mged_nmg_keypoint(
	point_t *pt,
	const char *UNUSED(keystr),
	const mat_t mat,
	const struct rt_db_internal *ip,
	const struct bn_tol *UNUSED(tol))
{
    const char *strp = NULL;
    point_t mpt = VINIT_ZERO;
    if (ip->idb_type != ID_NMG)
	return strp;

    struct vertex *v;
    struct vertexuse *vu;
    struct edgeuse *eu;
    struct loopuse *lu;
    struct faceuse *fu;
    struct shell *nmg_s;
    struct nmgregion *r;
    struct model *m = (struct model *) ip->idb_ptr;
    NMG_CK_MODEL(m);
    /* XXX Fall through, for now (How about first vertex?? - JRA) */

    /* set default first */
    VSETALL(mpt, 0.0);
    strp = "(origin)";

    /* XXX Try to use the first point of the selected edge */
    if (es_eu != (struct edgeuse *)NULL &&
	    es_eu->vu_p != (struct vertexuse *)NULL &&
	    es_eu->vu_p->v_p != (struct vertex *)NULL &&
	    es_eu->vu_p->v_p->vg_p != (struct vertex_g *)NULL) {
	VMOVE(mpt, es_eu->vu_p->v_p->vg_p->coord);
	goto nmg_keypoint_finalize;
    }

    if (BU_LIST_IS_EMPTY(&m->r_hd))
	goto nmg_keypoint_finalize;

    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    if (!r)
	goto nmg_keypoint_finalize;
    NMG_CK_REGION(r);

    if (BU_LIST_IS_EMPTY(&r->s_hd))
	goto nmg_keypoint_finalize;

    nmg_s = BU_LIST_FIRST(shell, &r->s_hd);
    if (!nmg_s)
	goto nmg_keypoint_finalize;
    NMG_CK_SHELL(nmg_s);

    if (BU_LIST_IS_EMPTY(&nmg_s->fu_hd))
	fu = (struct faceuse *)NULL;
    else
	fu = BU_LIST_FIRST(faceuse, &nmg_s->fu_hd);
    if (fu) {
	NMG_CK_FACEUSE(fu);
	lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	NMG_CK_LOOPUSE(lu);
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
	    eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	    NMG_CK_EDGEUSE(eu);
	    NMG_CK_VERTEXUSE(eu->vu_p);
	    v = eu->vu_p->v_p;
	} else {
	    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	    NMG_CK_VERTEXUSE(vu);
	    v = vu->v_p;
	}
	NMG_CK_VERTEX(v);
	if (!v->vg_p)
	    goto nmg_keypoint_finalize;
	VMOVE(mpt, v->vg_p->coord);
	strp = "V";
	goto nmg_keypoint_finalize;
    }
    if (BU_LIST_IS_EMPTY(&nmg_s->lu_hd))
	lu = (struct loopuse *)NULL;
    else
	lu = BU_LIST_FIRST(loopuse, &nmg_s->lu_hd);
    if (lu) {
	NMG_CK_LOOPUSE(lu);
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
	    eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	    NMG_CK_EDGEUSE(eu);
	    NMG_CK_VERTEXUSE(eu->vu_p);
	    v = eu->vu_p->v_p;
	} else {
	    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	    NMG_CK_VERTEXUSE(vu);
	    v = vu->v_p;
	}
	NMG_CK_VERTEX(v);
	if (!v->vg_p)
	    goto nmg_keypoint_finalize;
	VMOVE(mpt, v->vg_p->coord);
	strp = "V";
	goto nmg_keypoint_finalize;
    }
    if (BU_LIST_IS_EMPTY(&nmg_s->eu_hd))
	eu = (struct edgeuse *)NULL;
    else
	eu = BU_LIST_FIRST(edgeuse, &nmg_s->eu_hd);
    if (eu) {
	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(eu->vu_p);
	v = eu->vu_p->v_p;
	NMG_CK_VERTEX(v);
	if (!v->vg_p)
	    goto nmg_keypoint_finalize;
	VMOVE(mpt, v->vg_p->coord);
	strp = "V";
	goto nmg_keypoint_finalize;
    }
    vu = nmg_s->vu_p;
    if (vu) {
	NMG_CK_VERTEXUSE(vu);
	v = vu->v_p;
	NMG_CK_VERTEX(v);
	if (!v->vg_p)
	    goto nmg_keypoint_finalize;
	VMOVE(mpt, v->vg_p->coord);
	strp = "V";
	goto nmg_keypoint_finalize;
    }

nmg_keypoint_finalize:
    MAT4X3PNT(*pt, mat, mpt);
    return strp;
}


void
mged_nmg_labels(
	int *UNUSED(num_lines),
	point_t *UNUSED(lines),
	struct rt_point_labels *pl,
	int UNUSED(max_pl),
	const mat_t xform,
	struct rt_db_internal *ip,
	struct bn_tol *UNUSED(tol))
{
    point_t pos_view;
    int npl = 0;
#define POINT_LABEL_STR(_pt, _str) { \
    VMOVE(pl[npl].pt, _pt); \
    bu_strlcpy(pl[npl++].str, _str, sizeof(pl[0].str)); }
    struct model *m = (struct model *) ip->idb_ptr;
    NMG_CK_MODEL(m);
    // Conditional labeling
    if (es_eu) {
	point_t cent;
	NMG_CK_EDGEUSE(es_eu);
	VADD2SCALE(cent,
		es_eu->vu_p->v_p->vg_p->coord,
		es_eu->eumate_p->vu_p->v_p->vg_p->coord,
		0.5);
	MAT4X3PNT(pos_view, xform, cent);
	POINT_LABEL_STR(pos_view, " eu");
    }

    pl[npl].str[0] = '\0';	/* Mark ending */
}


void ecmd_nmg_emove(struct mged_state *s)
{
    point_t new_pt;

    if (!es_eu) {
	Tcl_AppendResult(s->interp, "No edge selected!\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    }
    NMG_CK_EDGEUSE(es_eu);

    if (es_mvalid) {
	VMOVE(new_pt, es_mparam);
    } else if (inpara == 3) {
	if (mged_variables->mv_context) {
	    /* apply es_invmat to convert to real model space */
	    MAT4X3PNT(new_pt, es_invmat, es_para);
	} else {
	    VMOVE(new_pt, es_para);
	}
    } else if (inpara && inpara != 3) {
	Tcl_AppendResult(s->interp, "x y z coordinates required for edge move\n",
		(char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    } else if (!es_mvalid && !inpara)
	return;

    if (!nmg_find_fu_of_eu(es_eu) && *es_eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
	struct loopuse *lu;
	fastf_t area;
	plane_t pl;

	/* this edge is in a wire loop
	 * keep the loop planar
	 */
	lu = es_eu->up.lu_p;
	NMG_CK_LOOPUSE(lu);

	/* get plane equation for loop */
	area = nmg_loop_plane_area(lu, pl);
	if (area > 0.0) {
	    vect_t view_z_dir;
	    vect_t view_dir;
	    fastf_t dist;

	    /* Get view direction vector */
	    VSET(view_z_dir, 0.0, 0.0, 1.0);
	    MAT4X3VEC(view_dir, view_state->vs_gvp->gv_view2model, view_z_dir);

	    /* intersect line through new_pt with plane of loop */
	    if (bg_isect_line3_plane(&dist, new_pt, view_dir, pl, &s->tol.tol) < 1) {
		/* line does not intersect plane, don't do an esplit */
		Tcl_AppendResult(s->interp, "Edge Move: Cannot place new point in plane of loop\n", (char *)NULL);
		mged_print_result(s, TCL_ERROR);
		return;
	    }
	    VJOIN1(new_pt, new_pt, dist, view_dir);
	}
    }

    if (nmg_move_edge_thru_pnt(es_eu, new_pt, &s->tol.tol) < 0) {
	VPRINT("Unable to hit", new_pt);
    }
}

void ecmd_nmg_ekill(struct mged_state *s)
{
    struct model *m;
    struct edge_g_lseg *eg;

    if (!es_eu) {
	Tcl_AppendResult(s->interp, "No edge selected!\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    }
    NMG_CK_EDGEUSE(es_eu);

    m = nmg_find_model(&es_eu->l.magic);

    if (*es_eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
	struct loopuse *lu;
	struct edgeuse *prev_eu, *next_eu;

	lu = es_eu->up.lu_p;
	NMG_CK_LOOPUSE(lu);

	if (*lu->up.magic_p != NMG_SHELL_MAGIC) {
	    /* Currently can only kill wire edges or edges in wire loops */
	    Tcl_AppendResult(s->interp, "Currently, we can only kill wire edges or edges in wire loops\n", (char *)NULL);
	    mged_print_result(s, TCL_ERROR);
	    es_edflag = IDLE;
	    return;
	}

	prev_eu = BU_LIST_PPREV_CIRC(edgeuse, &es_eu->l);
	NMG_CK_EDGEUSE(prev_eu);

	if (prev_eu == es_eu) {
	    /* only one edge left in the loop
	     * make it an edge to/from same vertex
	     */
	    if (es_eu->vu_p->v_p == es_eu->eumate_p->vu_p->v_p) {
		/* refuse to delete last edge that runs
		 * to/from same vertex
		 */
		Tcl_AppendResult(s->interp, "Cannot delete last edge running to/from same vertex\n", (char *)NULL);
		mged_print_result(s, TCL_ERROR);
		return;
	    }
	    NMG_CK_EDGEUSE(es_eu->eumate_p);
	    nmg_movevu(es_eu->eumate_p->vu_p, es_eu->vu_p->v_p);
	    return;
	}

	next_eu = BU_LIST_PNEXT_CIRC(edgeuse, &es_eu->l);
	NMG_CK_EDGEUSE(next_eu);

	nmg_movevu(next_eu->vu_p, es_eu->vu_p->v_p);
	if (nmg_keu(es_eu)) {
	    /* Should never happen! */
	    bu_exit(EXIT_FAILURE,  "sedit(s): killed edge and emptied loop!\n");
	}
	es_eu = prev_eu;
	nmg_rebound(m, &s->tol.tol);

	/* fix edge geometry for modified edge (next_eu) */
	eg = next_eu->g.lseg_p;
	NMG_CK_EDGE_G_LSEG(eg);
	VMOVE(eg->e_pt, next_eu->vu_p->v_p->vg_p->coord);
	VSUB2(eg->e_dir, next_eu->eumate_p->vu_p->v_p->vg_p->coord, next_eu->vu_p->v_p->vg_p->coord);

	return;
    } else if (*es_eu->up.magic_p == NMG_SHELL_MAGIC) {
	/* wire edge, just kill it */
	(void)nmg_keu(es_eu);
	es_eu = (struct edgeuse *)NULL;
	nmg_rebound(m, &s->tol.tol);
    }
}

void ecmd_nmg_esplit(struct mged_state *s)
{
    struct vertex *v=(struct vertex *)NULL;
    struct edge_g_lseg *eg;
    struct model *m;
    point_t new_pt;
    fastf_t area;
    plane_t pl;

    if (!es_eu) {
	Tcl_AppendResult(s->interp, "No edge selected!\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    }
    NMG_CK_EDGEUSE(es_eu);
    m = nmg_find_model(&es_eu->l.magic);
    NMG_CK_MODEL(m);
    if (es_mvalid) {
	VMOVE(new_pt, es_mparam);
    } else if (inpara == 3) {
	if (mged_variables->mv_context) {
	    /* apply es_invmat to convert to real model space */
	    MAT4X3PNT(new_pt, es_invmat, es_para);
	} else {
	    VMOVE(new_pt, es_para);
	}
    } else if (inpara && inpara != 3) {
	Tcl_AppendResult(s->interp, "x y z coordinates required for edge split\n",
		(char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    } else if (!es_mvalid && !inpara)
	return;

    if (*es_eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
	struct loopuse *lu;

	lu = es_eu->up.lu_p;
	NMG_CK_LOOPUSE(lu);

	/* Currently, can only split wire edges or edges in wire loops */
	if (*lu->up.magic_p != NMG_SHELL_MAGIC) {
	    Tcl_AppendResult(s->interp, "Currently, we can only split wire edges or edges in wire loops\n", (char *)NULL);
	    es_edflag = IDLE;
	    mged_print_result(s, TCL_ERROR);
	    return;
	}

	/* get plane equation for loop */
	area = nmg_loop_plane_area(lu, pl);
	if (area > 0.0) {
	    vect_t view_z_dir;
	    vect_t view_dir;
	    fastf_t dist;

	    /* Get view direction vector */
	    VSET(view_z_dir, 0.0, 0.0, 1.0);
	    MAT4X3VEC(view_dir, view_state->vs_gvp->gv_view2model, view_z_dir);

	    /* intersect line through new_pt with plane of loop */
	    if (bg_isect_line3_plane(&dist, new_pt, view_dir, pl, &s->tol.tol) < 1) {
		/* line does not intersect plane, don't do an esplit */
		Tcl_AppendResult(s->interp, "Edge Split: Cannot place new point in plane of loop\n", (char *)NULL);
		mged_print_result(s, TCL_ERROR);
		return;
	    }
	    VJOIN1(new_pt, new_pt, dist, view_dir);
	}
    }
    es_eu = nmg_esplit(v, es_eu, 0);
    nmg_vertex_gv(es_eu->vu_p->v_p, new_pt);
    nmg_rebound(m, &s->tol.tol);
    eg = es_eu->g.lseg_p;
    NMG_CK_EDGE_G_LSEG(eg);
    VMOVE(eg->e_pt, new_pt);
    VSUB2(eg->e_dir, es_eu->eumate_p->vu_p->v_p->vg_p->coord, new_pt);
}

void ecmd_nmg_lextru(struct mged_state *s)
{
    fastf_t dist;
    point_t to_pt;
    vect_t extrude_vec;
    struct loopuse *new_lu;
    struct faceuse *fu;
    struct model *m;
    plane_t new_lu_pl;
    fastf_t area;

    if (es_mvalid) {
	VMOVE(to_pt, es_mparam);
    } else if (inpara == 3) {
	if (mged_variables->mv_context) {
	    /* apply es_invmat to convert to real model space */
	    MAT4X3PNT(to_pt, es_invmat, es_para);
	} else {
	    VMOVE(to_pt, es_para);
	}
    } else if (inpara == 1) {
	VJOIN1(to_pt, lu_keypoint, es_para[0], lu_pl);
    } else if (inpara && inpara != 3) {
	Tcl_AppendResult(s->interp, "x y z coordinates required for loop extrusion\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    } else if (!es_mvalid && !inpara) {
	return;
    }

    VSUB2(extrude_vec, to_pt, lu_keypoint);

    if (bg_isect_line3_plane(&dist, to_pt, extrude_vec, lu_pl, &s->tol.tol) < 1) {
	Tcl_AppendResult(s->interp, "Cannot extrude parallel to plane of loop\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    }

    if (BU_LIST_NON_EMPTY(&es_s->fu_hd)) {
	struct nmgregion *r;

	r = es_s->r_p;
	(void) nmg_ks(es_s);
	es_s = nmg_msv(r);
    }

    new_lu = nmg_dup_loop(lu_copy, &es_s->l.magic, (long **)0);
    area = nmg_loop_plane_area(new_lu, new_lu_pl);
    if (area < 0.0) {
	Tcl_AppendResult(s->interp, "loop to be extruded as no area!\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    }

    if (VDOT(extrude_vec, new_lu_pl) > 0.0) {
	plane_t tmp_pl;

	fu = nmg_mf(new_lu->lumate_p);
	NMG_CK_FACEUSE(fu);
	HREVERSE(tmp_pl, new_lu_pl);
	nmg_face_g(fu, tmp_pl);
    } else {
	fu = nmg_mf(new_lu);
	NMG_CK_FACEUSE(fu);
	nmg_face_g(fu, new_lu_pl);
    }

    (void)nmg_extrude_face(fu, extrude_vec, s->vlfree, &s->tol.tol);

    nmg_fix_normals(fu->s_p, s->vlfree, &s->tol.tol);

    m = nmg_find_model(&fu->l.magic);
    nmg_rebound(m, &s->tol.tol);
    (void)nmg_ck_geometry(m, s->vlfree, &s->tol.tol);

    es_eu = (struct edgeuse *)NULL;

    replot_editing_solid(s);
    view_state->vs_flag = 1;
}


/* XXX Should just leave desired location in es_mparam for sedit(s) */
void ecmd_nmg_epick(struct mged_state *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */

    struct model *m =
	(struct model *)s->edit_state.es_int.idb_ptr;
    struct edge *e;
    struct bn_tol tmp_tol;
    NMG_CK_MODEL(m);

    /* Picking an edge should not depend on tolerances! */
    tmp_tol.magic = BN_TOL_MAGIC;
    tmp_tol.dist = 0.0;
    tmp_tol.dist_sq = tmp_tol.dist * tmp_tol.dist;
    tmp_tol.perp = 0.0;
    tmp_tol.para = 1 - tmp_tol.perp;

    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    if ((e = nmg_find_e_nearest_pt2(&m->magic, pos_view,
		    view_state->vs_gvp->gv_model2view, s->vlfree, &tmp_tol)) == (struct edge *)NULL) {
	Tcl_AppendResult(s->interp, "ECMD_NMG_EPICK: unable to find an edge\n",
		(char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    }
    es_eu = e->eu_p;
    NMG_CK_EDGEUSE(es_eu);

    {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls,
		"edgeuse selected = %p (%g %g %g) <-> (%g %g %g)\n",
		(void *)es_eu, V3ARGS(es_eu->vu_p->v_p->vg_p->coord),
		V3ARGS(es_eu->eumate_p->vu_p->v_p->vg_p->coord));
	Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	bu_vls_free(&tmp_vls);
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
