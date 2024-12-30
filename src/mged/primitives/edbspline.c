/*                         E D B S P L I N E . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2024 United States Government as represented by
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
/** @file mged/primitives/edbspline.c
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

#include "../mged.h"
#include "../sedit.h"
#include "../mged_dm.h"
#include "./edbspline.h"


static int spl_surfno;	/* What surf & ctl pt to edit on spline */
static int spl_ui;
static int spl_vi;

extern vect_t es_mparam;	/* mouse input param.  Only when es_mvalid set */
extern int es_mvalid;	/* es_mparam valid.  inpara must = 0 */

/*ARGSUSED*/
static void
spline_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    /* XXX Why wasn't this done by setting es_edflag = ECMD_SPLINE_VPICK? */
    if (arg < 0) {
	/* Enter picking state */
	chg_state(s, ST_S_EDIT, ST_S_VPICK, "Vertex Pick");
	return;
    }
    /* For example, this will set es_edflag = ECMD_VTRANS */
    es_edflag = arg;
    sedit(s);

    set_e_axes_pos(s, 1);
}
struct menu_item spline_menu[] = {
    { "SPLINE MENU", NULL, 0 },
    { "Pick Vertex", spline_ed, -1 },
    { "Move Vertex", spline_ed, ECMD_VTRANS },
    { "", NULL, 0 }
};

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
bspline_init_sedit(struct mged_state *s)
{
    struct rt_nurb_internal *sip =
	(struct rt_nurb_internal *) s->edit_state.es_int.idb_ptr;
    struct face_g_snurb *surf;
    RT_NURB_CK_MAGIC(sip);
    spl_surfno = sip->nsrf/2;
    surf = sip->srfs[spl_surfno];
    NMG_CK_SNURB(surf);
    spl_ui = surf->s_size[1]/2;
    spl_vi = surf->s_size[0]/2;
}

void
sedit_vpick(struct mged_state *s, point_t v_pos)
{
    point_t m_pos;
    int surfno, u, v;

    MAT4X3PNT(m_pos, view_state->vs_objview2model, v_pos);

    if (nurb_closest2d(&surfno, &u, &v,
		       (struct rt_nurb_internal *)s->edit_state.es_int.idb_ptr,
		       m_pos, view_state->vs_model2objview) >= 0) {
	spl_surfno = surfno;
	spl_ui = u;
	spl_vi = v;
	get_solid_keypoint(s, &es_keypoint, &es_keytag, &s->edit_state.es_int, es_mat);
    }
    chg_state(s, ST_S_VPICK, ST_S_EDIT, "Vertex Pick Complete");
    view_state->vs_flag = 1;
}

void
mged_bspline_labels(
  	int *UNUSED(num_lines),
	point_t *UNUSED(lines),
	struct rt_point_labels *pl,
	int UNUSED(max_pl),
	const mat_t xform,
	struct rt_db_internal *ip,
	struct bn_tol *UNUSED(tol)
    )
{
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
    struct face_g_snurb *surf;
    fastf_t *fp;
    surf = sip->srfs[spl_surfno];
    NMG_CK_SNURB(surf);
    fp = &RT_NURB_GET_CONTROL_POINT(surf, spl_ui, spl_vi);
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

void
bspline_solid_keypoint(struct mged_state *s, point_t *pt, const char **strp, struct rt_db_internal *ip, fastf_t *mat)
{
    point_t mpt = VINIT_ZERO;
    static char buf[BUFSIZ];

    RT_CK_DB_INTERNAL(ip);
    memset(buf, 0, BUFSIZ);

    struct rt_nurb_internal *sip =
	(struct rt_nurb_internal *) s->edit_state.es_int.idb_ptr;
    struct face_g_snurb *surf;
    fastf_t *fp;

    RT_NURB_CK_MAGIC(sip);
    surf = sip->srfs[spl_surfno];
    NMG_CK_SNURB(surf);
    fp = &RT_NURB_GET_CONTROL_POINT(surf, spl_ui, spl_vi);
    VMOVE(mpt, fp);
    sprintf(buf, "Surf %d, index %d,%d",
	    spl_surfno, spl_ui, spl_vi);
    *strp = buf;
    MAT4X3PNT(*pt, mat, mpt);
}

// I think this is bspline only??
void
ecmd_vtrans(struct mged_state *s)
{
    /* translate a vertex */
    es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
    es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
    es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
    bot_verts[0] = -1;
    bot_verts[1] = -1;
    bot_verts[2] = -1;
    if (es_mvalid) {
	/* Mouse parameter:  new position in model space */
	VMOVE(es_para, es_mparam);
	inpara = 1;
    }
    if (inpara) {


	/* Keyboard parameter:  new position in model space.
	 * XXX for now, splines only here */
	struct rt_nurb_internal *sip =
	    (struct rt_nurb_internal *) s->edit_state.es_int.idb_ptr;
	struct face_g_snurb *surf;
	fastf_t *fp;

	RT_NURB_CK_MAGIC(sip);
	surf = sip->srfs[spl_surfno];
	NMG_CK_SNURB(surf);
	fp = &RT_NURB_GET_CONTROL_POINT(surf, spl_ui, spl_vi);
	if (mged_variables->mv_context) {
	    /* apply es_invmat to convert to real model space */
	    MAT4X3PNT(fp, es_invmat, es_para);
	} else {
	    VMOVE(fp, es_para);
	}
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
