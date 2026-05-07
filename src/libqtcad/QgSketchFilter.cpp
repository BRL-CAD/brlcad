/*                 Q G S K E T C H F I L T E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file QgSketchFilter.cpp
 *
 * Qt mouse-event filter implementations for interactive 2-D sketch
 * editing via the librt ECMD_SKETCH_* API.
 */

#include "common.h"

#include <math.h>

extern "C" {
#include "bu/malloc.h"
#include "bv/util.h"
#include "raytrace.h"
#include "rt/functab.h"
#include "rt/geom.h"
#include "rt/primitives/sketch.h"
#include "rt/rt_ecmds.h"
}

#include "qtcad/QgSketchFilter.h"
#include "qtcad/QgSignalFlags.h"


/* ------------------------------------------------------------------ */
/* QgSketchFilter — base helpers                                       */
/* ------------------------------------------------------------------ */

QMouseEvent *
QgSketchFilter::view_sync(QEvent *e)
{
    if (!v)
	return NULL;

    QMouseEvent *m_e = NULL;
    if (e->type() == QEvent::MouseButtonPress
	    || e->type() == QEvent::MouseButtonRelease
	    || e->type() == QEvent::MouseButtonDblClick
	    || e->type() == QEvent::MouseMove)
	m_e = (QMouseEvent *)e;
    if (!m_e)
	return NULL;

    int e_x, e_y;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    e_x = m_e->x();
    e_y = m_e->y();
#else
    e_x = (int)m_e->position().x();
    e_y = (int)m_e->position().y();
#endif

    v->gv_prevMouseX = v->gv_mouse_x;
    v->gv_prevMouseY = v->gv_mouse_y;
    v->gv_mouse_x = e_x;
    v->gv_mouse_y = e_y;
    bv_screen_pt(&v->gv_point, (fastf_t)e_x, (fastf_t)e_y, v);

    /* Keyboard modifiers usually mean view navigation rather than editing. */
    if (m_e->modifiers() != Qt::NoModifier)
	return NULL;

    return m_e;
}

void
QgSketchFilter::screen_to_view(int sx, int sy, vect_t mvec) const
{
    if (!v) {
	VSETALL(mvec, 0.0);
	return;
    }
    fastf_t vx = 0.0, vy = 0.0;
    bv_screen_to_view(v, &vx, &vy, (fastf_t)sx, (fastf_t)sy);
    VSET(mvec, vx, vy, 0.0);
}

bool
QgSketchFilter::screen_to_uv(int sx, int sy,
			     fastf_t *u_out, fastf_t *v_out) const
{
    if (!v || !es)
	return false;

    const struct rt_sketch_internal *skt =
	(const struct rt_sketch_internal *)es->es_int.idb_ptr;
    if (!skt)
	return false;
    RT_SKETCH_CK_MAGIC(skt);

    /* Unproject screen pixel → model-space 3-D point */
    point_t p3d;
    if (bv_screen_pt(&p3d, (fastf_t)sx, (fastf_t)sy, v) != 0)
	return false;

    /* Project the 3-D point onto the sketch plane.
     * u_vec and v_vec are unit vectors, so the projection is simply
     * the dot product with the offset from the sketch origin V. */
    vect_t delta;
    VSUB2(delta, p3d, skt->V);
    *u_out = VDOT(delta, skt->u_vec);
    *v_out = VDOT(delta, skt->v_vec);
    return true;
}

bool
QgSketchFilter::snap_vertex_uv(int sx, int sy,
				fastf_t *u_out, fastf_t *v_out,
				fastf_t snap_px,
				int *snapped_idx) const
{
    if (snapped_idx)
	*snapped_idx = -1;

    if (!screen_to_uv(sx, sy, u_out, v_out))
	return false;

    if (snap_px <= 0.0 || !es || !v)
	return true;

    const struct rt_sketch_internal *skt =
	(const struct rt_sketch_internal *)es->es_int.idb_ptr;
    if (!skt || skt->vert_count == 0)
	return true;

    /* Model→view transform (including any edit transform) */
    mat_t m2v;
    bn_mat_mul(m2v, v->gv_model2view, es->model_changes);

    /* Cursor in view space */
    vect_t cursor_v;
    screen_to_view(sx, sy, cursor_v);

    /* Convert snap_px to view-space units.
     * One pixel = 2 / gv_width view units in X. */
    fastf_t px_to_view = (v->gv_width > 0)
	? (2.0 / (fastf_t)v->gv_width)
	: 0.005;
    fastf_t threshold2 = (snap_px * px_to_view) * (snap_px * px_to_view);

    fastf_t best_d2  = threshold2;
    int     best_vi  = -1;

    for (size_t i = 0; i < skt->vert_count; i++) {
	point_t p3d;
	VSET(p3d, skt->verts[i][0], skt->verts[i][1], 0.0);
	/* 3-D → model space (sketch coords already are model-space) */
	vect_t p3d_model;
	VJOIN1(p3d_model, skt->V, 1.0, p3d);
	/* Transform to view space */
	point_t p_view;
	MAT4X3PNT(p_view, m2v, p3d_model);

	fastf_t dx = p_view[X] - cursor_v[X];
	fastf_t dy = p_view[Y] - cursor_v[Y];
	fastf_t d2 = dx*dx + dy*dy;
	if (d2 < best_d2) {
	    best_d2 = d2;
	    best_vi = (int)i;
	}
    }

    if (best_vi >= 0) {
	*u_out = skt->verts[best_vi][0];
	*v_out = skt->verts[best_vi][1];
	if (snapped_idx)
	    *snapped_idx = best_vi;
    }

    return true;
}


/* ------------------------------------------------------------------ */
/* QgSketchPickVertexFilter                                            */
/* ------------------------------------------------------------------ */

bool
QgSketchPickVertexFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    if (!es)
	return false;

    /* Act on left button press only */
    if (m_e->type() == QEvent::MouseButtonPress
	    && m_e->buttons().testFlag(Qt::LeftButton)) {

	const struct rt_sketch_internal *skt =
	    (const struct rt_sketch_internal *)es->es_int.idb_ptr;
	if (!skt || skt->vert_count == 0)
	    return true;

	/* Store view-space cursor; edsketch.c's proximity search will use it */
	struct rt_sketch_edit *se = (struct rt_sketch_edit *)es->ipe_ptr;
	if (!se)
	    return true;

	vect_t mvec;
	screen_to_view(v->gv_mouse_x, v->gv_mouse_y, mvec);
	VSET(se->v_pos, mvec[X], mvec[Y], 0.0);
	se->v_pos_valid = 1;

	es->e_inpara = 0;
	EDOBJ[es->es_int.idb_type].ft_set_edit_mode(es,
		ECMD_SKETCH_PICK_VERTEX);
	rt_edit_process(es);

	emit sketch_changed();
	emit view_updated(QG_VIEW_REFRESH);
	return true;
    }

    /* Swallow other button events while this filter is active */
    if (m_e->type() == QEvent::MouseButtonPress
	    || m_e->type() == QEvent::MouseButtonRelease)
	return true;

    return false;
}


/* ------------------------------------------------------------------ */
/* QgSketchMoveVertexFilter                                            */
/* ------------------------------------------------------------------ */

bool
QgSketchMoveVertexFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    if (!es)
	return false;

    struct rt_sketch_edit *se = (struct rt_sketch_edit *)es->ipe_ptr;
    if (!se || se->curr_vert < 0)
	return false;

    if (m_e->type() == QEvent::MouseButtonPress
	    && m_e->buttons().testFlag(Qt::LeftButton)) {
	m_dragging = true;
	return true;
    }

    if (m_e->type() == QEvent::MouseButtonRelease) {
	m_dragging = false;
	return true;
    }

    if (m_e->type() == QEvent::MouseMove && m_dragging
	    && m_e->buttons().testFlag(Qt::LeftButton)) {

	fastf_t u = 0.0, vv = 0.0;
	if (!screen_to_uv(v->gv_mouse_x, v->gv_mouse_y, &u, &vv))
	    return true;

	/* Convert base units → local units */
	fastf_t scale = (es->local2base > 0.0) ? (1.0 / es->local2base) : 1.0;
	es->e_para[0] = u * scale;
	es->e_para[1] = vv * scale;
	es->e_inpara  = 2;

	EDOBJ[es->es_int.idb_type].ft_set_edit_mode(es,
		ECMD_SKETCH_MOVE_VERTEX);
	rt_edit_process(es);

	emit sketch_changed();
	emit view_updated(QG_VIEW_REFRESH);
	return true;
    }

    return false;
}


/* ------------------------------------------------------------------ */
/* QgSketchAddVertexFilter                                             */
/* ------------------------------------------------------------------ */

bool
QgSketchAddVertexFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    if (!es)
	return false;

    if (m_e->type() == QEvent::MouseButtonPress
	    && m_e->buttons().testFlag(Qt::LeftButton)) {

	fastf_t u = 0.0, vv = 0.0;
	/* Use snap_vertex_uv so the new vertex can snap to an existing one */
	if (!snap_vertex_uv(v->gv_mouse_x, v->gv_mouse_y, &u, &vv))
	    return true;

	/* Convert base units → local units */
	fastf_t scale = (es->local2base > 0.0) ? (1.0 / es->local2base) : 1.0;
	es->e_para[0] = u * scale;
	es->e_para[1] = vv * scale;
	es->e_inpara  = 2;

	EDOBJ[es->es_int.idb_type].ft_set_edit_mode(es,
		ECMD_SKETCH_ADD_VERTEX);
	rt_edit_process(es);

	emit sketch_changed();
	emit view_updated(QG_VIEW_REFRESH);
	return true;
    }

    /* Swallow other button events */
    if (m_e->type() == QEvent::MouseButtonPress
	    || m_e->type() == QEvent::MouseButtonRelease)
	return true;

    return false;
}


/* ------------------------------------------------------------------ */
/* QgSketchPickSegmentFilter                                           */
/* ------------------------------------------------------------------ */

/*
 * Sample a segment at t in [0,1] and return the 3-D model-space point.
 * Returns false for unsupported segment types.
 */
static bool
sketch_seg_sample(const struct rt_sketch_internal *skt,
		  int seg_idx, fastf_t t,
		  point_t *p3d_out)
{
    void *seg = skt->curve.segment[seg_idx];
    if (!seg)
	return false;

    uint32_t magic = *(uint32_t *)seg;

    fastf_t u = 0.0, vv = 0.0;

    if (magic == CURVE_LSEG_MAGIC) {
	struct line_seg *ls = (struct line_seg *)seg;
	u  = (1.0 - t) * skt->verts[ls->start][0]
	   + t          * skt->verts[ls->end  ][0];
	vv = (1.0 - t) * skt->verts[ls->start][1]
	   + t          * skt->verts[ls->end  ][1];

    } else if (magic == CURVE_CARC_MAGIC) {
	struct carc_seg *cs = (struct carc_seg *)seg;
	if (cs->radius < 0.0) {
	    /* Full circle: parametrise by angle */
	    fastf_t cx = skt->verts[cs->end][0];
	    fastf_t cy = skt->verts[cs->end][1];
	    fastf_t r  = -cs->radius;
	    fastf_t theta = t * 2.0 * M_PI;
	    u  = cx + r * cos(theta);
	    vv = cy + r * sin(theta);
	} else {
	    /* Partial arc: interpolate between start and end angles */
	    fastf_t sx = skt->verts[cs->start][0];
	    fastf_t sy = skt->verts[cs->start][1];
	    fastf_t ex = skt->verts[cs->end  ][0];
	    fastf_t ey = skt->verts[cs->end  ][1];
	    u  = (1.0 - t) * sx + t * ex;
	    vv = (1.0 - t) * sy + t * ey;
	}

    } else if (magic == CURVE_BEZIER_MAGIC) {
	struct bezier_seg *bs = (struct bezier_seg *)seg;
	int deg = bs->degree;

	/* Guard against pathologically high degree (RT_EDIT_MAXPARA - 1 = 19) */
	if (deg >= RT_EDIT_MAXPARA)
	    return false;

	/* De Casteljau evaluation — RT_EDIT_MAXPARA slots is always sufficient */
	fastf_t pu[RT_EDIT_MAXPARA], pv[RT_EDIT_MAXPARA];
	for (int i = 0; i <= deg; i++) {
	    pu[i] = skt->verts[bs->ctl_points[i]][0];
	    pv[i] = skt->verts[bs->ctl_points[i]][1];
	}
	for (int r = 1; r <= deg; r++) {
	    for (int i = 0; i <= deg - r; i++) {
		pu[i] = (1.0 - t) * pu[i] + t * pu[i + 1];
		pv[i] = (1.0 - t) * pv[i] + t * pv[i + 1];
	    }
	}
	u  = pu[0];
	vv = pv[0];

    } else {
	return false; /* NURB: not sampled for proximity */
    }

    VJOIN2(*p3d_out, skt->V,
	   u,  skt->u_vec,
	   vv, skt->v_vec);
    return true;
}

bool
QgSketchPickSegmentFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    if (!es)
	return false;

    if (m_e->type() == QEvent::MouseButtonPress
	    && m_e->buttons().testFlag(Qt::LeftButton)) {

	const struct rt_sketch_internal *skt =
	    (const struct rt_sketch_internal *)es->es_int.idb_ptr;
	if (!skt || skt->curve.count == 0)
	    return true;

	/* Build model→view matrix including any edit transform */
	mat_t m2v;
	bn_mat_mul(m2v, v->gv_model2view, es->model_changes);

	/* Cursor in view space */
	vect_t cursor_v;
	screen_to_view(v->gv_mouse_x, v->gv_mouse_y, cursor_v);

	int    best_seg  = -1;
	fastf_t best_d2  = INFINITY;
	int    nsamples  = 16;

	for (size_t si = 0; si < skt->curve.count; si++) {
	    for (int k = 0; k <= nsamples; k++) {
		fastf_t t = (fastf_t)k / (fastf_t)nsamples;
		point_t p3d;
		if (!sketch_seg_sample(skt, (int)si, t, &p3d))
		    continue;

		point_t p_view;
		MAT4X3PNT(p_view, m2v, p3d);

		fastf_t dx = p_view[X] - cursor_v[X];
		fastf_t dy = p_view[Y] - cursor_v[Y];
		fastf_t d2 = dx * dx + dy * dy;
		if (d2 < best_d2) {
		    best_d2  = d2;
		    best_seg = (int)si;
		}
	    }
	}

	if (best_seg < 0)
	    return true;

	es->e_para[0] = (fastf_t)best_seg;
	es->e_inpara  = 1;
	EDOBJ[es->es_int.idb_type].ft_set_edit_mode(es,
		ECMD_SKETCH_PICK_SEGMENT);
	rt_edit_process(es);

	emit sketch_changed();
	emit view_updated(QG_VIEW_REFRESH);
	return true;
    }

    if (m_e->type() == QEvent::MouseButtonPress
	    || m_e->type() == QEvent::MouseButtonRelease)
	return true;

    return false;
}


/* ------------------------------------------------------------------ */
/* QgSketchMoveSegmentFilter                                           */
/* ------------------------------------------------------------------ */

bool
QgSketchMoveSegmentFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    if (!es)
	return false;

    struct rt_sketch_edit *se = (struct rt_sketch_edit *)es->ipe_ptr;
    if (!se || se->curr_seg < 0)
	return false;

    if (m_e->type() == QEvent::MouseButtonPress
	    && m_e->buttons().testFlag(Qt::LeftButton)) {
	/* Record starting UV position */
	screen_to_uv(v->gv_mouse_x, v->gv_mouse_y, &m_prev_u, &m_prev_v);
	m_dragging = true;
	return true;
    }

    if (m_e->type() == QEvent::MouseButtonRelease) {
	m_dragging = false;
	return true;
    }

    if (m_e->type() == QEvent::MouseMove && m_dragging
	    && m_e->buttons().testFlag(Qt::LeftButton)) {

	fastf_t cur_u = 0.0, cur_v = 0.0;
	if (!screen_to_uv(v->gv_mouse_x, v->gv_mouse_y, &cur_u, &cur_v))
	    return true;

	fastf_t du = cur_u - m_prev_u;
	fastf_t dv = cur_v - m_prev_v;

	/* Convert delta from base units → local units */
	fastf_t scale = (es->local2base > 0.0) ? (1.0 / es->local2base) : 1.0;
	es->e_para[0] = du * scale;
	es->e_para[1] = dv * scale;
	es->e_inpara  = 2;

	EDOBJ[es->es_int.idb_type].ft_set_edit_mode(es,
		ECMD_SKETCH_MOVE_SEGMENT);
	rt_edit_process(es);

	m_prev_u = cur_u;
	m_prev_v = cur_v;

	emit sketch_changed();
	emit view_updated(QG_VIEW_REFRESH);
	return true;
    }

    return false;
}




/* ------------------------------------------------------------------ */
/* arc centre helper (shared by QgSketchArcRadiusFilter)              */
/* ------------------------------------------------------------------ */

/*
 * Compute the centre of a partial circular arc in sketch UV space.
 * Uses the perpendicular-bisector formula, matching sketch.c and
 * edsketch.c's split-segment code.
 *
 * Returns true on success; false if the geometry is degenerate (start
 * equals end, or the radius is too small for the chord length).
 * On success, *cu_out and *cv_out hold the centre coordinates in model
 * base units (the same units as skt->verts[]).
 */
static bool
sketch_arc_center_uv(const struct rt_sketch_internal *skt,
		     const struct carc_seg *cs,
		     fastf_t *cu_out, fastf_t *cv_out)
{
    fastf_t sx = skt->verts[cs->start][0];
    fastf_t sy = skt->verts[cs->start][1];
    fastf_t ex = skt->verts[cs->end  ][0];
    fastf_t ey = skt->verts[cs->end  ][1];
    fastf_t r  = cs->radius;  /* positive for partial arc */

    fastf_t midx = (sx + ex) * 0.5;
    fastf_t midy = (sy + ey) * 0.5;
    fastf_t s2mx = midx - sx;
    fastf_t s2my = midy - sy;
    fastf_t s2m_len_sq = s2mx*s2mx + s2my*s2my;
    if (s2m_len_sq <= SMALL_FASTF)
	return false;

    fastf_t len_sq = r*r - s2m_len_sq;
    if (len_sq < 0.0)
	len_sq = 0.0;  /* clamp: arc just barely reaches midpoint */

    /* Perpendicular direction to the start→end chord */
    fastf_t dirx = -s2my;
    fastf_t diry =  s2mx;
    fastf_t dir_len = sqrt(dirx*dirx + diry*diry);
    dirx /= dir_len;
    diry /= dir_len;

    fastf_t tmp_len = sqrt(len_sq);
    fastf_t cx = midx + tmp_len * dirx;
    fastf_t cy = midy + tmp_len * diry;

    /* Choose the side matching center_is_left.
     * When the initial centre (midx + tmp_len*dir) lies to the LEFT of the
     * directed start→end chord, the cross product z-component is positive.
     * If that left-side initial centre does NOT match center_is_left, flip
     * to the other perpendicular-bisector root (midx - tmp_len*dir).
     * This replicates the logic in edsketch.c::ecmd_sketch_split_segment. */
    fastf_t cross_z = (ex - sx)*(cy - sy) - (ey - sy)*(cx - sx);
    if ((cross_z <= 0.0) || !cs->center_is_left) {
	cx = midx - tmp_len * dirx;
	cy = midy - tmp_len * diry;
    }

    *cu_out = cx;
    *cv_out = cy;
    return true;
}


/* ------------------------------------------------------------------ */
/* QgSketchArcRadiusFilter                                            */
/* ------------------------------------------------------------------ */

bool
QgSketchArcRadiusFilter::eventFilter(QObject *, QEvent *e)
{
    if (!es || !v)
	return false;

    /* Extract mouse coordinates directly — bypass view_sync so that
     * modifier keys (Shift for multi-select in other filters) do not
     * block the drag. */
    QMouseEvent *m_e = NULL;
    if (e->type() == QEvent::MouseButtonPress
	    || e->type() == QEvent::MouseButtonRelease
	    || e->type() == QEvent::MouseMove)
	m_e = (QMouseEvent *)e;
    if (!m_e)
	return false;

    int sx, sy;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    sx = m_e->x();
    sy = m_e->y();
#else
    sx = (int)m_e->position().x();
    sy = (int)m_e->position().y();
#endif

    struct rt_sketch_edit *se = (struct rt_sketch_edit *)es->ipe_ptr;
    if (!se || se->curr_seg < 0)
	return false;

    const struct rt_sketch_internal *skt =
	(const struct rt_sketch_internal *)es->es_int.idb_ptr;
    if (!skt)
	return false;

    void *seg = skt->curve.segment[se->curr_seg];
    if (!seg || *(uint32_t *)seg != CURVE_CARC_MAGIC)
	return false;

    struct carc_seg *cs = (struct carc_seg *)seg;

    if (m_e->type() == QEvent::MouseButtonPress
	    && m_e->buttons().testFlag(Qt::LeftButton)) {

	if (cs->radius < 0.0) {
	    /* Full circle: centre is the end vertex */
	    m_center_u  = skt->verts[cs->end][0];
	    m_center_v  = skt->verts[cs->end][1];
	    m_full_circle = true;
	    m_dragging  = true;
	} else {
	    /* Partial arc: compute centre via perpendicular bisector */
	    m_full_circle = false;
	    m_dragging = sketch_arc_center_uv(skt, cs, &m_center_u, &m_center_v);
	}
	return true;
    }

    if (m_e->type() == QEvent::MouseButtonRelease) {
	m_dragging = false;
	return true;
    }

    if (m_e->type() == QEvent::MouseMove && m_dragging
	    && m_e->buttons().testFlag(Qt::LeftButton)) {

	fastf_t u = 0.0, vv = 0.0;
	if (!screen_to_uv(sx, sy, &u, &vv))
	    return true;

	/* New radius = distance from cursor to stored arc centre */
	fastf_t du = u  - m_center_u;
	fastf_t dv = vv - m_center_v;
	fastf_t new_r = sqrt(du*du + dv*dv);

	if (new_r < SMALL_FASTF)
	    return true;

	/* Preserve full-circle sign convention */
	if (m_full_circle)
	    new_r = -new_r;

	/* Convert base units → local units for e_para */
	fastf_t scale = (es->local2base > 0.0) ? (1.0 / es->local2base) : 1.0;
	es->e_para[0] = new_r * scale;
	es->e_inpara  = 1;

	EDOBJ[es->es_int.idb_type].ft_set_edit_mode(es,
		ECMD_SKETCH_SET_ARC_RADIUS);
	rt_edit_process(es);

	emit sketch_changed();
	emit view_updated(QG_VIEW_REFRESH);
	return true;
    }

    return false;
}


/* ------------------------------------------------------------------ */
/* QgSketchCursorTracker                                              */
/* ------------------------------------------------------------------ */

bool
QgSketchCursorTracker::eventFilter(QObject *, QEvent *e)
{
    if (e->type() != QEvent::MouseMove)
	return false;

    QMouseEvent *m_e = (QMouseEvent *)e;

    int sx, sy;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    sx = m_e->x();
    sy = m_e->y();
#else
    sx = (int)m_e->position().x();
    sy = (int)m_e->position().y();
#endif

    fastf_t u = 0.0, vv = 0.0;
    if (screen_to_uv(sx, sy, &u, &vv))
	emit uv_moved((double)u, (double)vv);

    return false;  /* never consume events */
}


/* ------------------------------------------------------------------ */
/* QgSketchSetTangencyFilter                                          */
/* ------------------------------------------------------------------ */

bool
QgSketchSetTangencyFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    if (!es)
	return false;

    if (m_e->type() == QEvent::MouseButtonPress
	    && m_e->buttons().testFlag(Qt::LeftButton)) {

	const struct rt_sketch_internal *skt =
	    (const struct rt_sketch_internal *)es->es_int.idb_ptr;
	if (!skt || skt->curve.count == 0)
	    return true;

	/* Build model→view matrix */
	mat_t m2v;
	bn_mat_mul(m2v, v->gv_model2view, es->model_changes);

	/* Cursor in view space */
	vect_t cursor_v;
	screen_to_view(v->gv_mouse_x, v->gv_mouse_y, cursor_v);

	int    best_seg = -1;
	fastf_t best_d2 = INFINITY;
	int nsamples    = 16;

	for (size_t si = 0; si < skt->curve.count; si++) {
	    /* skip the currently selected CARC itself */
	    struct rt_sketch_edit *se =
		(struct rt_sketch_edit *)es->ipe_ptr;
	    if ((int)si == se->curr_seg)
		continue;

	    for (int k = 0; k <= nsamples; k++) {
		fastf_t t = (fastf_t)k / (fastf_t)nsamples;
		point_t p3d;
		if (!sketch_seg_sample(skt, (int)si, t, &p3d))
		    continue;
		point_t p_view;
		MAT4X3PNT(p_view, m2v, p3d);
		fastf_t dx = p_view[X] - cursor_v[X];
		fastf_t dy = p_view[Y] - cursor_v[Y];
		fastf_t d2 = dx*dx + dy*dy;
		if (d2 < best_d2) {
		    best_d2  = d2;
		    best_seg = (int)si;
		}
	    }
	}

	if (best_seg < 0)
	    return true;

	rt_edit_checkpoint(es);
	es->e_para[0] = (fastf_t)best_seg;
	es->e_para[1] = tangency_angle;
	es->e_inpara  = 2;
	EDOBJ[es->es_int.idb_type].ft_set_edit_mode(es,
		ECMD_SKETCH_SET_TANGENCY);
	rt_edit_process(es);

	emit sketch_changed();
	emit view_updated(QG_VIEW_REFRESH);
	return true;
    }

    if (m_e->type() == QEvent::MouseButtonPress
	    || m_e->type() == QEvent::MouseButtonRelease)
	return true;

    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
