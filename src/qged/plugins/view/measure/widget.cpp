/*                 V I E W _ W I D G E T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file view_widget.cpp
 *
 */

#include "common.h"
#include <QMouseEvent>
#include <QVBoxLayout>
#include "../../../app.h"

#include "bu/opt.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bg/aabb_ray.h"
#include "bg/plane.h"
#include "bg/lod.h"

#include "./widget.h"

CADViewMeasure::CADViewMeasure(QWidget *)
{
    QVBoxLayout *wl = new QVBoxLayout;
    wl->setAlignment(Qt::AlignTop);

    measure_3d = new QCheckBox("Use 3D hit points");
    wl->addWidget(measure_3d);

    QLabel *ml1_label = new QLabel("Measured Length #1:");
    length1_report = new QLineEdit();
    length1_report->setReadOnly(true);
    wl->addWidget(ml1_label);
    wl->addWidget(length1_report);

    QLabel *ml2_label = new QLabel("Measured Length #2:");
    length2_report = new QLineEdit();
    length2_report->setReadOnly(true);
    wl->addWidget(ml2_label);
    wl->addWidget(length2_report);

    report_radians = new QCheckBox("Report angle in radians");
    wl->addWidget(report_radians);
    QObject::connect(report_radians, &QCheckBox::stateChanged, this, &CADViewMeasure::adjust_text);

    ma_label = new QLabel("Measured Angle (deg):");
    angle_report = new QLineEdit();
    angle_report->setReadOnly(true);
    wl->addWidget(ma_label);
    wl->addWidget(angle_report);

    color_2d = new QColorRGB(this, "2D:", QColor(Qt::yellow));
    wl->addWidget(color_2d);
    color_3d = new QColorRGB(this, "3D:", QColor(Qt::green));
    wl->addWidget(color_3d);
    QObject::connect(color_2d, &QColorRGB::color_changed, this, &CADViewMeasure::update_color);
    QObject::connect(color_3d, &QColorRGB::color_changed, this, &CADViewMeasure::update_color);

    this->setLayout(wl);
}

CADViewMeasure::~CADViewMeasure()
{
    bu_vls_free(&buffer);
    if (s)
	bv_obj_put(s);
}

struct rec_state {
    double cdist;
    point_t pt;
};

static int
_cpnt_hit(struct application *ap, struct partition *p_hp, struct seg *UNUSED(segs))
{
    struct rec_state *rc = (struct rec_state *)ap->a_uptr;
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
    struct rec_state *rc = (struct rec_state *)ap->a_uptr;
    struct hit *hitp = pp->pt_inhit;
    rc->cdist = hitp->hit_dist;
    VJOIN1(rc->pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
    return 1;
}


bool
CADViewMeasure::get_point(QMouseEvent *m_e)
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return false;
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return false;

    struct bview *v = gedp->ged_gvp;

    fastf_t vx, vy;
#ifdef USE_QT6
    vx = m_e->position().x();
    vy = m_e->position().y();
#else
    vx = m_e->x();
    vy = m_e->y();
#endif
    int x = (int)vx;
    int y = (int)vy;
    bv_screen_to_view(v, &vx, &vy, x, y);
    point_t vpnt;
    VSET(vpnt, vx, vy, 0);
    MAT4X3PNT(mpnt, v->gv_view2model, vpnt);

    if (!measure_3d->isChecked()) {
	// 2D case is straightforward - just use mpnt
	return true;
    }

    // Want a 3D point (hard case) - need the raytracer.
    //
    // NOTE:  This is an experiment - rather than prepping
    // the whole of what is drawn, we will instead prep only those objects
    // whose bounding boxes are currently under the mouse.  This may not be
    // viable if we need to do too many preps of different sets that are slow
    // to prep, but on the flip side we will be dealing with fewer objects
    // (which means less memory consumption and possibly avoiding a very
    // lengthy initial setup.)  If this doesn't work, we may need to just go
    // ahead and prep the whole who list, despite any up-front cost...
    struct bv_scene_obj **sset = NULL;
    int scnt = bg_view_objs_select(&sset, v, x, y);

    // If we didn't see anything, we have a no-op
    if (!scnt) {
	prev_cnt = scnt;
	return false;
    }

    bool need_prep = (!ap || !rtip || !resp) ? true : false;
    if (need_prep || !scene_obj_set_cnt || prev_cnt != scnt || scnt != scene_obj_set_cnt) {
	// Something changed - need to reset the raytrace data
	if (scene_obj_set)
	    bu_free(scene_obj_set, "old set");
	scene_obj_set_cnt = scnt;
	scene_obj_set = sset;
	need_prep = true;
    }
    if (!need_prep) {
	// We may be able to reuse the existing prep - make
	// sure the scene obj sets match
	for (int i = 0; i < scene_obj_set_cnt; i++) {
	    if (sset[i] != scene_obj_set[i]) {
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
	rtip = rt_new_rti(gedp->dbip);
	if (resp) {
	    // TODO - do we need more here?
	    BU_PUT(resp, struct resource);
	}
	BU_GET(resp, struct resource);
	rt_init_resource(resp, 0, rtip);
	ap->a_resource = resp;
	ap->a_rt_i = rtip;

	const char **objs = (const char **)bu_calloc(scene_obj_set_cnt + 1, sizeof(char *), "objs");
	for (int i = 0; i < scene_obj_set_cnt; i++) {
	    struct bv_scene_obj *l_s = scene_obj_set[i];
	    objs[i] = bu_vls_cstr(&l_s->s_name);
	}
	if (rt_gettrees_and_attrs(rtip, NULL, scnt, objs, 1)) {
	    bu_free(objs, "objs");
	    rt_free_rti(rtip);
	    rtip = NULL;
	    BU_PUT(resp, struct resource);
	    resp = NULL;
	    return false;
	}
	size_t ncpus = bu_avail_cpus();
	rt_prep_parallel(rtip, (int)ncpus);
	bu_free(objs, "objs");
    } else {
	bu_free(sset, "unneeded new set");
    }

    // Set up data container for result
    struct rec_state rc;
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

void
CADViewMeasure::update_color()
{
    if (!s)
	return;
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return;

    if (!measure_3d->isChecked()) {
	bu_color_to_rgb_chars(&color_2d->bc, s->s_color);
	emit view_updated(QTCAD_VIEW_REFRESH);
	return;
    }

    if (measure_3d->isChecked()) {
	bu_color_to_rgb_chars(&color_3d->bc, s->s_color);
	emit view_updated(QTCAD_VIEW_REFRESH);
	return;
    }
}

void
CADViewMeasure::adjust_text_db(void *)
{
    adjust_text();
}

void
CADViewMeasure::adjust_text()
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return;


    if (report_radians->isChecked()) {
	ma_label = new QLabel("Measured Angle (rad):");
    } else {
	ma_label = new QLabel("Measured Angle (deg):");
    }

    if (mode > 1) {
	bu_vls_sprintf(&buffer, "%.15f %s", DIST_PNT_PNT(p1, p2)*gedp->dbip->dbi_base2local, bu_units_string(gedp->dbip->dbi_local2base));
	length1_report->setText(bu_vls_cstr(&buffer));
    }

    if (mode > 2) {
	bu_vls_sprintf(&buffer, "%.15f %s", DIST_PNT_PNT(p2, p3)*gedp->dbip->dbi_base2local, bu_units_string(gedp->dbip->dbi_local2base));
	length2_report->setText(bu_vls_cstr(&buffer));
	vect_t v1, v2;
	VSUB2(v1, p1, p2);
	VSUB2(v2, p3, p2);
	VUNITIZE(v1);
	VUNITIZE(v2);
	angle = acos(VDOT(v1, v2));
	if (report_radians->isChecked()) {
	    bu_vls_sprintf(&buffer, "%.15f", angle);
	} else {
	    bu_vls_sprintf(&buffer, "%.15f", angle*180/M_PI);
	}
	angle_report->setText(bu_vls_cstr(&buffer));
    }

}

bool
CADViewMeasure::eventFilter(QObject *, QEvent *e)
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return false;
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return false;
    struct bview *v = gedp->ged_gvp;

    if (e->type() != QEvent::MouseButtonPress && e->type() != QEvent::MouseMove && e->type() != QEvent::MouseButtonRelease)
	return false;


    QMouseEvent *m_e = (QMouseEvent *)e;

    // If any other keys are down, we're not doing a measurement
    if (m_e->modifiers() != Qt::NoModifier) {
	return false;
    }

    // If we can't get a point (i.e. 3D point query with no result)
    // we have a no-op

    if (e->type() == QEvent::MouseButtonPress) {
	if (m_e->button() == Qt::RightButton) {
	    if (s)
		bv_obj_put(s);
	    mode = 0;
	    length1_report->clear();
	    length2_report->clear();
	    angle_report->clear();
	    emit view_updated(QTCAD_VIEW_REFRESH);
	    return true;
	}
	if (mode == 4) {
	    if (s)
		bv_obj_put(s);
	    mode = 0;
	    length1_report->clear();
	    length2_report->clear();
	    angle_report->clear();
	    emit view_updated(QTCAD_VIEW_REFRESH);
	    return true;
	}
	if (!mode) {
	    if (!get_point(m_e))
		return true;

	    if (s)
		bv_obj_put(s);
	    s = bv_obj_get(v, BV_VIEW_OBJS);
	    update_color();
	    mode = 1;
	    VMOVE(p1, mpnt);
	    VMOVE(p2, mpnt);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p1, BV_VLIST_LINE_MOVE);
	    bu_vls_init(&s->s_uuid);
	    bu_vls_printf(&s->s_uuid, "tool:measurement");
	    emit view_updated(QTCAD_VIEW_REFRESH);
	    return true;
	}
	if (mode == 1) {
	    if (!get_point(m_e))
		return true;

	    VMOVE(p2, mpnt);
	    return true;
	}
	if (mode == 2) {
	    if (!get_point(m_e))
		return true;

	    mode = 3;
	    BV_FREE_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p2, BV_VLIST_LINE_DRAW);
	    VMOVE(p3, mpnt);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p3, BV_VLIST_LINE_DRAW);
	    adjust_text();
	    emit view_updated(QTCAD_VIEW_REFRESH);
	}
	return true;
    }

    if (e->type() == QEvent::MouseMove) {
	if (!mode)
	    return false;
	if (mode == 1) {
	    if (!get_point(m_e))
		return true;

	    BV_FREE_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p1, BV_VLIST_LINE_MOVE);
	    VMOVE(p2, mpnt);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p2, BV_VLIST_LINE_DRAW);
	    adjust_text();
	    emit view_updated(QTCAD_VIEW_REFRESH);
	}
	if (mode == 3) {
	    if (!get_point(m_e))
		return true;

	    BV_FREE_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p2, BV_VLIST_LINE_DRAW);
	    VMOVE(p3, mpnt);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p3, BV_VLIST_LINE_DRAW);
	    adjust_text();
	    emit view_updated(QTCAD_VIEW_REFRESH);
	}
	return true;
    }

    if (e->type() == QEvent::MouseButtonRelease) {
	if (m_e->button() == Qt::RightButton) {
	    mode = 0;
	    if (s) {
		bv_obj_put(s);
		emit view_updated(QTCAD_VIEW_REFRESH);
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
	    if (!get_point(m_e))
		return true;
	    mode = 2;
	    BV_FREE_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p1, BV_VLIST_LINE_MOVE);
	    VMOVE(p2, mpnt);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p2, BV_VLIST_LINE_DRAW);
	    adjust_text();
	    emit view_updated(QTCAD_VIEW_REFRESH);
	    return true;
	}
	if (mode == 3) {
	    if (!get_point(m_e))
		return true;
	    mode = 4;
	    BV_FREE_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p2, BV_VLIST_LINE_DRAW);
	    VMOVE(p3, mpnt);
	    BV_ADD_VLIST(&s->s_v->gv_objs.gv_vlfree, &s->s_vlist, p3, BV_VLIST_LINE_DRAW);
	    adjust_text();
	    emit view_updated(QTCAD_VIEW_REFRESH);
	    return true;
	}

	return true;
    }

    // Shouldn't get here
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
