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
#include <QButtonGroup>
#include <QGroupBox>
#include <QVBoxLayout>
#include <string>
#include <vector>

#include "bu/vls.h"
#include "ged.h"
#include "ged/scene.h"
#include "ged/selection.h"

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgSelectFilter.h"
#include "qtcad/QgView.h"
#include "CADViewSelector.h"

static QByteArray
cad_selection_set_name(const QgPluginContext *context)
{
    return context ? context->selectionSetName().toLocal8Bit() : QByteArray();
}


static std::vector<std::string>
cad_selected_paths(struct ged *gedp, const char *set_name)
{
    std::vector<std::string> paths;
    struct bu_vls selected = BU_VLS_INIT_ZERO;
    (void)ged_selection_list_paths(gedp, set_name, &selected);

    const char *selected_str = bu_vls_cstr(&selected);
    const char *start = selected_str;
    for (const char *current = selected_str; ; current++) {
	if (*current == '\n' || *current == '\r' || *current == '\0') {
	    if (current > start)
		paths.push_back(std::string(start,
			(size_t)(current - start)));
	    if (*current == '\0')
		break;
	    start = current + 1;
	}
    }

    bu_vls_free(&selected);
    return paths;
}

static enum ged_scene_status
cad_scene_draw_paths(struct ged *gedp, ged_view_context *view,
	const std::vector<std::string> &paths, int *changed)
{
    if (changed)
	*changed = 0;
    if (!gedp || !view || paths.empty())
	return GED_SCENE_INVALID;

    std::vector<const char *> path_values;
    path_values.reserve(paths.size());
    for (const std::string &path : paths)
	path_values.push_back(path.c_str());

    struct ged_scene_draw_request request;
    ged_scene_draw_request_init(&request);
    request.view = view;
    request.paths = path_values.data();
    request.path_count = path_values.size();

    struct ged_scene_result *result = ged_scene_result_create();
    if (!result)
	return GED_SCENE_ERROR;
    const enum ged_scene_status status = ged_scene_draw(gedp, &request,
	result);
    if (changed)
	*changed = ged_scene_result_changed(result);
    ged_scene_result_destroy(result);
    return status;
}

static enum ged_scene_status
cad_scene_erase_paths(struct ged *gedp, ged_view_context *view,
	const std::vector<std::string> &paths, int *changed)
{
    if (changed)
	*changed = 0;
    if (!gedp || !view || paths.empty())
	return GED_SCENE_INVALID;

    struct ged_scene_result *result = ged_scene_result_create();
    if (!result)
	return GED_SCENE_ERROR;

    enum ged_scene_status status = GED_SCENE_OK;
    int scene_changed = 0;
    for (const std::string &path : paths) {
	struct ged_scene_erase_request request;
	ged_scene_erase_request_init(&request);
	request.view = view;
	request.path = path.c_str();
	status = ged_scene_erase(gedp, &request, result);
	if (status != GED_SCENE_OK)
	    break;
	scene_changed |= ged_scene_result_changed(result);
    }
    if (changed)
	*changed = scene_changed;
    ged_scene_result_destroy(result);
    return status;
}

CADViewSelector::CADViewSelector(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *wl = new QVBoxLayout;
    wl->setAlignment(Qt::AlignTop);

    QGroupBox *sstyle_box = new QGroupBox("Selection Style");
    QButtonGroup *sstyle_grp = new QButtonGroup(this);
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

    QButtonGroup *smode_grp = new QButtonGroup(this);
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
    detachFromView();
    bu_ptbl_free(&pf->selected_set);
    bu_ptbl_free(&bf->selected_set);
    bu_ptbl_free(&rf->selected_set);
    delete pf;
    delete bf;
    delete rf;
}

void
CADViewSelector::attachToView(QgView *view)
{
    if (!view || m_view == view)
	return;
    detachFromView();
    m_view = view;
    m_view->add_event_filter(this);
}

void
CADViewSelector::detachFromView(QgView *view)
{
    if (!m_view || (view && m_view != view))
	return;
    m_view->clear_event_filter(this);
    m_view = nullptr;
}

void
CADViewSelector::enable_groups(bool)
{
    group_contents->setEnabled(true);
    draw_selections->setEnabled(true);
    erase_selections->setEnabled(true);
}

void
CADViewSelector::disable_groups(bool)
{
    group_contents->setEnabled(false);
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
    struct ged *gedp = m_context ? m_context->getGed() : NULL;
    if (!gedp)
	return;

    const QByteArray set_name = cad_selection_set_name(m_context);
    const char *set_key = set_name.isEmpty() ? NULL : set_name.constData();
    unsigned long long chash = ged_selection_state_hash(gedp, set_key);
    if ((flags & QG_VIEW_SELECT) || chash != ohash) {
	group_contents->clear();
	ohash = chash;

	std::vector<std::string> paths = cad_selected_paths(gedp, set_key);
	for (const std::string &path : paths)
	    group_contents->addItem(QString::fromLocal8Bit(path.c_str()));
    }
}

void
CADViewSelector::select_objs()
{
    struct ged *gedp = m_context ? m_context->getGed() : NULL;
    if (!gedp || !cf)
	return;
    const QByteArray set_name = cad_selection_set_name(m_context);
    const char *set_key = set_name.isEmpty() ? NULL : set_name.constData();
    (void)ged_selection_batch_begin(gedp);
    for (size_t i = 0; i < BU_PTBL_LEN(&cf->selected_set); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cf->selected_set, i);
	if (!s || !ged_selection_select_path(gedp, set_key,
		bu_vls_cstr(&s->s_name), 0)) {
	    (void)ged_selection_batch_end(gedp);
	    return;
	}
    }
    (void)ged_selection_recompute(gedp, set_key);
    (void)ged_selection_batch_end(gedp);
}

void
CADViewSelector::deselect_objs()
{
    struct ged *gedp = m_context ? m_context->getGed() : NULL;
    if (!gedp || !cf)
	return;
    const QByteArray set_name = cad_selection_set_name(m_context);
    const char *set_key = set_name.isEmpty() ? NULL : set_name.constData();
    (void)ged_selection_batch_begin(gedp);
    for (size_t i = 0; i < BU_PTBL_LEN(&cf->selected_set); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cf->selected_set, i);
	if (!s || !ged_selection_deselect_path(gedp, set_key,
		bu_vls_cstr(&s->s_name), 0)) {
	    (void)ged_selection_batch_end(gedp);
	    return;
	}
    }
    (void)ged_selection_recompute(gedp, set_key);
    (void)ged_selection_batch_end(gedp);
}


int
CADViewSelector::erase_objs()
{
    struct ged *gedp = m_context ? m_context->getGed() : NULL;
    ged_view_context *view = m_context ? m_context->activeViewContext() : NULL;
    if (!gedp || !view || !cf)
	return 0;

    std::vector<std::string> paths;
    paths.reserve(BU_PTBL_LEN(&cf->selected_set));
    for (size_t i = 0; i < BU_PTBL_LEN(&cf->selected_set); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cf->selected_set, i);
	if (!s)
	    continue;
	paths.push_back(bu_vls_cstr(&s->s_name));
    }

    int changed = 0;
    return cad_scene_erase_paths(gedp, view, paths, &changed) ==
	GED_SCENE_OK && changed;
}

void
CADViewSelector::do_draw_selections()
{
    struct ged *gedp = m_context ? m_context->getGed() : NULL;
    ged_view_context *view = m_context ? m_context->activeViewContext() : NULL;
    if (!gedp || !view)
	return;

    const QByteArray set_name = cad_selection_set_name(m_context);
    const char *set_key = set_name.isEmpty() ? NULL : set_name.constData();
    std::vector<std::string> paths = cad_selected_paths(gedp, set_key);
    if (paths.empty())
	return;

    int changed = 0;
    if (cad_scene_draw_paths(gedp, view, paths, &changed) ==
	GED_SCENE_OK && changed)
	emit view_changed(QG_VIEW_DRAWN|QG_VIEW_SELECT);
}

void
CADViewSelector::do_erase_selections()
{
    struct ged *gedp = m_context ? m_context->getGed() : NULL;
    ged_view_context *view = m_context ? m_context->activeViewContext() : NULL;
    if (!gedp || !view)
	return;

    const QByteArray set_name = cad_selection_set_name(m_context);
    const char *set_key = set_name.isEmpty() ? NULL : set_name.constData();
    std::vector<std::string> paths = cad_selected_paths(gedp, set_key);
    if (paths.empty())
	return;

    int changed = 0;
    if (cad_scene_erase_paths(gedp, view, paths, &changed) ==
	GED_SCENE_OK && changed)
	emit view_changed(QG_VIEW_DRAWN|QG_VIEW_SELECT);
}

bool
CADViewSelector::eventFilter(QObject *o, QEvent *e)
{
    if (QApplication::keyboardModifiers() != Qt::NoModifier)
	return false;

    struct ged *gedp = m_context ? m_context->getGed() : NULL;
    struct bview *v = m_context ? m_context->activeViewContext() : NULL;
    if (!gedp || !v)
	return false;

    // Set the libqtcad filter based on current options
    cf = pf;
    if (use_rect_select_button->isChecked())
	cf = bf;
    if (use_pnt_select_button->isChecked() &&
	use_ray_test_ckbx->isChecked()) {
	rf->dbip = gedp->dbip;
	cf = rf;
    }

    // Inform the filter of the current settings and view
    cf->v = v;
    cf->first_only = use_rect_select_button->isChecked() ? false :
	!select_all_depth_ckbx->isChecked();

    // TODO - create and/or connect the signals and slots so cf can
    // properly trigger updating and drawing
    bool ret = cf->eventFilter(o, e);
    if (!ret)
	return false;

    if (erase_from_scene_button->isChecked()) {
	if (erase_objs())
	    emit view_changed(QG_VIEW_DRAWN);
	bu_ptbl_reset(&cf->selected_set);
	return true;
    }

    if (add_to_group_button->isChecked()) {
	select_objs();
	emit view_changed(QG_VIEW_DRAWN|QG_VIEW_SELECT);
	bu_ptbl_reset(&cf->selected_set);
	return true;
    }

    if (rm_from_group_button->isChecked()) {
	deselect_objs();
	emit view_changed(QG_VIEW_DRAWN|QG_VIEW_SELECT);
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
