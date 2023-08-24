/*                 Q G M E A S U R E F I L T E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2023 United States Government as represented by
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
/** @file QgMeasureFilter.cpp
 *
 * Measurement tool for Qt views.
 *
 */

#include "common.h"

extern "C" {
#include "bu/malloc.h"
#include "bg/lod.h"
#include "bg/polygon.h"
#include "bv.h"
#include "raytrace.h"
}

#include "qtcad/QgMeasureFilter.h"
#include "qtcad/QgSignalFlags.h"

QMouseEvent *
QgMeasureFilter::view_sync(QEvent *e)
{
    if (!v)
	return NULL;

    // If we don't have one of the relevant mouse operations, there's nothing to do
    QMouseEvent *m_e = NULL;
    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease || e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove)
	m_e = (QMouseEvent *)e;
    if (!m_e)
	return NULL;

    // We're going to need the mouse position
    int e_x, e_y;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    e_x = m_e->x();
    e_y = m_e->y();
#else
    e_x = m_e->position().x();
    e_y = m_e->position().y();
#endif

    // Update relevant bview variables
    v->gv_prevMouseX = v->gv_mouse_x;
    v->gv_prevMouseY = v->gv_mouse_y;
    v->gv_mouse_x = e_x;
    v->gv_mouse_y = e_y;

    // If we have modifiers, we're most likely doing shift grips
    if (m_e->modifiers() != Qt::NoModifier)
	return NULL;

    return m_e;
}

double
QgMeasureFilter::length1()
{
    return DIST_PNT_PNT(p1, p2);
}

double
QgMeasureFilter::length2()
{
    if (mode < 3)
	return 0.0;

    return DIST_PNT_PNT(p2, p3);
}

double
QgMeasureFilter::angle(bool radians)
{
    if (mode < 3)
	return 0.0;

    vect_t v1, v2;
    VSUB2(v1, p1, p2);
    VSUB2(v2, p3, p2);
    VUNITIZE(v1);
    VUNITIZE(v2);
    double a = acos(VDOT(v1, v2));
    if (radians)
	return a*180/M_PI;
    return a;
}

void
QgMeasureFilter::update_color(struct bu_color *c)
{
    if (!s || !c)
	return;
    bu_color_to_rgb_chars(c, s->s_color);
}

bool
QgMeasureFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    if (e->type() == QEvent::MouseButtonPress) {
	if (m_e->button() == Qt::RightButton) {
	    if (s)
		bv_obj_put(s);
	    mode = 0;
	    VSETALL(p1, 0.0);
	    VSETALL(p2, 0.0);
	    VSETALL(p3, 0.0);
	    emit view_updated(QG_VIEW_REFRESH);
	    return true;
	}
	if (mode == 4) {
	    if (s)
		bv_obj_put(s);
	    mode = 0;
	    emit view_updated(QG_VIEW_REFRESH);
	    return true;
	}
	if (!mode) {
	    if (!get_point())
		return true;

	    VSETALL(p1, 0.0);
	    VSETALL(p2, 0.0);
	    VSETALL(p3, 0.0);

	    if (s)
		bv_obj_put(s);
	    s = bv_obj_get(v, BV_VIEW_OBJS);

	    mode = 1;
	    VMOVE(p1, mpnt);
	    VMOVE(p2, mpnt);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p1, BV_VLIST_LINE_MOVE);
	    bu_vls_init(&s->s_name);
	    bu_vls_printf(&s->s_name, "%s", oname.c_str());
	    emit view_updated(QG_VIEW_REFRESH);
	    return true;
	}
	if (mode == 1) {
	    if (!get_point())
		return true;

	    VMOVE(p2, mpnt);
	    return true;
	}
	if (mode == 2) {
	    if (!get_point())
		return true;
	    mode = 3;
	    BV_FREE_VLIST(s->vlfree, &s->s_vlist);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p2, BV_VLIST_LINE_DRAW);
	    VMOVE(p3, mpnt);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p3, BV_VLIST_LINE_DRAW);
	    emit view_updated(QG_VIEW_REFRESH);
	}
	return true;
    }

    if (e->type() == QEvent::MouseMove) {
	if (!mode)
	    return false;
	if (mode == 1) {
	    if (!get_point())
		return true;

	    BV_FREE_VLIST(s->vlfree, &s->s_vlist);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p1, BV_VLIST_LINE_MOVE);
	    VMOVE(p2, mpnt);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p2, BV_VLIST_LINE_DRAW);
	    emit view_updated(QG_VIEW_REFRESH);
	}
	if (mode == 3) {
	    if (!get_point())
		return true;

	    BV_FREE_VLIST(s->vlfree, &s->s_vlist);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p2, BV_VLIST_LINE_DRAW);
	    VMOVE(p3, mpnt);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p3, BV_VLIST_LINE_DRAW);
	    emit view_updated(QG_VIEW_REFRESH);
	}
	return true;
    }

    if (e->type() == QEvent::MouseButtonRelease) {
	if (m_e->button() == Qt::RightButton) {
	    mode = 0;
	    if (s) {
		bv_obj_put(s);
		emit view_updated(QG_VIEW_REFRESH);
	    }
	    s = NULL;
	    return true;
	}
	if (!mode)
	    return false;
	if (mode == 1 && DIST_PNT_PNT(p1, p2) < SMALL_FASTF) {
	    return true;
	}
	if (mode == 1) {
	    if (!get_point())
		return true;

	    if (length_only) {
		// Angle measurement disabled, starting over
		mode = 0;
		emit view_updated(QG_VIEW_REFRESH);
		return true;
	    }

	    mode = 2;
	    BV_FREE_VLIST(s->vlfree, &s->s_vlist);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p1, BV_VLIST_LINE_MOVE);
	    VMOVE(p2, mpnt);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p2, BV_VLIST_LINE_DRAW);
	    emit view_updated(QG_VIEW_REFRESH);
	    return true;
	}
	if (mode == 3) {
	    if (!get_point())
		return true;
	    mode = 4;
	    BV_FREE_VLIST(s->vlfree, &s->s_vlist);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p2, BV_VLIST_LINE_DRAW);
	    VMOVE(p3, mpnt);
	    BV_ADD_VLIST(s->vlfree, &s->s_vlist, p3, BV_VLIST_LINE_DRAW);
	    emit view_updated(QG_VIEW_REFRESH);
	    return true;
	}

	return true;
    }

    // Shouldn't get here
    return false;
}

bool
QMeasure2DFilter::get_point()
{
    fastf_t vx, vy;
    bv_screen_to_view(v, &vx, &vy, v->gv_mouse_x, v->gv_mouse_y);
    point_t vpnt;
    VSET(vpnt, vx, vy, 0);
    MAT4X3PNT(mpnt, v->gv_view2model, vpnt);
    return true;
}

bool
QMeasure2DFilter::eventFilter(QObject *o, QEvent *e)
{
    return QgMeasureFilter::eventFilter(o, e);
}

QMeasure3DFilter::QMeasure3DFilter()
{
}

QMeasure3DFilter::~QMeasure3DFilter()
{
    bu_ptbl_free(&scene_obj_set);
}

struct measure_rec_state {
    double cdist;
    point_t pt;
};

static int
_cpnt_hit(struct application *ap, struct partition *p_hp, struct seg *UNUSED(segs))
{
    struct measure_rec_state *rc = (struct measure_rec_state *)ap->a_uptr;
    for (struct partition *pp = p_hp->pt_forw; pp != p_hp; pp = pp->pt_forw) {
	struct hit *hitp = pp->pt_inhit;
	if (hitp->hit_dist < rc->cdist) {
	    rc->cdist = hitp->hit_dist;
	    VJOIN1(rc->pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
	}
    }
    return 1;
}

static int
_cpnt_ovlp(struct application *ap, struct partition *pp, struct region *UNUSED(reg1), struct region *UNUSED(reg2), struct partition *UNUSED(ihp))
{
    struct measure_rec_state *rc = (struct measure_rec_state *)ap->a_uptr;
    struct hit *hitp = pp->pt_inhit;
    rc->cdist = hitp->hit_dist;
    VJOIN1(rc->pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
    return 1;
}

bool
QMeasure3DFilter::get_point()
{
    if (!dbip)
	return false;

    fastf_t vx, vy;
    bv_screen_to_view(v, &vx, &vy, v->gv_mouse_x, v->gv_mouse_y);
    point_t vpnt;
    VSET(vpnt, vx, vy, 0);
    MAT4X3PNT(mpnt, v->gv_view2model, vpnt);

    // With this filter we want a 3D point based on scene geometry (hard case)
    // - need to interrogate the scene with the raytracer.
    //
    // Rather than prepping the whole of what is drawn, we will instead prep
    // only those objects whose bounding boxes are currently under the mouse.
    // Under most circumstances that should substantially cut down the
    // interrogation time for large models.
    struct bu_ptbl sset = BU_PTBL_INIT_ZERO;
    int scnt = bg_view_objs_select(&sset, v, v->gv_mouse_x, v->gv_mouse_y);

    // If we didn't see anything, we have a no-op
    if (!scnt) {
	prev_cnt = scnt;
	bu_ptbl_free(&sset);
	return false;
    }

    bool need_prep = (!ap || !rtip || !resp) ? true : false;
    if (need_prep || prev_cnt != scnt || scnt != (int)BU_PTBL_LEN(&scene_obj_set)) {
	// Something changed - need to reset the raytrace data
	bu_ptbl_reset(&scene_obj_set);
	bu_ptbl_cat(&scene_obj_set, &sset);
	need_prep = true;
    }
    if (!need_prep) {
	// We may be able to reuse the existing prep - make sure the scene obj
	// sets match.  The above check should ensure that the lengths of sset
	// and scene_obj_set match - if they don't, we already know we need to
	// re-prep.
	for (size_t i = 0; i < BU_PTBL_LEN(&scene_obj_set); i++) {
	    if (BU_PTBL_GET(&sset, i) != BU_PTBL_GET(&scene_obj_set, i)) {
		need_prep = true;
		break;
	    }
	}
    }

    prev_cnt = scnt;

    if (need_prep) {
	if (!ap) {
	    BU_GET(ap, struct application);
	    RT_APPLICATION_INIT(ap);
	    ap->a_onehit = 1;
	    ap->a_hit = _cpnt_hit;
	    ap->a_miss = NULL;
	    ap->a_overlap = _cpnt_ovlp;
	    ap->a_logoverlap = NULL;
	}
	if (rtip) {
	    rt_free_rti(rtip);
	    rtip = NULL;
	}
	rtip = rt_new_rti(dbip);
	if (resp) {
	    // TODO - do we need more here?
	    BU_PUT(resp, struct resource);
	}
	BU_GET(resp, struct resource);
	rt_init_resource(resp, 0, rtip);
	ap->a_resource = resp;
	ap->a_rt_i = rtip;

	const char **objs = (const char **)bu_calloc(BU_PTBL_LEN(&scene_obj_set) + 1, sizeof(char *), "objs");
	for (size_t i = 0; i < BU_PTBL_LEN(&scene_obj_set); i++) {
	    struct bv_scene_obj *l_s = (struct bv_scene_obj *)BU_PTBL_GET(&scene_obj_set, i);
	    objs[i] = bu_vls_cstr(&l_s->s_name);
	}
	if (rt_gettrees_and_attrs(rtip, NULL, scnt, objs, 1)) {
	    bu_free(objs, "objs");
	    rt_free_rti(rtip);
	    rtip = NULL;
	    BU_PUT(resp, struct resource);
	    resp = NULL;
	    bu_ptbl_free(&sset);
	    return false;
	}
	size_t ncpus = bu_avail_cpus();
	rt_prep_parallel(rtip, (int)ncpus);
	bu_free(objs, "objs");
    }

    bu_ptbl_free(&sset);

    // Set up data container for result
    struct measure_rec_state rc;
    rc.cdist = INFINITY;
    ap->a_uptr = (void *)&rc;

    // Set up the ray itself
    vect_t dir;
    VMOVEN(dir, v->gv_rotation + 8, 3);
    VUNITIZE(dir);
    VSCALE(dir, dir, v->radius);
    VADD2(ap->a_ray.r_pt, mpnt, dir);
    VUNITIZE(dir);
    VSCALE(ap->a_ray.r_dir, dir, -1);

    (void)rt_shootray(ap);

    if (rc.cdist < INFINITY) {
	VMOVE(mpnt, rc.pt);
	return true;
    }

    return false;
}

bool
QMeasure3DFilter::eventFilter(QObject *o, QEvent *e)
{
    return QgMeasureFilter::eventFilter(o, e);
}




// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
