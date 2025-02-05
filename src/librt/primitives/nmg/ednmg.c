/*                        E D N M G . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
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
/** @file primitives/ednmg.c
 *
 * Editing logic specific to dealing with NMG primitives.
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
#include "rt/primitives/nmg.h"
#include "../edit_private.h"

#define ECMD_NMG_EPICK		11019	/* edge pick */
#define ECMD_NMG_EMOVE		11020	/* edge move */
#define ECMD_NMG_EDEBUG		11021	/* edge debug */
#define ECMD_NMG_FORW		11022	/* next eu */
#define ECMD_NMG_BACK		11023	/* prev eu */
#define ECMD_NMG_RADIAL		11024	/* radial+mate eu */
#define ECMD_NMG_ESPLIT		11025	/* split current edge */
#define ECMD_NMG_EKILL		11026	/* kill current edge */
#define ECMD_NMG_LEXTRU		11027	/* Extrude loop */

void *
rt_solid_edit_nmg_prim_edit_create(struct rt_solid_edit *UNUSED(s))
{
    struct rt_nmg_edit *e;
    BU_GET(e, struct rt_nmg_edit);

    e->es_eu = NULL;
    e->lu_copy = NULL;
    e->es_s = NULL;

    return (void *)e;
}

void
rt_solid_edit_nmg_prim_edit_destroy(struct rt_nmg_edit *e)
{
    if (!e)
	return;

    // Sanity
    e->es_eu = NULL;
    if (e->lu_copy) {
	struct model *m = nmg_find_model(&e->lu_copy->l.magic);
	nmg_km(m);
	e->lu_copy = (struct loopuse *)NULL;
    }

    BU_PUT(e, struct rt_nmg_edit);
}

void
rt_solid_edit_nmg_prim_edit_reset(struct rt_solid_edit *s)
{
    struct rt_nmg_edit *n = (struct rt_nmg_edit *)s->ipe_ptr;
    n->es_eu = NULL;
    n->es_s = NULL;
}

/*
 * Handler for events in the NMG menu.
 * Mostly just set appropriate state flags to prepare us for user's
 * next event.
 */
/*ARGSUSED*/
static void
nmg_ed(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    struct rt_nmg_edit *n = (struct rt_nmg_edit *)s->ipe_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    switch (arg) {
	default:
	    bu_vls_printf(s->log_str, "nmg_ed: undefined menu event?\n");
	    return;
	case ECMD_NMG_EPICK:
	case ECMD_NMG_EMOVE:
	case ECMD_NMG_ESPLIT:
	case ECMD_NMG_EKILL:
	    break;
	case ECMD_NMG_EDEBUG:
	    if (!n->es_eu) {
		bu_vls_printf(s->log_str, "nmg_ed: no edge selected yet\n");
		return;
	    }

	    nmg_pr_fu_around_eu(n->es_eu, s->tol);

	    // plotting clbk
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_NMG_EDEBUG, 0, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, s);

	    if (*n->es_eu->up.magic_p == NMG_LOOPUSE_MAGIC)
		nmg_veu(&n->es_eu->up.lu_p->down_hd, n->es_eu->up.magic_p);
	    /* no change of state or edit_flag */
	    int vs_flag = 1;
	    f = NULL; d = NULL;
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_VIEW_SET_FLAG, 0, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, &vs_flag);
	    return;
	case ECMD_NMG_FORW:
	    if (!n->es_eu) {
		bu_vls_printf(s->log_str, "nmg_ed: no edge selected yet\n");
		return;
	    }
	    NMG_CK_EDGEUSE(n->es_eu);
	    n->es_eu = BU_LIST_PNEXT_CIRC(edgeuse, n->es_eu);

	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		bu_vls_printf(&tmp_vls, "edgeuse selected = %p (%g %g %g) <-> (%g %g %g)\n",
			      (void *)n->es_eu, V3ARGS(n->es_eu->vu_p->v_p->vg_p->coord),
			      V3ARGS(n->es_eu->eumate_p->vu_p->v_p->vg_p->coord));
		bu_vls_printf(s->log_str, "%s", bu_vls_cstr(&tmp_vls));
		bu_vls_free(&tmp_vls);
	    }

	    rt_solid_edit_process(s);
	    return;
	case ECMD_NMG_BACK:
	    if (!n->es_eu) {
		bu_log("nmg_ed: no edge selected yet\n");
		return;
	    }
	    NMG_CK_EDGEUSE(n->es_eu);
	    n->es_eu = BU_LIST_PPREV_CIRC(edgeuse, n->es_eu);

	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		bu_vls_printf(&tmp_vls, "edgeuse selected = %p (%g %g %g) <-> (%g %g %g)\n",
			      (void *)n->es_eu, V3ARGS(n->es_eu->vu_p->v_p->vg_p->coord),
			      V3ARGS(n->es_eu->eumate_p->vu_p->v_p->vg_p->coord));
		bu_vls_printf(s->log_str, "%s", bu_vls_cstr(&tmp_vls));
		bu_vls_free(&tmp_vls);
	    }

	    rt_solid_edit_process(s);
	    return;
	case ECMD_NMG_RADIAL:
	    if (!n->es_eu) {
		bu_log("nmg_ed: no edge selected yet\n");
		return;
	    }
	    NMG_CK_EDGEUSE(n->es_eu);
	    n->es_eu = n->es_eu->eumate_p->radial_p;

	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		bu_vls_printf(&tmp_vls, "edgeuse selected = %p (%g %g %g) <-> (%g %g %g)\n",
			      (void *)n->es_eu, V3ARGS(n->es_eu->vu_p->v_p->vg_p->coord),
			      V3ARGS(n->es_eu->eumate_p->vu_p->v_p->vg_p->coord));
		bu_vls_printf(s->log_str, "%s", bu_vls_cstr(&tmp_vls));
		bu_vls_free(&tmp_vls);
	    }

	    rt_solid_edit_process(s);
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

		m = (struct model *)s->es_int.idb_ptr;
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
		    bu_vls_printf(s->log_str, "No sketch (wire loop) to extrude\n");
		    return;
		}

		if (wire_loop_count > 1) {
		    bu_vls_printf(s->log_str, "Too many wire loops!  Don't know which to extrude!\n");
		    return;
		}

		if (!lu || *lu->up.magic_p != NMG_SHELL_MAGIC) {
		    /* This should never happen */
		    bu_exit(EXIT_FAILURE,  "Cannot find wire loop!\n");
		}

		/* Make sure loop is not a crack */
		area = nmg_loop_plane_area(lu, n->lu_pl);

		if (area < 0.0) {
		    bu_vls_printf(s->log_str, "Cannot extrude loop with no area\n");
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
							    v2->vg_p->coord, edge2, s->tol)) > (-1))
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

			    bu_vls_printf(s->log_str, "%s", bu_vls_cstr(&tmp_vls));
			    bu_vls_free(&tmp_vls);
			    return;
			}
		    }
		}

		/* Create a temporary model to store the basis loop */
		m_tmp = nmg_mm();
		r_tmp = nmg_mrsv(m_tmp);
		s_tmp = BU_LIST_FIRST(shell, &r_tmp->s_hd);
		n->lu_copy = nmg_dup_loop(lu, &s_tmp->l.magic, (long **)0);
		if (!n->lu_copy) {
		    bu_vls_printf(s->log_str, "Failed to make copy of loop\n");
		    nmg_km(m_tmp);
		    return;
		}

		/* Get the first vertex in the loop as the basis for extrusion */
		eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
		VMOVE(n->lu_keypoint, eu->vu_p->v_p->vg_p->coord);

		sh = lu->up.s_p;

		if (BU_LIST_NON_EMPTY(&sh->fu_hd)) {
		    /* make a new shell to hold the extruded solid */

		    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
		    NMG_CK_REGION(r);
		    n->es_s = nmg_msv(r);
		} else {
		    n->es_s = sh;
		}

	    }
	    break;
    }
    /* For example, this will set edit_flagflag = ECMD_NMG_EPICK */
    s->edit_flag = arg;

    switch (arg) {
	case ECMD_NMG_EMOVE:
	case ECMD_NMG_ESPLIT:
	case ECMD_NMG_LEXTRU:
	    s->solid_edit_rotate = 0;
	    s->solid_edit_translate = 1;
	    s->solid_edit_scale = 0;
	    s->solid_edit_pick = 0;
	    break;
	case ECMD_NMG_EPICK:
	    s->solid_edit_rotate = 0;
	    s->solid_edit_translate = 0;
	    s->solid_edit_scale = 0;
	    s->solid_edit_pick = 1;
	    break;
	default:
	    rt_solid_edit_set_edflag(s, arg);
	    break;
    };

    rt_solid_edit_process(s);
}
struct rt_solid_edit_menu_item nmg_menu[] = {
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

struct rt_solid_edit_menu_item *
rt_solid_edit_nmg_menu_item(const struct bn_tol *UNUSED(tol))
{
    return nmg_menu;
}


const char *
rt_solid_edit_nmg_keypoint(
	point_t *pt,
	const char *UNUSED(keystr),
	const mat_t mat,
	struct rt_solid_edit *s,
	const struct bn_tol *UNUSED(tol))
{
    struct rt_db_internal *ip = &s->es_int;
    struct rt_nmg_edit *n = (struct rt_nmg_edit *)s->ipe_ptr;
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
    if (n->es_eu != (struct edgeuse *)NULL &&
	    n->es_eu->vu_p != (struct vertexuse *)NULL &&
	    n->es_eu->vu_p->v_p != (struct vertex *)NULL &&
	    n->es_eu->vu_p->v_p->vg_p != (struct vertex_g *)NULL) {
	VMOVE(mpt, n->es_eu->vu_p->v_p->vg_p->coord);
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
rt_solid_edit_nmg_labels(
	int *UNUSED(num_lines),
	point_t *UNUSED(lines),
	struct rt_point_labels *pl,
	int UNUSED(max_pl),
	const mat_t xform,
	struct rt_solid_edit*s,
	struct bn_tol *UNUSED(tol))
{
    struct rt_db_internal *ip = &s->es_int;
    struct rt_nmg_edit *n = (struct rt_nmg_edit *)s->ipe_ptr;
    point_t pos_view;
    int npl = 0;
#define POINT_LABEL_STR(_pt, _str) { \
    VMOVE(pl[npl].pt, _pt); \
    bu_strlcpy(pl[npl++].str, _str, sizeof(pl[0].str)); }
    struct model *m = (struct model *) ip->idb_ptr;
    NMG_CK_MODEL(m);
    // Conditional labeling
    if (n->es_eu) {
	point_t cent;
	NMG_CK_EDGEUSE(n->es_eu);
	VADD2SCALE(cent,
		n->es_eu->vu_p->v_p->vg_p->coord,
		n->es_eu->eumate_p->vu_p->v_p->vg_p->coord,
		0.5);
	MAT4X3PNT(pos_view, xform, cent);
	POINT_LABEL_STR(pos_view, " eu");
    }

    pl[npl].str[0] = '\0';	/* Mark ending */
}


void ecmd_nmg_emove(struct rt_solid_edit *s)
{
    struct rt_nmg_edit *n = (struct rt_nmg_edit *)s->ipe_ptr;
    point_t new_pt;
    bu_clbk_t f = NULL;
    void *d = NULL;

    /* must convert to base units */
    s->e_para[0] *= s->local2base;
    s->e_para[1] *= s->local2base;
    s->e_para[2] *= s->local2base;

    if (!n->es_eu) {
	bu_vls_printf(s->log_str, "No edge selected!\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return;
    }
    NMG_CK_EDGEUSE(n->es_eu);

    if (s->e_mvalid) {
	VMOVE(new_pt, s->e_mparam);
    } else if (s->e_inpara == 3) {
	if (s->mv_context) {
	    /* apply s->e_invmat to convert to real model space */
	    MAT4X3PNT(new_pt, s->e_invmat, s->e_para);
	} else {
	    VMOVE(new_pt, s->e_para);
	}
    } else if (s->e_inpara && s->e_inpara != 3) {
	bu_vls_printf(s->log_str, "x y z coordinates required for edge move\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return;
    } else if (!s->e_mvalid && !s->e_inpara)
	return;

    if (!nmg_find_fu_of_eu(n->es_eu) && *n->es_eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
	struct loopuse *lu;
	fastf_t area;
	plane_t pl;

	/* this edge is in a wire loop
	 * keep the loop planar
	 */
	lu = n->es_eu->up.lu_p;
	NMG_CK_LOOPUSE(lu);

	/* get plane equation for loop */
	area = nmg_loop_plane_area(lu, pl);
	if (area > 0.0) {
	    vect_t view_z_dir;
	    vect_t view_dir;
	    fastf_t dist;

	    /* Get view direction vector */
	    VSET(view_z_dir, 0.0, 0.0, 1.0);
	    MAT4X3VEC(view_dir, s->vp->gv_view2model, view_z_dir);

	    /* intersect line through new_pt with plane of loop */
	    if (bg_isect_line3_plane(&dist, new_pt, view_dir, pl, s->tol) < 1) {
		/* line does not intersect plane, don't do an esplit */
		bu_vls_printf(s->log_str, "Edge Move: Cannot place new point in plane of loop\n");
		rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
		if (f)
		    (*f)(0, NULL, d, NULL);
		return;
	    }
	    VJOIN1(new_pt, new_pt, dist, view_dir);
	}
    }

    if (nmg_move_edge_thru_pnt(n->es_eu, new_pt, s->tol) < 0) {
	VPRINT("Unable to hit", new_pt);
    }
}

void ecmd_nmg_ekill(struct rt_solid_edit *s)
{
    struct rt_nmg_edit *n = (struct rt_nmg_edit *)s->ipe_ptr;
    struct model *m;
    struct edge_g_lseg *eg;
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (!n->es_eu) {
	bu_vls_printf(s->log_str, "No edge selected!\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return;
    }
    NMG_CK_EDGEUSE(n->es_eu);

    m = nmg_find_model(&n->es_eu->l.magic);

    if (*n->es_eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
	struct loopuse *lu;
	struct edgeuse *prev_eu, *next_eu;

	lu = n->es_eu->up.lu_p;
	NMG_CK_LOOPUSE(lu);

	if (*lu->up.magic_p != NMG_SHELL_MAGIC) {
	    /* Currently can only kill wire edges or edges in wire loops */
	    bu_vls_printf(s->log_str, "Currently, we can only kill wire edges or edges in wire loops\n");
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    rt_solid_edit_set_edflag(s, RT_SOLID_EDIT_IDLE);
	    return;
	}

	prev_eu = BU_LIST_PPREV_CIRC(edgeuse, &n->es_eu->l);
	NMG_CK_EDGEUSE(prev_eu);

	if (prev_eu == n->es_eu) {
	    /* only one edge left in the loop
	     * make it an edge to/from same vertex
	     */
	    if (n->es_eu->vu_p->v_p == n->es_eu->eumate_p->vu_p->v_p) {
		/* refuse to delete last edge that runs
		 * to/from same vertex
		 */
		bu_vls_printf(s->log_str, "Cannot delete last edge running to/from same vertex\n");
		rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
		if (f)
		    (*f)(0, NULL, d, NULL);
		return;
	    }
	    NMG_CK_EDGEUSE(n->es_eu->eumate_p);
	    nmg_movevu(n->es_eu->eumate_p->vu_p, n->es_eu->vu_p->v_p);
	    return;
	}

	next_eu = BU_LIST_PNEXT_CIRC(edgeuse, &n->es_eu->l);
	NMG_CK_EDGEUSE(next_eu);

	nmg_movevu(next_eu->vu_p, n->es_eu->vu_p->v_p);
	if (nmg_keu(n->es_eu)) {
	    /* Should never happen! */
	    bu_exit(EXIT_FAILURE,  "rt_solid_edit_process(s): killed edge and emptied loop!\n");
	}
	n->es_eu = prev_eu;
	nmg_rebound(m, s->tol);

	/* fix edge geometry for modified edge (next_eu) */
	eg = next_eu->g.lseg_p;
	NMG_CK_EDGE_G_LSEG(eg);
	VMOVE(eg->e_pt, next_eu->vu_p->v_p->vg_p->coord);
	VSUB2(eg->e_dir, next_eu->eumate_p->vu_p->v_p->vg_p->coord, next_eu->vu_p->v_p->vg_p->coord);

	return;
    } else if (*n->es_eu->up.magic_p == NMG_SHELL_MAGIC) {
	/* wire edge, just kill it */
	(void)nmg_keu(n->es_eu);
	n->es_eu = (struct edgeuse *)NULL;
	nmg_rebound(m, s->tol);
    }
}

void ecmd_nmg_esplit(struct rt_solid_edit *s)
{
    struct rt_nmg_edit *n = (struct rt_nmg_edit *)s->ipe_ptr;
    struct vertex *v=(struct vertex *)NULL;
    struct edge_g_lseg *eg;
    struct model *m;
    point_t new_pt;
    fastf_t area;
    plane_t pl;
    bu_clbk_t f = NULL;
    void *d = NULL;

    /* must convert to base units */
    s->e_para[0] *= s->local2base;
    s->e_para[1] *= s->local2base;
    s->e_para[2] *= s->local2base;

    if (!n->es_eu) {
	bu_vls_printf(s->log_str, "No edge selected!\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return;
    }
    NMG_CK_EDGEUSE(n->es_eu);
    m = nmg_find_model(&n->es_eu->l.magic);
    NMG_CK_MODEL(m);
    if (s->e_mvalid) {
	VMOVE(new_pt, s->e_mparam);
    } else if (s->e_inpara == 3) {
	if (s->mv_context) {
	    /* apply s->e_invmat to convert to real model space */
	    MAT4X3PNT(new_pt, s->e_invmat, s->e_para);
	} else {
	    VMOVE(new_pt, s->e_para);
	}
    } else if (s->e_inpara && s->e_inpara != 3) {
	bu_vls_printf(s->log_str, "x y z coordinates required for edge split\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return;
    } else if (!s->e_mvalid && !s->e_inpara)
	return;

    if (*n->es_eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
	struct loopuse *lu;

	lu = n->es_eu->up.lu_p;
	NMG_CK_LOOPUSE(lu);

	/* Currently, can only split wire edges or edges in wire loops */
	if (*lu->up.magic_p != NMG_SHELL_MAGIC) {
	    bu_vls_printf(s->log_str, "Currently, we can only split wire edges or edges in wire loops\n");
	    rt_solid_edit_set_edflag(s, RT_SOLID_EDIT_IDLE);
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
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
	    MAT4X3VEC(view_dir, s->vp->gv_view2model, view_z_dir);

	    /* intersect line through new_pt with plane of loop */
	    if (bg_isect_line3_plane(&dist, new_pt, view_dir, pl, s->tol) < 1) {
		/* line does not intersect plane, don't do an esplit */
		bu_vls_printf(s->log_str, "Edge Split: Cannot place new point in plane of loop\n");
		rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
		if (f)
		    (*f)(0, NULL, d, NULL);
		return;
	    }
	    VJOIN1(new_pt, new_pt, dist, view_dir);
	}
    }
    n->es_eu = nmg_esplit(v, n->es_eu, 0);
    nmg_vertex_gv(n->es_eu->vu_p->v_p, new_pt);
    nmg_rebound(m, s->tol);
    eg = n->es_eu->g.lseg_p;
    NMG_CK_EDGE_G_LSEG(eg);
    VMOVE(eg->e_pt, new_pt);
    VSUB2(eg->e_dir, n->es_eu->eumate_p->vu_p->v_p->vg_p->coord, new_pt);
}

void ecmd_nmg_lextru(struct rt_solid_edit *s)
{
    struct rt_nmg_edit *n = (struct rt_nmg_edit *)s->ipe_ptr;
    fastf_t dist;
    point_t to_pt;
    vect_t extrude_vec;
    struct loopuse *new_lu;
    struct faceuse *fu;
    struct model *m;
    plane_t new_lu_pl;
    fastf_t area;
    bu_clbk_t f = NULL;
    void *d = NULL;

    /* must convert to base units */
    s->e_para[0] *= s->local2base;
    s->e_para[1] *= s->local2base;
    s->e_para[2] *= s->local2base;

    if (s->e_mvalid) {
	VMOVE(to_pt, s->e_mparam);
    } else if (s->e_inpara == 3) {
	if (s->mv_context) {
	    /* apply s->e_invmat to convert to real model space */
	    MAT4X3PNT(to_pt, s->e_invmat, s->e_para);
	} else {
	    VMOVE(to_pt, s->e_para);
	}
    } else if (s->e_inpara == 1) {
	VJOIN1(to_pt, n->lu_keypoint, s->e_para[0], n->lu_pl);
    } else if (s->e_inpara && s->e_inpara != 3) {
	bu_vls_printf(s->log_str, "x y z coordinates required for loop extrusion\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return;
    } else if (!s->e_mvalid && !s->e_inpara) {
	return;
    }

    VSUB2(extrude_vec, to_pt, n->lu_keypoint);

    if (bg_isect_line3_plane(&dist, to_pt, extrude_vec, n->lu_pl, s->tol) < 1) {
	bu_vls_printf(s->log_str, "Cannot extrude parallel to plane of loop\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return;
    }

    if (BU_LIST_NON_EMPTY(&n->es_s->fu_hd)) {
	struct nmgregion *r;

	r = n->es_s->r_p;
	(void) nmg_ks(n->es_s);
	n->es_s = nmg_msv(r);
    }

    new_lu = nmg_dup_loop(n->lu_copy, &n->es_s->l.magic, (long **)0);
    area = nmg_loop_plane_area(new_lu, new_lu_pl);
    if (area < 0.0) {
	bu_vls_printf(s->log_str, "loop to be extruded as no area!\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
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

    (void)nmg_extrude_face(fu, extrude_vec, s->vlfree, s->tol);

    nmg_fix_normals(fu->s_p, s->vlfree, s->tol);

    m = nmg_find_model(&fu->l.magic);
    nmg_rebound(m, s->tol);
    (void)nmg_ck_geometry(m, s->vlfree, s->tol);

    n->es_eu = (struct edgeuse *)NULL;

    f = NULL; d = NULL;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_REPLOT_EDITING_SOLID, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, NULL);

    int vs_flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_VIEW_SET_FLAG, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &vs_flag);
}


/* XXX Should just leave desired location in s->e_mparam for rt_solid_edit_process(s) */
void ecmd_nmg_epick(struct rt_solid_edit *s, const vect_t mousevec)
{
    struct rt_nmg_edit *n = (struct rt_nmg_edit *)s->ipe_ptr;
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */

    struct model *m =
	(struct model *)s->es_int.idb_ptr;
    struct edge *e;
    struct bn_tol tmp_tol;
    bu_clbk_t f = NULL;
    void *d = NULL;

    NMG_CK_MODEL(m);

    /* Picking an edge should not depend on tolerances! */
    tmp_tol.magic = BN_TOL_MAGIC;
    tmp_tol.dist = 0.0;
    tmp_tol.dist_sq = tmp_tol.dist * tmp_tol.dist;
    tmp_tol.perp = 0.0;
    tmp_tol.para = 1 - tmp_tol.perp;

    MAT4X3PNT(pos_view, s->vp->gv_model2view, s->curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    if ((e = nmg_find_e_nearest_pt2(&m->magic, pos_view,
		    s->vp->gv_model2view, s->vlfree, &tmp_tol)) == (struct edge *)NULL) {
	bu_vls_printf(s->log_str, "ECMD_NMG_EPICK: unable to find an edge\n");
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	return;
    }
    n->es_eu = e->eu_p;
    NMG_CK_EDGEUSE(n->es_eu);

    {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls,
		"edgeuse selected = %p (%g %g %g) <-> (%g %g %g)\n",
		(void *)n->es_eu, V3ARGS(n->es_eu->vu_p->v_p->vg_p->coord),
		V3ARGS(n->es_eu->eumate_p->vu_p->v_p->vg_p->coord));
	bu_vls_printf(s->log_str, "%s", bu_vls_cstr(&tmp_vls));
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
	bu_vls_free(&tmp_vls);
    }
}

int
rt_solid_edit_nmg_edit(struct rt_solid_edit *s, int edflag)
{
    struct rt_nmg_edit *n = (struct rt_nmg_edit *)s->ipe_ptr;
    switch (edflag) {
	case RT_SOLID_EDIT_SCALE:
	    /* scale the solid uniformly about its vertex point */
	    n->es_eu = (struct edgeuse *)NULL;	/* Reset n->es_eu */
	    return rt_solid_edit_generic_sscale(s, &s->es_int);
	case RT_SOLID_EDIT_TRANS:
	    /* translate solid */
	    n->es_eu = (struct edgeuse *)NULL;	/* Reset n->es_eu */
	    rt_solid_edit_generic_strans(s, &s->es_int);
	    break;
	case RT_SOLID_EDIT_ROT:
	    /* rot solid about vertex */
	    n->es_eu = (struct edgeuse *)NULL;	/* Reset n->es_eu */
	    rt_solid_edit_generic_srot(s, &s->es_int);
	    break;
	case ECMD_NMG_EPICK:
	    /* XXX Nothing to do here (yet), all done in mouse routine. */
	    break;
	case ECMD_NMG_EMOVE:
	    ecmd_nmg_emove(s);
	    break;
	case ECMD_NMG_EKILL:
	    ecmd_nmg_ekill(s);
	    /* fall through */
	case ECMD_NMG_ESPLIT:
	    ecmd_nmg_esplit(s);
	    break;
	case ECMD_NMG_LEXTRU:
	    ecmd_nmg_lextru(s);
	    break;
    }

    return 0;
}

int
rt_solid_edit_nmg_edit_xy(
	struct rt_solid_edit *s,
	int edflag,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    vect_t temp = VINIT_ZERO;
    struct rt_db_internal *ip = &s->es_int;
    bu_clbk_t f = NULL;
    void *d = NULL;

    switch (edflag) {
	case RT_SOLID_EDIT_SCALE:
	case RT_SOLID_EDIT_PSCALE:
	    rt_solid_edit_generic_sscale_xy(s, mousevec);
	    rt_solid_edit_nmg_edit(s, edflag);
	    return 0;
	case RT_SOLID_EDIT_TRANS:
	    rt_solid_edit_generic_strans_xy(&pos_view, s, mousevec);
	    break;
	case ECMD_NMG_EPICK:
	    /* XXX Should just leave desired location in s->e_mparam for rt_solid_edit_nmg_edit */
	    ecmd_nmg_epick(s, mousevec);
	    break;
	case ECMD_NMG_LEXTRU:
	case ECMD_NMG_EMOVE:
	case ECMD_NMG_ESPLIT:
              MAT4X3PNT(pos_view, s->vp->gv_model2view, s->curr_e_axes_pos);
              pos_view[X] = mousevec[X];
              pos_view[Y] = mousevec[Y];
              MAT4X3PNT(temp, s->vp->gv_view2model, pos_view);
              MAT4X3PNT(s->e_mparam, s->e_invmat, temp);
              s->e_mvalid = 1;
	      break;
	default:
	    bu_vls_printf(s->log_str, "%s: XY edit undefined in solid edit mode %d\n", EDOBJ[ip->idb_type].ft_label, edflag);
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
    }

    rt_update_edit_absolute_tran(s, pos_view);
    rt_solid_edit_nmg_edit(s, edflag);

    return 0;
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
