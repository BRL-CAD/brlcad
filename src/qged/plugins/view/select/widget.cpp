/*                     W I D G E T . C P P
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
/** @file widget.cpp
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
#include "app.h"

#include "./widget.h"

CADViewSelecter::CADViewSelecter(QWidget *)
{
    QVBoxLayout *wl = new QVBoxLayout;
    wl->setAlignment(Qt::AlignTop);

    QGroupBox *sstyle_box = new QGroupBox("Selection Style");
    QButtonGroup *sstyle_grp = new QButtonGroup();
    QVBoxLayout *sstyle_gl = new QVBoxLayout;
    sstyle_gl->setAlignment(Qt::AlignTop);

    use_pnt_select_button = new QRadioButton("Select Under Point");
    use_pnt_select_button->setChecked(true);
    sstyle_grp->addButton(use_pnt_select_button);
    sstyle_gl->addWidget(use_pnt_select_button);

    use_rect_select_button = new QRadioButton("Select Under Rectangle");
    sstyle_grp->addButton(use_rect_select_button);
    sstyle_gl->addWidget(use_rect_select_button);

    sstyle_box->setLayout(sstyle_gl);

    select_all_depth_ckbx = new QCheckBox("Use All Intersections");
    use_ray_test_ckbx = new QCheckBox("Test with Raytracing");

    QObject::connect(use_pnt_select_button, &QRadioButton::clicked, this, &CADViewSelecter::enable_raytrace_opt);
    QObject::connect(use_pnt_select_button, &QRadioButton::clicked, this, &CADViewSelecter::enable_useall_opt);
    QObject::connect(use_rect_select_button, &QRadioButton::clicked, this, &CADViewSelecter::disable_raytrace_opt);
    QObject::connect(use_rect_select_button, &QRadioButton::clicked, this, &CADViewSelecter::disable_useall_opt);

    QGroupBox *smode_box = new QGroupBox("Selection Mode");
    QVBoxLayout *smode_gl = new QVBoxLayout;
    smode_gl->setAlignment(Qt::AlignTop);

    QButtonGroup *smode_grp = new QButtonGroup();
    erase_from_scene_button = new QRadioButton("Erase from Scene");
    erase_from_scene_button->setChecked(true);
    smode_grp->addButton(erase_from_scene_button);
    smode_gl->addWidget(erase_from_scene_button);

    add_to_group_button = new QRadioButton("Add to Current Set");
    smode_grp->addButton(add_to_group_button);
    smode_gl->addWidget(add_to_group_button);

    rm_from_group_button = new QRadioButton("Remove from Current Set");
    smode_grp->addButton(rm_from_group_button);
    smode_gl->addWidget(rm_from_group_button);

    QGroupBox *wgroups = new QGroupBox("Current Selection Set");
    QVBoxLayout *sgrp_gl = new QVBoxLayout;
    sgrp_gl->setAlignment(Qt::AlignTop);


    draw_selections = new QPushButton("Draw selected");
    sgrp_gl->addWidget(draw_selections);
    QObject::connect(draw_selections, &QPushButton::clicked, this, &CADViewSelecter::do_draw_selections);

    erase_selections = new QPushButton("Erase selected");
    sgrp_gl->addWidget(erase_selections);
    QObject::connect(erase_selections, &QPushButton::clicked, this, &CADViewSelecter::do_erase_selections);


    QWidget *sgrp = new QWidget();
    QHBoxLayout *groups_gl = new QHBoxLayout;
    groups_gl->setAlignment(Qt::AlignLeft);
    //current_group = new QComboBox();
    //groups_gl->addWidget(current_group);
    //add_new_group = new QPushButton("+");
    //groups_gl->addWidget(add_new_group);
    //rm_group = new QPushButton("-");
    //groups_gl->addWidget(rm_group);
    sgrp->setLayout(groups_gl);
    sgrp_gl->addWidget(sgrp);

    group_contents = new QListWidget();
    sgrp_gl->addWidget(group_contents);

    disable_groups(false);

    QObject::connect(erase_from_scene_button , &QRadioButton::clicked, this, &CADViewSelecter::disable_groups);
    QObject::connect(add_to_group_button , &QRadioButton::clicked, this, &CADViewSelecter::enable_groups);
    QObject::connect(rm_from_group_button , &QRadioButton::clicked, this, &CADViewSelecter::enable_groups);

    wgroups->setLayout(sgrp_gl);
    smode_gl->addWidget(wgroups);

    smode_box->setLayout(smode_gl);

    wl->addWidget(sstyle_box);
    wl->addWidget(select_all_depth_ckbx);
    wl->addWidget(use_ray_test_ckbx);
    wl->addWidget(smode_box );

    this->setLayout(wl);
}

CADViewSelecter::~CADViewSelecter()
{
}

void
CADViewSelecter::enable_groups(bool)
{
    //current_group->setEnabled(true);
    group_contents->setEnabled(true);
    //add_new_group->setEnabled(true);
    //rm_group->setEnabled(true);
    draw_selections->setEnabled(true);
    erase_selections->setEnabled(true);
}

void
CADViewSelecter::disable_groups(bool)
{
    //current_group->setEnabled(false);
    group_contents->setEnabled(false);
    //add_new_group->setEnabled(false);
    //rm_group->setEnabled(false);
    draw_selections->setEnabled(false);
    erase_selections->setEnabled(false);
}

void
CADViewSelecter::enable_raytrace_opt(bool)
{
    use_ray_test_ckbx->setEnabled(true);
}

void
CADViewSelecter::disable_raytrace_opt(bool)
{
    use_ray_test_ckbx->setEnabled(false);
}

void
CADViewSelecter::enable_useall_opt(bool)
{
    select_all_depth_ckbx->setEnabled(true);
}

void
CADViewSelecter::disable_useall_opt(bool)
{
    select_all_depth_ckbx->setEnabled(false);
}

void
CADViewSelecter::do_view_update(unsigned long long flags)
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;

    unsigned long long mhash = ged_selection_hash_sets(gedp->ged_selection_sets);
    if ((flags & QTCAD_VIEW_SELECT) || mhash != omhash) {
#if 0
	current_group->clear();
	struct bu_ptbl ssets = BU_PTBL_INIT_ZERO;
	size_t sscnt = ged_selection_sets_lookup(&ssets, gedp->ged_selection_sets, "*");
	for (size_t i = 0; i < sscnt; i++) {
	    struct ged_selection_set *s = (struct ged_selection_set *)BU_PTBL_GET  (&ssets, i);
	    current_group->addItem(QString(bu_vls_cstr(&s->name)));
	}
	bu_ptbl_free(&ssets);
#endif
    }

    unsigned long long chash = ged_selection_hash_set(gedp->ged_cset);
    if ((flags & QTCAD_VIEW_SELECT) || chash != ohash) {
	group_contents->clear();
	ohash = chash;
	char **spaths = NULL;
	int pscnt = ged_selection_set_list(&spaths, gedp->ged_cset);
	for (int i = 0; i < pscnt; i++) {
	    group_contents->addItem(QString(spaths[i]));
	}
	bu_free(spaths, "spaths");
    }
}




struct rec_state {
    std::unordered_set<std::string> active;
    int rec_all;
    double cdist;
    std::string closest;
};

static int
_obj_record(struct application *ap, struct partition *p_hp, struct seg *UNUSED(segs))
{
    struct rec_state *rc = (struct rec_state *)ap->a_uptr;
    for (struct partition *pp = p_hp->pt_forw; pp != p_hp; pp = pp->pt_forw) {
	if (rc->rec_all) {
	    rc->active.insert(std::string(pp->pt_regionp->reg_name));
	} else {
	    struct hit *hitp = pp->pt_inhit;
	    if (hitp->hit_dist < rc->cdist) {
		rc->closest = std::string(pp->pt_regionp->reg_name);
		rc->cdist = hitp->hit_dist;
	    }
	}
    }
    bu_log("hit\n");
    return 1;
}

static int
_ovlp_record(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2, struct partition *UNUSED(ihp))
{
    struct rec_state *rc = (struct rec_state *)ap->a_uptr;
    if (rc->rec_all) {
	rc->active.insert(std::string(reg1->reg_name));
	rc->active.insert(std::string(reg2->reg_name));
    } else {
	rc->closest = std::string(reg1->reg_name);
	rc->cdist = pp->pt_inhit->hit_dist;
    }
    bu_log("ovlp\n");
    return 1;
}

bool
CADViewSelecter::process_obj_bbox(int mode)
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return false;
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return false;
    struct bview *v = gedp->ged_gvp;

    if (select_all_depth_ckbx->isChecked() || use_rect_select_button->isChecked()) {
	if (mode == 0) {
	    // erase_obj_bbox
	    const char **av = (const char **)bu_calloc(scnt+2, sizeof(char *), "av");
	    av[0] = "erase";
	    for (int i = 0; i < scnt; i++) {
		struct bv_scene_obj *s = sset[i];
		av[i+1] = bu_vls_cstr(&s->s_name);
		bu_log("%s\n", av[i+1]);
	    }
	    ged_exec(gedp, scnt+1, av);
	    bu_free(av, "av");
	    return true;
	}

	struct ged_selection_set *gs = gedp->ged_cset;
	if (!gs)
	    return false;
	struct bu_vls dpath = BU_VLS_INIT_ZERO;
	for (int i = 0; i < scnt; i++) {
	    struct bv_scene_obj *s = sset[i];
	    bu_vls_sprintf(&dpath, "%s",  bu_vls_cstr(&s->s_name));
	    if (bu_vls_cstr(&dpath)[0] != '/')
		bu_vls_prepend(&dpath, "/");
	    if (mode == 1) {
		// add_obj_bbox
		if (!ged_selection_insert(gs, bu_vls_cstr(&dpath))) {
		    bu_vls_free(&dpath);
		    return false;
		}
	    }
	    if (mode == 2) {
		// rm_obj_bbox
		ged_selection_remove(gs, bu_vls_cstr(&dpath));
	    }
	}
	bu_vls_free(&dpath);
	return true;
    }

    // Only removing one object, not using all-up librt raytracing -
    // need to find the first bbox intersection, then run the select
    // command.
    struct bv_scene_obj *s_closest = NULL;
    double dist = DBL_MAX;
    int ix = (int)vx;
    int iy = (int)vy;
    bv_screen_to_view(v, &vx, &vy, ix, iy);
    point_t vpnt, mpnt;
    VSET(vpnt, vx, vy, 0);
    MAT4X3PNT(mpnt, v->gv_view2model, vpnt);
    point_t rmin, rmax;
    vect_t dir;
    VMOVEN(dir, v->gv_rotation + 8, 3);
    VUNITIZE(dir);
    VSCALE(dir, dir, v->radius);
    VADD2(mpnt, mpnt, dir);
    VUNITIZE(dir);
    bg_ray_invdir(&dir, dir);
    for (int i = 0; i < scnt; i++) {
	struct bv_scene_obj *s = sset[i];
	if (bg_isect_aabb_ray(rmin, rmax, mpnt, dir, s->bmin, s->bmax)){
	    double ndist = DIST_PNT_PNT(rmin, v->gv_vc_backout);
	    if (ndist < dist) {
		dist = ndist;
		s_closest = s;
	    }
	}
    }
    if (s_closest) {

	if (mode == 0) {
	    const char **av = (const char **)bu_calloc(3, sizeof(char *), "av");
	    av[0] = "erase";
	    av[1] = bu_vls_cstr(&s_closest->s_name);
	    ged_exec(gedp, 2, av);
	    bu_free(av, "av");
	    return true;
	}

	struct ged_selection_set *gs = gedp->ged_cset;
	if (!gs)
	    return false;

	struct bu_vls dpath = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&dpath, "%s",  bu_vls_cstr(&s_closest->s_name));
	if (bu_vls_cstr(&dpath)[0] != '/')
	    bu_vls_prepend(&dpath, "/");

	if (mode == 1) {
	    if (!ged_selection_insert(gs, bu_vls_cstr(&dpath))) {
		bu_vls_free(&dpath);
		return false;
	    }
	}

	if (mode == 2) {
	    ged_selection_remove(gs, bu_vls_cstr(&dpath));
	}

	bu_vls_free(&dpath);
	return true;

    } else {
	// no-op
	return false;
    }
}


bool
CADViewSelecter::process_obj_ray(int mode)
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return false;
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return false;
    struct bview *v = gedp->ged_gvp;

    // librt intersection test.
    struct application *ap;
    BU_GET(ap, struct application);
    RT_APPLICATION_INIT(ap);
    ap->a_onehit = 0;
    ap->a_hit = _obj_record;
    ap->a_miss = NULL;
    ap->a_overlap = _ovlp_record;
    ap->a_logoverlap = NULL;

    struct rt_i *rtip = rt_new_rti(gedp->dbip);
    struct resource *resp = NULL;
    BU_GET(resp, struct resource);
    rt_init_resource(resp, 0, rtip);
    ap->a_resource = resp;
    ap->a_rt_i = rtip;
    const char **objs = (const char **)bu_calloc(scnt + 1, sizeof(char *), "objs");
    for (int i = 0; i < scnt; i++) {
	struct bv_scene_obj *s = sset[i];
	objs[i] = bu_vls_cstr(&s->s_name);
    }
    if (rt_gettrees_and_attrs(rtip, NULL, scnt, objs, 1)) {
	bu_free(objs, "objs");
	rt_free_rti(rtip);
	BU_PUT(resp, struct resource);
	BU_PUT(ap, struct appliation);
	return false;
    }
    size_t ncpus = bu_avail_cpus();
    rt_prep_parallel(rtip, (int)ncpus);
    int ix = (int)vx;
    int iy = (int)vy;
    bv_screen_to_view(v, &vx, &vy, ix, iy);
    point_t vpnt, mpnt;
    VSET(vpnt, vx, vy, 0);
    MAT4X3PNT(mpnt, v->gv_view2model, vpnt);
    vect_t dir;
    VMOVEN(dir, v->gv_rotation + 8, 3);
    VUNITIZE(dir);
    VSCALE(dir, dir, v->radius);
    VADD2(ap->a_ray.r_pt, mpnt, dir);
    VUNITIZE(dir);
    VSCALE(ap->a_ray.r_dir, dir, -1);

    struct rec_state rc;

    // Since most of the work of the raytracing approach is the same,
    // we just change what we record in the hit function based on
    // the checkbox settings
    if (select_all_depth_ckbx->isChecked()) {
	rc.rec_all = 1;
    } else {
	rc.rec_all = 0;
	rc.cdist = INFINITY;
    }
    ap->a_uptr = (void *)&rc;

    (void)rt_shootray(ap);
    bu_free(objs, "objs");
    rt_free_rti(rtip);
    BU_PUT(resp, struct resource);
    BU_PUT(ap, struct appliation);

    if (select_all_depth_ckbx->isChecked()) {
	if (rc.active.size()) {
	    std::unordered_set<std::string>::iterator a_it;
	    if (mode == 0) {
		const char **av = (const char **)bu_calloc(rc.active.size()+2, sizeof(char *), "av");
		av[0] = bu_strdup("erase");
		int i = 0;
		for (a_it = rc.active.begin(); a_it != rc.active.end(); a_it++) {
		    av[i+1] = bu_strdup(a_it->c_str());
		    i++;
		}
		ged_exec(gedp, rc.active.size()+1, av);
		bu_argv_free(rc.active.size()+1, (char **)av);
		return true;
	    }

	    struct ged_selection_set *gs = gedp->ged_cset;
	    if (!gs)
		return false;
	    struct bu_vls dpath = BU_VLS_INIT_ZERO;
	    for (a_it = rc.active.begin(); a_it != rc.active.end(); a_it++) {
		bu_vls_sprintf(&dpath, "%s",  a_it->c_str());
		if (bu_vls_cstr(&dpath)[0] != '/')
		    bu_vls_prepend(&dpath, "/");
		if (mode == 1) {
		    if (!ged_selection_insert(gs, bu_vls_cstr(&dpath))) {
			bu_vls_free(&dpath);
			return false;
		    }
		}
		if (mode == 2) {
		    ged_selection_remove(gs, bu_vls_cstr(&dpath));
		}
	    }
	    bu_vls_free(&dpath);
	    return true;
	} else {
	    return false;
	}
    } else {
	if (rc.cdist < INFINITY) {
	    if (mode == 0) {
		const char **av = (const char **)bu_calloc(3, sizeof(char *), "av");
		av[0] = "erase";
		av[1] = bu_strdup(rc.closest.c_str());
		ged_exec(gedp, 2, av);
		bu_free((void *)av[1], "ncpy");
		return true;
	    }

	    struct ged_selection_set *gs = gedp->ged_cset;
	    if (!gs)
		return false;
	    struct bu_vls dpath = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&dpath, "%s",  rc.closest.c_str());
	    if (bu_vls_cstr(&dpath)[0] != '/')
		bu_vls_prepend(&dpath, "/");
	    if (mode == 1) {
		if (!ged_selection_insert(gs, bu_vls_cstr(&dpath))) {
		    bu_vls_free(&dpath);
		    return false;
		}
	    }
	    if (mode == 2) {
		ged_selection_remove(gs, bu_vls_cstr(&dpath));
	    }
	    bu_vls_free(&dpath);
	    return true;
	} else {
	    return false;
	}
    }
}

bool
CADViewSelecter::erase_obj_bbox()
{
    return process_obj_bbox(0);
}

bool
CADViewSelecter::erase_obj_ray()
{
    return process_obj_ray(0);
}

bool
CADViewSelecter::add_obj_bbox()
{
    return process_obj_bbox(1);
}

bool
CADViewSelecter::add_obj_ray()
{
    return process_obj_ray(1);
}

bool
CADViewSelecter::rm_obj_bbox()
{
    return process_obj_bbox(2);
}

bool
CADViewSelecter::rm_obj_ray()
{
    return process_obj_ray(2);
}

void
CADViewSelecter::do_draw_selections()
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return;

    char **spaths = NULL;
    int pscnt = ged_selection_set_list(&spaths, gedp->ged_cset);
    const char **av = (const char **)bu_calloc(pscnt+2, sizeof(char *), "av");
    av[0] = "draw";
    for (int i = 0; i < pscnt; i++) {
	av[i+1] = spaths[i];
    }
    ged_exec(gedp, pscnt+1, av);
    bu_free(spaths, "spaths");
    bu_free(av, "av");

    emit view_changed(QTCAD_VIEW_DRAWN|QTCAD_VIEW_SELECT);
}

void
CADViewSelecter::do_erase_selections()
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return;
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return;

    char **spaths = NULL;
    int pscnt = ged_selection_set_list(&spaths, gedp->ged_cset);
    const char **av = (const char **)bu_calloc(pscnt+2, sizeof(char *), "av");
    av[0] = "erase";
    for (int i = 0; i < pscnt; i++) {
	av[i+1] = spaths[i];
    }
    ged_exec(gedp, pscnt+1, av);
    bu_free(spaths, "spaths");
    bu_free(av, "av");

    emit view_changed(QTCAD_VIEW_DRAWN|QTCAD_VIEW_SELECT);
}

bool
CADViewSelecter::eventFilter(QObject *, QEvent *e)
{
    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return false;
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return false;
    struct bview *v = gedp->ged_gvp;
    scnt = 0;
    sset = NULL;
    vx = -FLT_MAX;
    vy = -FLT_MAX;

    // If this isn't a mouse event, we're done
    if (e->type() != QEvent::MouseButtonPress &&
	    e->type() != QEvent::MouseButtonRelease &&
	    e->type() != QEvent::MouseButtonDblClick &&
	    e->type() != QEvent::MouseMove &&
	    e->type() != QEvent::Wheel
       )
	return false;

    QMouseEvent *m_e = (QMouseEvent *)e;

    if (e->type() == QEvent::MouseButtonDblClick)
	return true;

    if (QApplication::keyboardModifiers() != Qt::NoModifier) {
	enabled = false;
	return false;
    }

    if (e->type() == QEvent::MouseButtonPress) {
	if (use_rect_select_button->isChecked()) {
#ifdef USE_QT6
	    px = m_e->position().x();
	    py = m_e->position().y();
#else
	    px = m_e->x();
	    py = m_e->y();
#endif
	    gedp->ged_gvp->gv_s->gv_rect.line_width = 1;
	    gedp->ged_gvp->gv_s->gv_rect.pos[0] = px;
	    gedp->ged_gvp->gv_s->gv_rect.pos[1] = dm_get_height((struct dm *)gedp->ged_gvp->dmp) - py;
	    gedp->ged_gvp->gv_s->gv_rect.cdim[0] = dm_get_width((struct dm *)gedp->ged_gvp->dmp);
	    gedp->ged_gvp->gv_s->gv_rect.cdim[1] = dm_get_height((struct dm *)gedp->ged_gvp->dmp);
	    gedp->ged_gvp->gv_s->gv_rect.aspect = (fastf_t)gedp->ged_gvp->gv_s->gv_rect.cdim[X] / gedp->ged_gvp->gv_s->gv_rect.cdim[Y];
	    emit view_changed(QTCAD_VIEW_DRAWN);
	}
	return true;
    }

    // If certain kinds of mouse events take place, we know we are manipulating the
    // view to achieve something other than erasure.  Flag accordingly, so we don't
    // fire off the select event at the end of whatever we're doing instead.
    if (e->type() == QEvent::MouseMove && !use_rect_select_button->isChecked()) {
	enabled = false;
	return false;
    }

    if (e->type() == QEvent::MouseMove && enabled && use_rect_select_button->isChecked()) {
#ifdef USE_QT6
	vx = m_e->position().x();
	vy = m_e->position().y();
#else
	vx = m_e->x();
	vy = m_e->y();
#endif
	gedp->ged_gvp->gv_s->gv_rect.draw = 1;
	gedp->ged_gvp->gv_s->gv_rect.dim[0] = vx - px;
	gedp->ged_gvp->gv_s->gv_rect.dim[1] = (dm_get_height((struct dm *)gedp->ged_gvp->dmp) - vy) - gedp->ged_gvp->gv_s->gv_rect.pos[1];

	struct bv_interactive_rect_state *grsp = &gedp->ged_gvp->gv_s->gv_rect;
	grsp->x = (grsp->pos[X] / (fastf_t)grsp->cdim[X] - 0.5) * 2.0;
	grsp->y = ((0.5 - (grsp->cdim[Y] - grsp->pos[Y]) / (fastf_t)grsp->cdim[Y]) / grsp->aspect * 2.0);
	grsp->width = grsp->dim[X] * 2.0 / (fastf_t)grsp->cdim[X];
	grsp->height = grsp->dim[Y] * 2.0 / (fastf_t)grsp->cdim[X];

	emit view_changed(QTCAD_VIEW_DRAWN);
	return true;
    }

    if (e->type() == QEvent::MouseButtonRelease) {

	if (m_e->button() == Qt::RightButton)
	    return true;

	// If we were doing something else and the mouse release signals we're
	// done, re-enable the select behavior
	if (!enabled) {
	    enabled = true;
	    return false;
	}

#ifdef USE_QT6
	vx = m_e->position().x();
	vy = m_e->position().y();
#else
	vx = m_e->x();
	vy = m_e->y();
#endif
	int ix = (int)vx;
	int iy = (int)vy;

	if (use_rect_select_button->isChecked()) {
	    int ipx = (int)px;
	    int ipy = (int)py;
	    scnt = bg_view_objs_rect_select(&sset, v, ipx, ipy, ix, iy);

	    // reset rectangle
	    struct bv_interactive_rect_state *grsp = &gedp->ged_gvp->gv_s->gv_rect;
	    grsp->draw = 0;
	    grsp->line_width = 0;
	    grsp->pos[0] = 0;
	    grsp->pos[1] = 0;
	    grsp->dim[0] = 0;
	    grsp->dim[1] = 0;
	    emit view_changed(QTCAD_VIEW_DRAWN);
	} else {
	    scnt = bg_view_objs_select(&sset, v, ix, iy);
	}

	// If we didn't select anything, we have a no-op
	if (!scnt)
	    return false;

	if (erase_from_scene_button->isChecked()) {
	    if (!use_ray_test_ckbx->isChecked()) {
		bool ret = erase_obj_bbox();
		if (ret)
		    emit view_changed(QTCAD_VIEW_DRAWN);
		bu_free(sset, "sset");
		return true;
	    }

	    if (use_pnt_select_button->isChecked() && use_ray_test_ckbx->isChecked()) {
		bool ret = erase_obj_ray();
		if (ret)
		    emit view_changed(QTCAD_VIEW_DRAWN);
		bu_free(sset, "sset");
		return ret;
	    }

	    // If we didn't process by this point, no-op
	    return false;
	}

	if (add_to_group_button->isChecked()) {
	    if (!use_ray_test_ckbx->isChecked()) {
		bool ret = add_obj_bbox();
		if (ret)
		    emit view_changed(QTCAD_VIEW_SELECT);
		bu_free(sset, "sset");
		return true;
	    }

	    if (use_pnt_select_button->isChecked() && use_ray_test_ckbx->isChecked()) {
		bool ret = add_obj_ray();
		if (ret)
		    emit view_changed(QTCAD_VIEW_SELECT);
		bu_free(sset, "sset");
		return ret;
	    }

	    // If we didn't process by this point, no-op
	    return false;
	}


	if (rm_from_group_button->isChecked()) {
	    if (!use_ray_test_ckbx->isChecked()) {
		bool ret = rm_obj_bbox();
		if (ret)
		    emit view_changed(QTCAD_VIEW_SELECT);
		bu_free(sset, "sset");
		return true;
	    }

	    if (use_pnt_select_button->isChecked() && use_ray_test_ckbx->isChecked()) {
		bool ret = rm_obj_ray();
		if (ret)
		    emit view_changed(QTCAD_VIEW_SELECT);
		bu_free(sset, "sset");
		return ret;
	    }

	    // If we didn't process by this point, no-op
	    return false;
	}

	// If we didn't process by this point (??), no-op
	return false;

    }

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
