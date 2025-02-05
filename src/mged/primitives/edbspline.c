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
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"
#include "edfunctab.h"

#define ECMD_VTRANS		9017	/* vertex translate */
#define ECMD_SPLINE_VPICK       9018	/* vertex pick */

struct rt_bspline_edit {
    int spl_surfno;	/* What surf & ctl pt to edit on spline */
    int spl_ui;
    int spl_vi;

    point_t v_pos;  // vpick point.
};

void *
rt_solid_edit_bspline_prim_edit_create(struct rt_solid_edit *s)
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

    return (void *)e;
}

void
rt_solid_edit_bspline_prim_edit_destroy(struct rt_bspline_edit *e)
{
    if (!e)
	return;
    BU_PUT(e, struct rt_bspline_edit);
}

/*ARGSUSED*/
static void
spline_ed(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    /* XXX Why were we doing VPICK with chg_state?? */
    //chg_state(s, ST_S_EDIT, ST_S_VPICK, "Vertex Pick");
    if (arg < 0) {
	/* Enter picking state */
	s->edit_flag = ECMD_SPLINE_VPICK;
	s->solid_edit_rotate = 0;
	s->solid_edit_translate = 0;
	s->solid_edit_scale = 0;
	s->solid_edit_pick = 1;
	return;
    }
    /* For example, this will set edit_flag = ECMD_VTRANS */
    if (arg == ECMD_VTRANS) {
	s->edit_flag = ECMD_VTRANS;
	s->solid_edit_rotate = 0;
	s->solid_edit_translate = 1;
	s->solid_edit_scale = 0;
	s->solid_edit_pick = 0;
    } else {
	rt_solid_edit_set_edflag(s, arg);
    }

    rt_solid_edit_process(s);

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_clbk_get(&f, &d, s, ECMD_EAXES_POS, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item spline_menu[] = {
    { "SPLINE MENU", NULL, 0 },
    { "Pick Vertex", spline_ed, -1 },
    { "Move Vertex", spline_ed, ECMD_VTRANS },
    { "", NULL, 0 }
};

struct rt_solid_edit_menu_item *
rt_solid_edit_bspline_menu_item(const struct bn_tol *UNUSED(tol))
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
sedit_vpick(struct rt_solid_edit *s)
{
#if 0
    point_t m_pos;
    int surfno, u, v;

    MAT4X3PNT(m_pos, view_state->vs_objview2model, v_pos);

    if (nurb_closest2d(&surfno, &u, &v,
		       (struct rt_nurb_internal *)s->es_int.idb_ptr,
		       m_pos, view_state->vs_model2objview) >= 0) {
	spl_surfno = surfno;
	spl_ui = u;
	spl_vi = v;
	s->e_keytag = (*EDOBJ[ID_BSPLINE].ft_keypoint)(&s->e_keypoint, s->e_keytag, s->e_mat, &s->es_int, s->tol);
    }
    chg_state(s, ST_S_VPICK, ST_S_EDIT, "Vertex Pick Complete");
#endif

    /* draw arrow, etc. */
    bu_clbk_t f = NULL;
    void *d = NULL;
    int vs_flag = 1;
    rt_solid_edit_clbk_get(&f, &d, s, ECMD_VIEW_SET_FLAG, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &vs_flag);
}

void
rt_solid_edit_bspline_labels(
  	int *UNUSED(num_lines),
	point_t *UNUSED(lines),
	struct rt_point_labels *pl,
	int UNUSED(max_pl),
	const mat_t xform,
	struct rt_solid_edit *s,
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
rt_solid_edit_bspline_keypoint(
	point_t *pt,
	const char *UNUSED(keystr),
	const mat_t mat,
	struct rt_solid_edit *s,
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

// I think this is bspline only??
void
ecmd_vtrans(struct rt_solid_edit *s)
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
rt_solid_edit_bspline_edit(struct rt_solid_edit *s, int edflag)
{
    switch (edflag) {
	case RT_SOLID_EDIT_SCALE:
	    /* scale the solid uniformly about its vertex point */
	    return rt_solid_edit_generic_sscale(s, &s->es_int);
	case RT_SOLID_EDIT_TRANS:
	    /* translate solid */
	    rt_solid_edit_generic_strans(s, &s->es_int);
	    break;
	case RT_SOLID_EDIT_ROT:
	    /* rot solid about vertex */
	    rt_solid_edit_generic_srot(s, &s->es_int);
	    break;
	case ECMD_SPLINE_VPICK:
	    sedit_vpick(s);
	    break;
	case ECMD_VTRANS:
	    // I think this is bspline only??
	    ecmd_vtrans(s);
	    break;
    }

    return 0;
}

int
rt_solid_edit_bspline_edit_xy(
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
	    rt_solid_edit_bspline_edit(s, edflag);
	    return 0;
	case RT_SOLID_EDIT_TRANS:
	    rt_solid_edit_generic_strans_xy(&pos_view, s, mousevec);
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
	    bu_vls_printf(s->log_str, "%s: XY edit undefined in solid edit mode %d\n", EDOBJ[ip->idb_type].ft_label, edflag);
	    rt_solid_edit_clbk_get(&f, &d, s, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
    }

    rt_update_edit_absolute_tran(s, pos_view);
    rt_solid_edit_bspline_edit(s, edflag);

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
