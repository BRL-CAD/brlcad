/*             C A D V I E W S E L E C T O R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2023-2026 United States Government as represented by
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
#include <QApplication>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <string>
#include <set>
#include <vector>
#include "qtcad/QgPluginContext.h"
#include "qtcad/QgSignalFlags.h"

#include "bu/opt.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bg/aabb_ray.h"
#include "bg/plane.h"
#include "bsg/interaction.h"
#include "bsg/node.h"
#include "ged/selection_state.h"

#include "qtcad/QgSelectFilter.h"
#include "./CADViewSelector.h"

static std::vector<std::string>
qged_selection_paths(struct ged *gedp)
{
    std::vector<std::string> ret;
    if (!ged_selection_state_available(gedp))
	return ret;

    struct bu_vls paths = BU_VLS_INIT_ZERO;
    if (!ged_selection_list_paths(gedp, nullptr, &paths)) {
	bu_vls_free(&paths);
	return ret;
    }

    const char *pstr = bu_vls_cstr(&paths);
    const char *start = pstr;
    for (const char *c = pstr; c && *c; c++) {
	if (*c != '\n')
	    continue;
	if (c > start)
	    ret.push_back(std::string(start, (size_t)(c - start)));
	start = c + 1;
    }
    if (start && *start)
	ret.push_back(std::string(start));
    bu_vls_free(&paths);

    return ret;
}

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
CADViewSelector::do_view_update(QgViewUpdateFlags flags)
{
    struct ged *gedp = m_ctx ? m_ctx->getGed() : nullptr;
    if (!ged_selection_state_available(gedp))
	return;

    unsigned long long chash = ged_selection_state_hash(gedp, nullptr);
    if ((flags & QG_VIEW_SELECT) || chash != ohash) {
	group_contents->clear();
	ohash = chash;

	std::set<std::string> ordered_paths;
	std::vector<std::string> paths = qged_selection_paths(gedp);
	for (size_t i = 0; i < paths.size(); i++)
	    ordered_paths.insert(paths[i]);
	std::set<std::string>::iterator o_it;
	for (o_it = ordered_paths.begin(); o_it != ordered_paths.end(); o_it++) {
	    group_contents->addItem(QString(o_it->c_str()));
	}
    }
}

void
CADViewSelector::select_objs()
{
    struct ged *gedp = m_ctx ? m_ctx->getGed() : nullptr;
    if (!ged_selection_state_available(gedp))
	return;

    const struct bsg_interaction_result *res = cf->interaction_result();
    if (!res || !bsg_interaction_result_count(res))
	return;
    for (size_t i = 0; i < bsg_interaction_result_count(res); i++) {
	const struct bsg_interaction_record *rec = bsg_interaction_result_get(res, i);
	if (!rec)
	    continue;
	const char *path = bsg_interaction_record_path(rec);
	if (!path || !path[0])
	    continue;
	if (!ged_selection_select_path(gedp, nullptr, path, 0))
	    return;
    }

    ged_selection_recompute(gedp, nullptr);
    ged_selection_draw_sync(gedp, nullptr);
}

void
CADViewSelector::deselect_objs()
{
    struct ged *gedp = m_ctx ? m_ctx->getGed() : nullptr;
    if (!ged_selection_state_available(gedp))
	return;

    const struct bsg_interaction_result *res = cf->interaction_result();
    if (!res || !bsg_interaction_result_count(res))
	return;
    for (size_t i = 0; i < bsg_interaction_result_count(res); i++) {
	const struct bsg_interaction_record *rec = bsg_interaction_result_get(res, i);
	if (!rec)
	    continue;
	const char *path = bsg_interaction_record_path(rec);
	if (!path || !path[0])
	    continue;
	if (!ged_selection_deselect_path(gedp, nullptr, path, 0))
	    return;
    }

    ged_selection_recompute(gedp, nullptr);
    ged_selection_draw_sync(gedp, nullptr);
}


void
CADViewSelector::erase_objs()
{
    struct ged *gedp = m_ctx ? m_ctx->getGed() : nullptr;
    if (!gedp)
	return;
    const struct bsg_interaction_result *res = cf->interaction_result();
    if (!res || !bsg_interaction_result_count(res))
	return;
    size_t pick_cnt = bsg_interaction_result_count(res);
    const char **av = (const char **)bu_calloc(pick_cnt+2, sizeof(char *), "av");
    av[0] = "erase";
    int scnt = 1;
    for (size_t i = 0; i < pick_cnt; i++) {
	const struct bsg_interaction_record *rec = bsg_interaction_result_get(res, i);
	if (!rec)
	    continue;
	const char *path = bsg_interaction_record_path(rec);
	if (!path || !path[0])
	    continue;
	av[scnt++] = path;
    }
    ged_exec_erase(gedp, scnt, av);
    bu_free(av, "av");
}

void
CADViewSelector::do_draw_selections()
{
    struct ged *gedp = m_ctx ? m_ctx->getGed() : nullptr;
    if (!gedp || !gedp->ged_gvp)
	return;

    std::vector<std::string> paths = qged_selection_paths(gedp);
    if (!paths.size())
	return;

    const char **av = (const char **)bu_calloc(paths.size()+2, sizeof(char *), "av");
    av[0] = bu_strdup("draw");

    for (size_t i = 0; i < paths.size(); i++)
	av[i+1] = bu_strdup(paths[i].c_str());

    ged_exec_draw(gedp, (int)(paths.size()+1), av);
    for (size_t j = 0; j < paths.size()+1; j++) {
	bu_free((void *)av[j], "path");
    }
    bu_free(av, "av");

    emit view_changed(QG_VIEW_DRAWN|QG_VIEW_SELECT);
}

void
CADViewSelector::do_erase_selections()
{
    struct ged *gedp = m_ctx ? m_ctx->getGed() : nullptr;
    if (!gedp || !gedp->ged_gvp)
	return;

    std::vector<std::string> paths = qged_selection_paths(gedp);
    if (!paths.size())
	return;

    const char **av = (const char **)bu_calloc(paths.size()+2, sizeof(char *), "av");
    av[0] = bu_strdup("erase");

    for (size_t i = 0; i < paths.size(); i++)
	av[i+1] = bu_strdup(paths[i].c_str());

    ged_exec_erase(gedp, (int)(paths.size()+1), av);
    for (size_t j = 0; j < paths.size()+1; j++) {
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

    struct ged *gedp = m_ctx ? m_ctx->getGed() : nullptr;
    if (!gedp || !gedp->ged_gvp)
	return false;
    struct bsg_view *v = gedp->ged_gvp;

    // Set the libqtcad filter based on current options
    cf = pf;
    if (use_rect_select_button->isChecked())
	cf = bf;
    if (use_ray_test_ckbx->isChecked()) {
	rf->dbip = gedp->dbip;
	cf = rf;
    }

    // Inform the filter of the current settings and view
    cf->set_view(v);
    cf->first_only = select_all_depth_ckbx->isChecked() ? false : true;

    // TODO - create and/or connect the signals and slots so cf can
    // properly trigger updating and drawing
    bool ret = cf->eventFilter(o, e);
    if (!ret)
	return false;

    if (erase_from_scene_button->isChecked()) {
	erase_objs();
	emit view_changed(QG_VIEW_DRAWN);
	return true;
    }

    if (add_to_group_button->isChecked()) {
	select_objs();
	emit view_changed(QG_VIEW_DRAWN);
	return true;
    }

    if (rm_from_group_button->isChecked()) {
	deselect_objs();
	emit view_changed(QG_VIEW_DRAWN);
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
