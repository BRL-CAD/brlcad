/*             C A D V I E W S E L E C T O R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2023 United States Government as represented by
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
/** @file CADViewSelector.cpp
 *
 * Brief description
 *
 */

#include "common.h"
#include <QMouseEvent>
#include <QVBoxLayout>
#include <string>
#include <set>
#include "../../../QgEdApp.h"

#include "bu/opt.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bg/aabb_ray.h"
#include "bg/plane.h"
#include "bg/lod.h"

#include "qtcad/QgSelectFilter.h"
#include "./CADViewSelector.h"

CADViewSelector::CADViewSelector(QWidget *)
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

    QObject::connect(use_pnt_select_button, &QRadioButton::clicked, this, &CADViewSelector::enable_raytrace_opt);
    QObject::connect(use_pnt_select_button, &QRadioButton::clicked, this, &CADViewSelector::enable_useall_opt);
    QObject::connect(use_rect_select_button, &QRadioButton::clicked, this, &CADViewSelector::disable_raytrace_opt);
    QObject::connect(use_rect_select_button, &QRadioButton::clicked, this, &CADViewSelector::disable_useall_opt);

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
    QObject::connect(draw_selections, &QPushButton::clicked, this, &CADViewSelector::do_draw_selections);

    erase_selections = new QPushButton("Erase selected");
    sgrp_gl->addWidget(erase_selections);
    QObject::connect(erase_selections, &QPushButton::clicked, this, &CADViewSelector::do_erase_selections);


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

    QObject::connect(erase_from_scene_button , &QRadioButton::clicked, this, &CADViewSelector::disable_groups);
    QObject::connect(add_to_group_button , &QRadioButton::clicked, this, &CADViewSelector::enable_groups);
    QObject::connect(rm_from_group_button , &QRadioButton::clicked, this, &CADViewSelector::enable_groups);

    wgroups->setLayout(sgrp_gl);
    smode_gl->addWidget(wgroups);

    smode_box->setLayout(smode_gl);

    wl->addWidget(sstyle_box);
    wl->addWidget(select_all_depth_ckbx);
    wl->addWidget(use_ray_test_ckbx);
    wl->addWidget(smode_box );

    this->setLayout(wl);

    pf = new QgSelectPntFilter();
    bf = new QgSelectBoxFilter();
    rf = new QgSelectRayFilter();
    cf = pf;
}

CADViewSelector::~CADViewSelector()
{
}

void
CADViewSelector::enable_groups(bool)
{
    //current_group->setEnabled(true);
    group_contents->setEnabled(true);
    //add_new_group->setEnabled(true);
    //rm_group->setEnabled(true);
    draw_selections->setEnabled(true);
    erase_selections->setEnabled(true);
}

void
CADViewSelector::disable_groups(bool)
{
    //current_group->setEnabled(false);
    group_contents->setEnabled(false);
    //add_new_group->setEnabled(false);
    //rm_group->setEnabled(false);
    draw_selections->setEnabled(false);
    erase_selections->setEnabled(false);
}

void
CADViewSelector::enable_raytrace_opt(bool)
{
    use_ray_test_ckbx->setEnabled(true);
}

void
CADViewSelector::disable_raytrace_opt(bool)
{
    use_ray_test_ckbx->setEnabled(false);
}

void
CADViewSelector::enable_useall_opt(bool)
{
    select_all_depth_ckbx->setEnabled(true);
}

void
CADViewSelector::disable_useall_opt(bool)
{
    select_all_depth_ckbx->setEnabled(false);
}

void
CADViewSelector::do_view_update(unsigned long long flags)
{
    if (!gedp || !gedp->dbi_state)
	return;

    BSelectState *ss = gedp->dbi_state->find_selected_state(NULL);
    if (!ss)
	return;

    unsigned long long chash = ss->state_hash();
    if ((flags & QG_VIEW_SELECT) || chash != ohash) {
	group_contents->clear();
	ohash = chash;

	std::set<std::string> ordered_paths;
	std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator s_it;
	for (s_it = ss->selected.begin(); s_it != ss->selected.end(); s_it++) {
	    std::string spath = std::string(gedp->dbi_state->pathstr(s_it->second));
	    ordered_paths.insert(spath);
	}
	std::set<std::string>::iterator o_it;
	for (o_it = ordered_paths.begin(); o_it != ordered_paths.end(); o_it++) {
	    group_contents->addItem(QString(o_it->c_str()));
	}
    }
}

void
CADViewSelector::select_objs()
{
    BSelectState *ss = gedp->dbi_state->find_selected_state(NULL);
    if (!ss)
	return;

    struct bu_vls dpath = BU_VLS_INIT_ZERO;
    for (size_t i = 0; i < BU_PTBL_LEN(&cf->selected_set); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cf->selected_set, i);
	bu_vls_sprintf(&dpath, "%s",  bu_vls_cstr(&s->s_name));
	if (bu_vls_cstr(&dpath)[0] != '/')
	    bu_vls_prepend(&dpath, "/");
	if (!ss->select_path(bu_vls_cstr(&dpath), false)) {
	    bu_vls_free(&dpath);
	    return;
	}
    }

    bu_vls_free(&dpath);
    ss->characterize();
    ss->draw_sync();
}

void
CADViewSelector::deselect_objs()
{
    BSelectState *ss = gedp->dbi_state->find_selected_state(NULL);
    if (!ss)
	return;

    struct bu_vls dpath = BU_VLS_INIT_ZERO;
    for (size_t i = 0; i < BU_PTBL_LEN(&cf->selected_set); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cf->selected_set, i);
	bu_vls_sprintf(&dpath, "%s",  bu_vls_cstr(&s->s_name));
	if (bu_vls_cstr(&dpath)[0] != '/')
	    bu_vls_prepend(&dpath, "/");
	if (!ss->deselect_path(bu_vls_cstr(&dpath), false)) {
	    bu_vls_free(&dpath);
	    return;
	}
    }

    bu_vls_free(&dpath);
    ss->characterize();
    ss->draw_sync();
}


void
CADViewSelector::erase_objs()
{
    // erase_obj_bbox
    const char **av = (const char **)bu_calloc(BU_PTBL_LEN(&cf->selected_set)+2, sizeof(char *), "av");
    av[0] = "erase";
    int scnt = 1;
    for (size_t i = 0; i < BU_PTBL_LEN(&cf->selected_set); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cf->selected_set, i);
	if (!s)
	    continue;
	av[i+1] = bu_vls_cstr(&s->s_name);
	scnt++;
    }
    ged_exec(gedp, scnt, av);
    bu_free(av, "av");
}

void
CADViewSelector::do_draw_selections()
{
    if (!gedp || !gedp->ged_gvp)
	return;

    BSelectState *ss = gedp->dbi_state->find_selected_state(NULL);
    if (!ss || !ss->selected.size())
	return;

    const char **av = (const char **)bu_calloc(ss->selected.size()+2, sizeof(char *), "av");
    av[0] = bu_strdup("draw");

    int i = 0;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator s_it;
    for (s_it = ss->selected.begin(); s_it != ss->selected.end(); s_it++) {
	av[i+1] = bu_strdup(gedp->dbi_state->pathstr(s_it->second));
	i++;
    }

    ged_exec(gedp, (int)(ss->selected.size()+1), av);
    for (size_t j = 0; j < ss->selected.size()+1; j++) {
	bu_free((void *)av[j], "path");
    }
    bu_free(av, "av");

    emit view_changed(QG_VIEW_DRAWN|QG_VIEW_SELECT);
}

void
CADViewSelector::do_erase_selections()
{
    if (!gedp || !gedp->ged_gvp)
	return;

    BSelectState *ss = gedp->dbi_state->find_selected_state(NULL);
    if (!ss || !ss->selected.size())
	return;

    const char **av = (const char **)bu_calloc(ss->selected.size()+2, sizeof(char *), "av");
    av[0] = bu_strdup("erase");

    int i = 0;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator s_it;
    for (s_it = ss->selected.begin(); s_it != ss->selected.end(); s_it++) {
	av[i+1] = bu_strdup(gedp->dbi_state->pathstr(s_it->second));
	i++;
    }

    ged_exec(gedp, (int)(ss->selected.size()+1), av);
    for (size_t j = 0; j < ss->selected.size()+1; j++) {
	bu_free((void *)av[j], "path");
    }
    bu_free(av, "av");

    emit view_changed(QG_VIEW_DRAWN|QG_VIEW_SELECT);
}

bool
CADViewSelector::eventFilter(QObject *o, QEvent *e)
{
    if (QApplication::keyboardModifiers() != Qt::NoModifier)
	return false;

    QgModel *m = ((QgEdApp *)qApp)->mdl;
    if (!m)
	return false;
    gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return false;
    struct bview *v = gedp->ged_gvp;

    // Set the libqtcad filter based on current options
    cf = pf;
    if (use_rect_select_button->isChecked())
	cf = bf;
    if (use_ray_test_ckbx->isChecked()) {
	rf->dbip = gedp->dbip;
	cf = rf;
    }

    // Inform the filter of the current settings and view
    cf->v = v;
    cf->first_only = select_all_depth_ckbx->isChecked() ? false : true;

    // TODO - create and/or connect the signals and slots so cf can
    // properly trigger updating and drawing
    bool ret = cf->eventFilter(o, e);
    if (!ret)
	return false;

    if (erase_from_scene_button->isChecked()) {
	erase_objs();
	emit view_changed(QG_VIEW_DRAWN);
	bu_ptbl_reset(&cf->selected_set);
	return true;
    }

    if (add_to_group_button->isChecked()) {
	select_objs();
	emit view_changed(QG_VIEW_DRAWN);
	bu_ptbl_reset(&cf->selected_set);
	return true;
    }

    if (rm_from_group_button->isChecked()) {
	deselect_objs();
	emit view_changed(QG_VIEW_DRAWN);
	bu_ptbl_reset(&cf->selected_set);
	return true;
    }

    // Shouldn't get here...
    return true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
