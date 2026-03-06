/*                         E D B S P L I N E . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2025 United States Government as represented by
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
/** @file primitives/edbspline.c
 *
 *  TODO - this whole bspline editing logic needs a re-look, or better still we
 *  should just translate these into brep objects and do any editing there.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"
#include "../edit_private.h"

#define ECMD_VTRANS		9017	/* vertex translate */
#define ECMD_SPLINE_VPICK       9018	/* vertex pick via mouse proximity */
#define ECMD_BSPLINE_PICK_CP    9019	/* pick control point by (surf, u, v) indices from e_para */
/*
 * Pick a knot by surface index + direction + position index.
 *
 * e_para[0] = surface index (0-based)
 * e_para[1] = knot direction: 0 = U knot vector, 1 = V knot vector
 * e_para[2] = knot index (0-based within the selected direction's vector)
 * e_inpara  = 3
 *
 * On success the selection is stored in rt_bspline_edit::knot_dir and
 * rt_bspline_edit::knot_idx (and spl_surfno is updated).
 */
#define ECMD_BSPLINE_PICK_KNOT  9020
/*
 * Set the currently selected knot value.
 *
 * e_para[0] = new knot value
 * e_inpara  = 1
 *
 * The value is written directly to the surface's knot vector at the
 * position selected by a prior ECMD_BSPLINE_PICK_KNOT.  No monotonicity
 * enforcement is performed — caller is responsible for maintaining a
 * valid (non-decreasing) knot sequence.
 */
#define ECMD_BSPLINE_SET_KNOT   9021

struct rt_bspline_edit {
    int spl_surfno;	/* What surf & ctl pt to edit on spline */
    int spl_ui;
    int spl_vi;

    point_t v_pos;  // vpick point.

    /* Knot selection (set by ECMD_BSPLINE_PICK_KNOT, used by ECMD_BSPLINE_SET_KNOT) */
    int knot_dir;   /* 0 = U knot vector, 1 = V knot vector */
    int knot_idx;   /* index within the selected direction's vector */
};

void *
rt_edit_bspline_prim_edit_create(struct rt_edit *s)
{
    struct rt_bspline_edit *e;
    BU_GET(e, struct rt_bspline_edit);
    if (!s)
	return (void *)e;

    // If we have the solid edit, do some more setup
    struct rt_nurb_internal *sip = (struct rt_nurb_internal *) s->es_int.idb_ptr;
    RT_NURB_CK_MAGIC(sip);
    e->spl_surfno = sip->nsrf/2;
    struct face_g_snurb *surf = sip->srfs[e->spl_surfno];
    NMG_CK_SNURB(surf);
    e->spl_ui = surf->s_size[1]/2;
    e->spl_vi = surf->s_size[0]/2;
    e->knot_dir = 0;
    e->knot_idx = 0;

    return (void *)e;
}

void
rt_edit_bspline_prim_edit_destroy(struct rt_bspline_edit *e)
{
    if (!e)
	return;
    BU_PUT(e, struct rt_bspline_edit);
}

void
rt_edit_bspline_set_edit_mode(struct rt_edit *s, int mode)
{
    /* XXX Why were we doing VPICK with chg_state?? */
    //chg_state(s, ST_S_EDIT, ST_S_VPICK, "Vertex Pick");
    if (mode < 0) {
	/* Enter picking state */
	rt_edit_set_edflag(s, ECMD_SPLINE_VPICK);
	s->edit_mode = RT_PARAMS_EDIT_PICK;
	return;
    }

    rt_edit_set_edflag(s, mode);
    if (mode == ECMD_VTRANS)
	s->edit_mode = RT_PARAMS_EDIT_TRANS;
    if (mode == ECMD_BSPLINE_PICK_CP)
	s->edit_mode = RT_PARAMS_EDIT_PICK;
    if (mode == ECMD_BSPLINE_PICK_KNOT)
	s->edit_mode = RT_PARAMS_EDIT_PICK;
    if (mode == ECMD_BSPLINE_SET_KNOT)
	s->edit_mode = RT_PARAMS_EDIT_TRANS;

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);

}

/*ARGSUSED*/
static void
spline_ed(struct rt_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    /* XXX Why were we doing VPICK with chg_state?? */
    //chg_state(s, ST_S_EDIT, ST_S_VPICK, "Vertex Pick");
    if (arg < 0) {
	/* Enter picking state */
	rt_edit_set_edflag(s, ECMD_SPLINE_VPICK);
	s->edit_mode = RT_PARAMS_EDIT_PICK;
	return;
    }

    rt_edit_set_edflag(s, arg);
    if (arg == ECMD_VTRANS)
	s->edit_mode = RT_PARAMS_EDIT_TRANS;
    if (arg == ECMD_BSPLINE_PICK_CP)
	s->edit_mode = RT_PARAMS_EDIT_PICK;
    if (arg == ECMD_BSPLINE_PICK_KNOT)
	s->edit_mode = RT_PARAMS_EDIT_PICK;
    if (arg == ECMD_BSPLINE_SET_KNOT)
	s->edit_mode = RT_PARAMS_EDIT_TRANS;

    rt_edit_process(s);

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_edit_menu_item spline_menu[] = {
    { "SPLINE MENU", NULL, 0 },
    { "Pick Vertex", spline_ed, -1 },
    { "Pick CP by Index", spline_ed, ECMD_BSPLINE_PICK_CP },
    { "Move Vertex", spline_ed, ECMD_VTRANS },
    { "Pick Knot", spline_ed, ECMD_BSPLINE_PICK_KNOT },
    { "Set Knot Value", spline_ed, ECMD_BSPLINE_SET_KNOT },
    { "", NULL, 0 }
};

struct rt_edit_menu_item *
rt_edit_bspline_menu_item(const struct bn_tol *UNUSED(tol))
{
    return spline_menu;
}


// TODO - either use vmath.h versions or merge these into it...
#define DIST2D(P0, P1) sqrt(((P1)[X] - (P0)[X])*((P1)[X] - (P0)[X]) + \
				((P1)[Y] - (P0)[Y])*((P1)[Y] - (P0)[Y]))

#define DIST3D(P0, P1) sqrt(((P1)[X] - (P0)[X])*((P1)[X] - (P0)[X]) + \
				((P1)[Y] - (P0)[Y])*((P1)[Y] - (P0)[Y]) + \
				((P1)[Z] - (P0)[Z])*((P1)[Z] - (P0)[Z]))

/*
 * Given a pointer (vhead) to vlist point coordinates, a reference
 * point (ref_pt), and a transformation matrix (mat), pass back in
 * "closest_pt" the original, untransformed 3 space coordinates of
 * the point nearest the reference point after all points have been
 * transformed into 2 space projection plane coordinates.
 */
int
nurb_closest2d(
    int *surface,
    int *uval,
    int *vval,
    const struct rt_nurb_internal *spl,
    const point_t ref_pt,
    const mat_t mat)
{
    struct face_g_snurb *srf;
    point_t ref_2d;
    fastf_t *mesh;
    fastf_t d;
    fastf_t c_dist;		/* closest dist so far */
    int c_surfno;
    int c_u, c_v;
    int u, v;
    int i;

    RT_NURB_CK_MAGIC(spl);

    c_dist = INFINITY;
    c_surfno = c_u = c_v = -1;

    /* transform reference point to 2d */
    MAT4X3PNT(ref_2d, mat, ref_pt);

    for (i = 0; i < spl->nsrf; i++) {
	int advance;

	srf = spl->srfs[i];
	NMG_CK_SNURB(srf);
	mesh = srf->ctl_points;
	advance = RT_NURB_EXTRACT_COORDS(srf->pt_type);

	for (v = 0; v < srf->s_size[0]; v++) {
	    for (u = 0; u < srf->s_size[1]; u++) {
		point_t cur;
		/* XXX 4-tuples? */
		MAT4X3PNT(cur, mat, mesh);
		d = DIST2D(ref_2d, cur);
		if (d < c_dist) {
		    c_dist = d;
		    c_surfno = i;
		    c_u = u;
		    c_v = v;
		}
		mesh += advance;
	    }
	}
    }
    if (c_surfno < 0) return -1;		/* FAIL */
    *surface = c_surfno;
    *uval = c_u;
    *vval = c_v;

    return 0;				/* success */
}


void
sedit_vpick(struct rt_edit *s)
{
    struct rt_bspline_edit *b = (struct rt_bspline_edit *)s->ipe_ptr;
    int surfno, u, v;

    if (!s->vp) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_SPLINE_VPICK: no view attached\n");
	return;
    }

    /*
     * Compute the object-view matrices:
     *   model2objview = gv_model2view * model_changes
     *   objview2model = inverse(model2objview)
     *
     * These are the equivalents of vs_model2objview and vs_objview2model
     * in MGED's view_state.
     */
    mat_t model2objview, objview2model;
    bn_mat_mul(model2objview, s->vp->gv_model2view, s->model_changes);
    bn_mat_inv(objview2model, model2objview);

    /*
     * b->v_pos is the view-space cursor position set by ft_edit_xy
     * before dispatching to ft_edit.  Transform it into model space
     * for use as the reference point for the 2-D proximity search.
     */
    point_t m_pos;
    MAT4X3PNT(m_pos, objview2model, b->v_pos);

    if (nurb_closest2d(&surfno, &u, &v,
		       (struct rt_nurb_internal *)s->es_int.idb_ptr,
		       m_pos, model2objview) >= 0) {
	b->spl_surfno = surfno;
	b->spl_ui = u;
	b->spl_vi = v;
	s->e_keytag = (*EDOBJ[ID_BSPLINE].ft_keypoint)(
		&s->e_keypoint, s->e_keytag, s->e_mat, s, s->tol);
    }

    /* draw arrow, etc. */
    bu_clbk_t f = NULL;
    void *d = NULL;
    int vs_flag = 1;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_VIEW_SET_FLAG, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &vs_flag);
}

void
rt_edit_bspline_labels(
  	int *UNUSED(num_lines),
	point_t *UNUSED(lines),
	struct rt_point_labels *pl,
	int UNUSED(max_pl),
	const mat_t xform,
	struct rt_edit *s,
	struct bn_tol *UNUSED(tol)
    )
{
    struct rt_db_internal *ip = &s->es_int;
    struct rt_bspline_edit *b = (struct rt_bspline_edit *)s->ipe_ptr;
    point_t pos_view;
    int npl = 0;

#define POINT_LABEL(_pt, _char) { \
    VMOVE(pl[npl].pt, _pt); \
    pl[npl].str[0] = _char; \
    pl[npl++].str[1] = '\0'; }

#define POINT_LABEL_STR(_pt, _str) { \
    VMOVE(pl[npl].pt, _pt); \
    bu_strlcpy(pl[npl++].str, _str, sizeof(pl[0].str)); }

    struct rt_nurb_internal *sip = (struct rt_nurb_internal *)ip->idb_ptr;
    RT_NURB_CK_MAGIC(sip);

    // Conditional labeling
    struct face_g_snurb *surf = sip->srfs[b->spl_surfno];
    NMG_CK_SNURB(surf);
    fastf_t *fp = &RT_NURB_GET_CONTROL_POINT(surf, b->spl_ui, b->spl_vi);
    MAT4X3PNT(pos_view, xform, fp);
    POINT_LABEL(pos_view, 'V');

    fp = &RT_NURB_GET_CONTROL_POINT(surf, 0, 0);
    MAT4X3PNT(pos_view, xform, fp);
    POINT_LABEL_STR(pos_view, " 0,0");
    fp = &RT_NURB_GET_CONTROL_POINT(surf, 0, surf->s_size[1]-1);
    MAT4X3PNT(pos_view, xform, fp);
    POINT_LABEL_STR(pos_view, " 0,u");
    fp = &RT_NURB_GET_CONTROL_POINT(surf, surf->s_size[0]-1, 0);
    MAT4X3PNT(pos_view, xform, fp);
    POINT_LABEL_STR(pos_view, " v,0");
    fp = &RT_NURB_GET_CONTROL_POINT(surf, surf->s_size[0]-1, surf->s_size[1]-1);
    MAT4X3PNT(pos_view, xform, fp);
    POINT_LABEL_STR(pos_view, " u,v");

    pl[npl].str[0] = '\0';	/* Mark ending */
}

const char *
rt_edit_bspline_keypoint(
	point_t *pt,
	const char *UNUSED(keystr),
	const mat_t mat,
	struct rt_edit *s,
	const struct bn_tol *UNUSED(tol))
{
    struct rt_db_internal *ip = &s->es_int;
    struct rt_bspline_edit *b = (struct rt_bspline_edit *)s->ipe_ptr;
    point_t mpt = VINIT_ZERO;
    static char buf[BUFSIZ];

    RT_CK_DB_INTERNAL(ip);
    memset(buf, 0, BUFSIZ);

    struct rt_nurb_internal *sip = (struct rt_nurb_internal *)ip->idb_ptr;
    struct face_g_snurb *surf;
    fastf_t *fp;

    RT_NURB_CK_MAGIC(sip);
    surf = sip->srfs[b->spl_surfno];
    NMG_CK_SNURB(surf);
    fp = &RT_NURB_GET_CONTROL_POINT(surf, b->spl_ui, b->spl_vi);
    VMOVE(mpt, fp);
    sprintf(buf, "Surf %d, index %d,%d",
	    b->spl_surfno, b->spl_ui, b->spl_vi);
    MAT4X3PNT(*pt, mat, mpt);
    return (const char *)buf;
}

/* Pick a control point by explicit (surfno, u, v) indices.
 * e_para[0] = surface index, e_para[1] = u index, e_para[2] = v index.
 * e_inpara must be 3. */
static int
ecmd_bspline_pick_cp(struct rt_edit *s)
{
    struct rt_bspline_edit *b = (struct rt_bspline_edit *)s->ipe_ptr;
    struct rt_nurb_internal *sip =
	(struct rt_nurb_internal *)s->es_int.idb_ptr;

    RT_NURB_CK_MAGIC(sip);

    if (!s->e_inpara || s->e_inpara < 3) {
	bu_vls_printf(s->log_str,
		"ERROR: three indices required (surfno u v)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    int sno = (int)s->e_para[0];
    int ui  = (int)s->e_para[1];
    int vi  = (int)s->e_para[2];

    if (sno < 0 || sno >= sip->nsrf) {
	bu_vls_printf(s->log_str,
		"ERROR: surface index %d out of range [0, %d)\n",
		sno, sip->nsrf);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    struct face_g_snurb *surf = sip->srfs[sno];
    NMG_CK_SNURB(surf);

    if (ui < 0 || ui >= surf->s_size[1] ||
	vi < 0 || vi >= surf->s_size[0]) {
	bu_vls_printf(s->log_str,
		"ERROR: CP index (%d,%d) out of range u=[0,%d) v=[0,%d)\n",
		ui, vi, surf->s_size[1], surf->s_size[0]);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    b->spl_surfno = sno;
    b->spl_ui     = ui;
    b->spl_vi     = vi;
    s->e_inpara   = 0;
    return 0;
}

/* Pick a knot by surface index, direction, and knot position index.
 * e_para[0] = surface index, e_para[1] = direction (0=U, 1=V),
 * e_para[2] = knot index within the chosen vector.
 * e_inpara must be 3. */
static int
ecmd_bspline_pick_knot(struct rt_edit *s)
{
    struct rt_bspline_edit *b = (struct rt_bspline_edit *)s->ipe_ptr;
    struct rt_nurb_internal *sip =
	(struct rt_nurb_internal *)s->es_int.idb_ptr;

    RT_NURB_CK_MAGIC(sip);

    if (!s->e_inpara || s->e_inpara < 3) {
	bu_vls_printf(s->log_str,
		"ERROR: three values required (surfno direction knot_index)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    int sno  = (int)s->e_para[0];
    int dir  = (int)s->e_para[1];
    int kidx = (int)s->e_para[2];

    if (sno < 0 || sno >= sip->nsrf) {
	bu_vls_printf(s->log_str,
		"ERROR: surface index %d out of range [0, %d)\n",
		sno, sip->nsrf);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    struct face_g_snurb *surf = sip->srfs[sno];
    NMG_CK_SNURB(surf);

    if (dir != 0 && dir != 1) {
	bu_vls_printf(s->log_str,
		"ERROR: direction must be 0 (U) or 1 (V), got %d\n", dir);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    int ksize = (dir == 0) ? surf->u.k_size : surf->v.k_size;
    if (kidx < 0 || kidx >= ksize) {
	bu_vls_printf(s->log_str,
		"ERROR: knot index %d out of range [0, %d) for direction %s\n",
		kidx, ksize, (dir == 0) ? "U" : "V");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    b->spl_surfno = sno;
    b->knot_dir   = dir;
    b->knot_idx   = kidx;
    s->e_inpara   = 0;
    return 0;
}

/* Set the currently selected knot to the value in e_para[0].
 * e_inpara must be 1.  Caller must ensure the resulting vector stays
 * non-decreasing. */
static int
ecmd_bspline_set_knot(struct rt_edit *s)
{
    struct rt_bspline_edit *b = (struct rt_bspline_edit *)s->ipe_ptr;
    struct rt_nurb_internal *sip =
	(struct rt_nurb_internal *)s->es_int.idb_ptr;

    RT_NURB_CK_MAGIC(sip);

    if (!s->e_inpara || s->e_inpara < 1) {
	bu_vls_printf(s->log_str,
		"ERROR: knot value required (e_inpara must be 1)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    if (b->spl_surfno < 0 || b->spl_surfno >= sip->nsrf) {
	bu_vls_printf(s->log_str,
		"ERROR: no valid surface selected (use ECMD_BSPLINE_PICK_KNOT first)\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    struct face_g_snurb *surf = sip->srfs[b->spl_surfno];
    NMG_CK_SNURB(surf);

    struct knot_vector *kv = (b->knot_dir == 0) ? &surf->u : &surf->v;

    if (b->knot_idx < 0 || b->knot_idx >= kv->k_size) {
	bu_vls_printf(s->log_str,
		"ERROR: stored knot index %d out of range [0, %d)\n",
		b->knot_idx, kv->k_size);
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    kv->knots[b->knot_idx] = s->e_para[0];
    s->e_inpara = 0;
    return 0;
}

// I think this is bspline only??
void
ecmd_vtrans(struct rt_edit *s)
{
    struct rt_bspline_edit *b = (struct rt_bspline_edit *)s->ipe_ptr;

    /* must convert to base units */
    s->e_para[0] *= s->local2base;
    s->e_para[1] *= s->local2base;
    s->e_para[2] *= s->local2base;

    /* translate a vertex */
    if (s->e_mvalid) {
	/* Mouse parameter:  new position in model space */
	VMOVE(s->e_para, s->e_mparam);
	s->e_inpara = 1;
    }
    if (s->e_inpara) {


	/* Keyboard parameter:  new position in model space.
	 * XXX for now, splines only here */
	struct rt_nurb_internal *sip =
	    (struct rt_nurb_internal *) s->es_int.idb_ptr;
	struct face_g_snurb *surf;
	fastf_t *fp;

	RT_NURB_CK_MAGIC(sip);
	surf = sip->srfs[b->spl_surfno];
	NMG_CK_SNURB(surf);
	fp = &RT_NURB_GET_CONTROL_POINT(surf, b->spl_ui, b->spl_vi);
	if (s->mv_context) {
	    /* apply s->e_invmat to convert to real model space */
	    MAT4X3PNT(fp, s->e_invmat, s->e_para);
	} else {
	    VMOVE(fp, s->e_para);
	}
    }
}


int
rt_edit_bspline_edit(struct rt_edit *s)
{
    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    /* scale the solid uniformly about its vertex point */
	    return edit_sscale(s);
	case RT_PARAMS_EDIT_TRANS:
	    /* translate solid */
	    edit_stra(s);
	    break;
	case RT_PARAMS_EDIT_ROT:
	    /* rot solid about vertex */
	    edit_srot(s);
	    break;
	case ECMD_SPLINE_VPICK:
	    sedit_vpick(s);
	    break;
	case ECMD_BSPLINE_PICK_CP:
	    return ecmd_bspline_pick_cp(s);
	case ECMD_BSPLINE_PICK_KNOT:
	    return ecmd_bspline_pick_knot(s);
	case ECMD_BSPLINE_SET_KNOT:
	    return ecmd_bspline_set_knot(s);
	case ECMD_VTRANS:
	    // I think this is bspline only??
	    ecmd_vtrans(s);
	    break;
	default:
	    return edit_generic(s);
    }

    return 0;
}

int
rt_edit_bspline_edit_xy(
	struct rt_edit *s,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    vect_t temp = VINIT_ZERO;

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	case ECMD_BSPLINE_PICK_CP:
	case ECMD_BSPLINE_PICK_KNOT:
	    edit_sscale_xy(s, mousevec);
	    return 0;
	case ECMD_SPLINE_VPICK: {
	    /* Store the view-space cursor position in b->v_pos so that
	     * sedit_vpick() can convert it to model space and find the
	     * nearest control point via nurb_closest2d. */
	    struct rt_bspline_edit *b2 = (struct rt_bspline_edit *)s->ipe_ptr;
	    VSET(b2->v_pos, mousevec[X], mousevec[Y], 0.0);
	    /* ft_edit will call sedit_vpick */
	    return 0;
	}
	case RT_PARAMS_EDIT_TRANS:
	    edit_stra_xy(&pos_view, s, mousevec);
	    break;
	case ECMD_VTRANS:
	    /*
	     * Use mouse to change a vertex location.
	     * Project vertex (in solid keypoint) into view space,
	     * replace X, Y (but NOT Z) components, and
	     * project result back to model space.
	     * Leave desired location in s->e_mparam.
	     */
	    MAT4X3PNT(pos_view, s->vp->gv_model2view, s->curr_e_axes_pos);
	    pos_view[X] = mousevec[X];
	    pos_view[Y] = mousevec[Y];
	    MAT4X3PNT(temp, s->vp->gv_view2model, pos_view);
	    MAT4X3PNT(s->e_mparam, s->e_invmat, temp);
	    s->e_mvalid = 1;      /* s->e_mparam is valid */
	    /* Leave the rest to code in ft_edit */
	    break;
	default:
	    return edit_generic_xy(s, mousevec);
    }

    edit_abs_tra(s, pos_view);

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
