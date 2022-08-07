/*            Q G T R E E S E L E C T I O N M O D E L . C P P
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
/** @file QgTreeSelectionModel.cpp
 *
 */

#include "common.h"
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <stack>
#include <vector>
#include "qtcad/QgUtil.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgTreeSelectionModel.h"
#include "qtcad/SignalFlags.h"

void
QgTreeSelectionModel::select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags flags)
{
    QgModel *m = treeview->m;
    struct ged *gedp = m->gedp;
    struct ged_selection_set *gs = gedp->ged_cset;
    if (!ged_doing_sync) {
	if (!(flags & QItemSelectionModel::Deselect)) {
	    QModelIndexList dl = selection.indexes();
	    std::stack<QgItem *> to_process;
	    for (long int i = 0; i < dl.size(); i++) {
		QgItem *snode = static_cast<QgItem *>(dl.at(i).internalPointer());
		QgItem *pnode = snode->parent();
		while (pnode) {
		    QString nstr = pnode->toString();
		    if (nstr == QString())
			break;
		    to_process.push(pnode);
		    pnode = pnode->parent();
		}

		std::queue<QgItem *> get_children;
		if (snode->children.size()) {
		    get_children.push(snode);
		}
		while (!get_children.empty()) {
		    QgItem *cnode = get_children.front();
		    get_children.pop();
		    for (size_t j = 0; j < cnode->children.size(); j++) {
			QgItem *ccnode = cnode->children[j];
			get_children.push(ccnode);
			to_process.push(ccnode);
		    }
		}
	    }
	    while (!to_process.empty()) {
		QgItem *itm = to_process.top();
		to_process.pop();
		QModelIndex pind = itm->ctx->NodeIndex(itm);
		select(pind, QItemSelectionModel::Deselect);
	    }

	    if (gs) {
		for (long int i = 0; i < dl.size(); i++) {
		    QgItem *snode = static_cast<QgItem *>(dl.at(i).internalPointer());
		    struct bu_vls tpath = BU_VLS_INIT_ZERO;
		    QString nstr = snode->toString();
		    bu_vls_sprintf(&tpath, "%s", nstr.toLocal8Bit().data());
		    std::cout << "ged insert: " << bu_vls_cstr(&tpath) << "\n";
		    ged_selection_insert(gs, bu_vls_cstr(&tpath));
		    bu_vls_free(&tpath);
		}
	    }

	    emit treeview->view_changed(QTCAD_VIEW_SELECT);

	} else {

	    QModelIndexList dl = selection.indexes();
	    std::queue<QgItem *> to_process;
	    for (long int i = 0; i < dl.size(); i++) {
		QgItem *snode = static_cast<QgItem *>(dl.at(i).internalPointer());
		QString nstr = snode->toString();
		if (nstr != QString()) {
		    struct bu_vls tpath = BU_VLS_INIT_ZERO;
		    bu_vls_sprintf(&tpath, "%s", nstr.toLocal8Bit().data());
		    std::cout << "ged remove: " << bu_vls_cstr(&tpath) << "\n";
		    ged_selection_remove(gs, bu_vls_cstr(&tpath));
		    bu_vls_free(&tpath);
		}
	    }

	    emit treeview->view_changed(QTCAD_VIEW_SELECT);
	}
    }

    QItemSelectionModel::select(selection, flags);
}

void
QgTreeSelectionModel::select(const QModelIndex &index, QItemSelectionModel::SelectionFlags flags)
{
    if (!ged_doing_sync && !(flags & QItemSelectionModel::Deselect)) {
	std::stack<QgItem *> to_process;
	QgItem *snode = static_cast<QgItem *>(index.internalPointer());
	if (snode) {
	    QgItem *pnode = snode->parent();
	    while (pnode) {
		QString nstr = pnode->toString();
		if (nstr == QString())
		    break;
		to_process.push(pnode);
		pnode = pnode->parent();
	    }

	    std::queue<QgItem *> get_children;
	    if (snode->children.size()) {
		get_children.push(snode);
	    }
	    while (!get_children.empty()) {
		QgItem *cnode = get_children.front();
		get_children.pop();
		for (size_t j = 0; j < cnode->children.size(); j++) {
		    QgItem *ccnode = cnode->children[j];
		    get_children.push(ccnode);
		    to_process.push(ccnode);
		}
	    }
	    while (!to_process.empty()) {
		QgItem *itm = to_process.top();
		to_process.pop();
		QModelIndex pind = itm->ctx->NodeIndex(itm);
		select(pind, QItemSelectionModel::Deselect);
	    }
	} else {
	    // If we have an empty selection, clear the GED set
	    QgModel *m = treeview->m;
	    struct ged *gedp = m->gedp;
	    struct ged_selection_set *gs = NULL;
	    if (gedp->ged_selection_sets) {
		struct bu_ptbl ssets = BU_PTBL_INIT_ZERO;
		size_t scnt = ged_selection_sets_lookup(&ssets, gedp->ged_selection_sets, "default");
		if (scnt == 1)
		    gs = (struct ged_selection_set *)BU_PTBL_GET(&ssets, 0);
		bu_ptbl_free(&ssets);
	    }
	    if (gs) {
		ged_selection_set_clear(gs);
	    }
	}

    }

    QItemSelectionModel::select(index, flags);
}

void
QgTreeSelectionModel::mode_change(int i)
{
    if (i != interaction_mode && treeview) {
	interaction_mode = i;
	update_selected_node_relationships(treeview->selected());
    }
}

void
QgTreeSelectionModel::ged_selection_sync(QgItem *start, struct ged_selection_set *gs)
{
    if (!gs || ged_doing_sync)
	return;

    char **contents = NULL;
    int scnt = ged_selection_set_list(&contents, gs);
    for (int i = 0; i < scnt; i++) {
	bu_log("%s\n", contents[i]);
    }

    ged_doing_sync = true;
    QgModel *m = treeview->m;
    std::queue<QgItem *> to_check;
    if (start && start != m->root()) {
	to_check.push(start);
    } else {
	for (size_t i = 0; i < m->root()->children.size(); i++) {
	    to_check.push(m->root()->children[i]);
	}
    }
    struct bu_vls pstr = BU_VLS_INIT_ZERO;
    while (!to_check.empty()) {
	QgItem *cnode = to_check.front();
	to_check.pop();
	for (size_t i = 0; i < cnode->children.size(); i++) {
	    to_check.push(cnode->children[i]);
	}
	QString qstr = cnode->toString();
	bu_vls_sprintf(&pstr, "%s", qstr.toLocal8Bit().data());
	QModelIndex pind = cnode->ctx->NodeIndex(cnode);
	if (ged_selection_find(gs, bu_vls_cstr(&pstr))) {
	    bu_log("ged sync select: %s\n", bu_vls_cstr(&pstr));
	    select(pind, QItemSelectionModel::Select);
	} else {
	    select(pind, QItemSelectionModel::Deselect);
	}
    }

    ged_doing_sync = false;
}

static void
_fp_path_split(std::vector<std::string> &objs, const char *str)
{
    std::string s(str);
    size_t pos = 0;
    if (s.c_str()[0] == '/')
	s.erase(0, 1);  //Remove leading slash
    while ((pos = s.find_first_of("/", 0)) != std::string::npos) {
	std::string ss = s.substr(0, pos);
	objs.push_back(ss);
	s.erase(0, pos + 1);
    }
    objs.push_back(s);
}

static bool
_path_top_match(std::vector<std::string> &top, std::vector<std::string> &candidate)
{
    for (size_t i = 0; i < top.size(); i++) {
	if (i == candidate.size())
	    return false;
	if (top[i] != candidate[i])
	    return false;
    }
    return true;
}

/* There are three possible determinations - 0, which is not drawn, 1, which is
 * fully drawn, and 2 which is partially drawn */
static int
_get_draw_state(std::vector<std::vector<std::string>> &view_objs, std::vector<std::string> &candidate)
{
    for (size_t i = 0; i < view_objs.size(); i++) {
	// If view_objs is a top path for candidate, it is fully drawn
	if (_path_top_match(view_objs[i], candidate))
	    return 1;
	// If we don't have a match, see if the candidate is a top path of a
	// view_obj.  If so, candidate is partially drawn
	if (_path_top_match(candidate, view_objs[i]))
	    return 2;
    }

    return 0;
}

void
QgTreeSelectionModel::ged_drawn_sync(QgItem *start, struct ged *gedp)
{
    if (!gedp)
	return;

    bu_log("ged_drawn_sync\n");

    // Query from GED to get the drawn view objects (i.e. the "who" list).
    // This will tell us what the current drawn state is.
    std::vector<std::vector<std::string>> view_objs;
    struct bu_ptbl *sg = bv_view_objs(gedp->ged_gvp, BV_DB_OBJS);
    if (!sg)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
	std::vector<std::string> cg_p;
	_fp_path_split(cg_p, bu_vls_cstr(&cg->s_name));
	view_objs.push_back(cg_p);
    }

    ged_doing_sync = true;

    QgModel *m = treeview->m;
    std::queue<QgItem *> to_check;
    if (start && start != m->root()) {
	to_check.push(start);
    } else {
	for (size_t i = 0; i < m->root()->children.size(); i++) {
	    to_check.push(m->root()->children[i]);
	}
    }

    // Note that we always have to visit all the QgItem children, regardless of
    // parent drawn determinations, to make sure all the flags are consistent
    // with the current state.  We don't have to do path tests if the parent
    // state is decisive, but we do need to track and set the flags.
    struct bu_vls pstr = BU_VLS_INIT_ZERO;
    while (!to_check.empty()) {
	QgItem *cnode = to_check.front();
	to_check.pop();
	QString qstr = cnode->toString();
	bu_vls_sprintf(&pstr, "%s", qstr.toLocal8Bit().data());
	std::vector<std::string> cg_p;
	_fp_path_split(cg_p, bu_vls_cstr(&pstr));
	cnode->draw_state = _get_draw_state(view_objs, cg_p);
	if (cnode->draw_state == 0) {
	    // Not drawn - none of the children are drawn either, zero
	    // all of them out.
	    std::queue<QgItem *> to_clear;
	    for (size_t i = 0; i < cnode->children.size(); i++) {
		to_clear.push(cnode->children[i]);
	    }
	    while (!to_clear.empty()) {
		QgItem *nnode = to_clear.front();
		to_clear.pop();
		nnode->draw_state = 0;
		for (size_t i = 0; i < nnode->children.size(); i++) {
		    to_clear.push(nnode->children[i]);
		}
	    }
	    continue;
	}
	if (cnode->draw_state == 1) {
	    // Fully drawn - all of the children are drawn, set
	    // all of them.
	    std::queue<QgItem *> to_set;
	    for (size_t i = 0; i < cnode->children.size(); i++) {
		to_set.push(cnode->children[i]);
	    }
	    while (!to_set.empty()) {
		QgItem *nnode = to_set.front();
		to_set.pop();
		nnode->draw_state = 1;
		for (size_t i = 0; i < nnode->children.size(); i++) {
		    to_set.push(nnode->children[i]);
		}
	    }
	    continue;
	}

	// Partially drawn.  cnode draw state is set to 2 - we need to look
	// more closely at the children
	for (size_t i = 0; i < cnode->children.size(); i++) {
	    to_check.push(cnode->children[i]);
	}
    }

    ged_doing_sync = false;
}

void
QgTreeSelectionModel::ged_deselect(const QItemSelection &UNUSED(selected), const QItemSelection &deselected)
{
    if (!deselected.size() || ged_doing_sync)
	return;
    QgModel *m = treeview->m;
    struct ged *gedp = m->gedp;
    if (!gedp->ged_selection_sets)
	return;
    struct bu_ptbl ssets = BU_PTBL_INIT_ZERO;
    size_t scnt = ged_selection_sets_lookup(&ssets, gedp->ged_selection_sets, "default");
    if (scnt != 1) {
	bu_ptbl_free(&ssets);
	return;
    }
    struct ged_selection_set *gs = (struct ged_selection_set *)BU_PTBL_GET(&ssets, 0);
    bu_ptbl_free(&ssets);

    struct bu_vls tpath = BU_VLS_INIT_ZERO;
    QModelIndexList dl = deselected.indexes();
    for (long int i = 0; i < dl.size(); i++) {
	QgItem *snode = static_cast<QgItem *>(dl.at(i).internalPointer());
	QString nstr = snode->toString();
	bu_vls_sprintf(&tpath, "%s", nstr.toLocal8Bit().data());
	ged_selection_remove(gs, bu_vls_cstr(&tpath));
    }
    emit treeview->view_changed(QTCAD_VIEW_SELECT);
}

// These functions tell the related-object highlighting logic what the current
// status is.  We need to do this whenever the selected object is changed -
// highlighted items may change anywhere (or everywhere) in the tree view with
// any operation, so we have to check everything.
//
// Note that we're setting flags here, not deciding whether to highlight a
// particular QgItem.  That decision will be made later, and is a combination
// of active_flag status and the open status of the QgItem.
//
// TODO - this may be the correct place to also illuminate or de-illuminate the
// drawn solids in response to tree selections, in which case this method name
// should be generalized...
void
QgTreeSelectionModel::update_selected_node_relationships(const QModelIndex &idx)
{
    std::unordered_map<unsigned long long, gInstance *>::iterator g_it;

    // Clear all highlighting state
    QgModel *m = treeview->m;
    if (!m || !m->instances)
	return;

    for (g_it = m->instances->begin(); g_it != m->instances->end(); g_it++) {
	g_it->second->active_flag = 0;
    }

    if (!idx.isValid() || interaction_mode == QgViewMode) {
	// For the case of a selection change, emit a layout change signal so
	// the drawBranches call updates the portions of the row colors not
	// handled by the itemDelegate painting.  For expand and close
	// operations on items this is already handled by Qt, but layout
	// updating is not a normal part of the selection process in most tree
	// views so for the customized selection drawing we do we need to call
	// it manually.
	emit m->layoutChanged();
	return;
    }

    QgItem *snode = static_cast<QgItem *>(idx.internalPointer());

    if (!snode) {
	emit m->layoutChanged();
	return;
    }

    gInstance *sg = snode->instance();

    std::unordered_set<gInstance *> processed;
    processed.insert(sg);

    std::queue<gInstance *> to_flag;
    // If we're in QgInstanceEditMode, we key off of the exact gInstance
    // pointer that corresponds to this instance.  Since that instance is
    // unique, we only need to flag parent instances (and all the parents of
    // those parents) so the parent QgItems know to highlight themselves if
    // their children are closed.
    if (interaction_mode == QgInstanceEditMode) {

	sg->active_flag = 3;

	// For gInstances where the child dp == the parent (i.e. the comb
	// directly containing the instance), set to 2
	for (g_it = m->instances->begin(); g_it != m->instances->end(); g_it++) {
	    gInstance *cd = g_it->second;
	    if (cd->dp == sg->parent && processed.find(cd) == processed.end()) {
		cd->active_flag = 2;
		to_flag.push(cd);
		processed.insert(cd);
	    }
	}
	// For gInstances above the active instance, set to 1
	while (!to_flag.empty()) {
	    gInstance *curr = to_flag.front();
	    to_flag.pop();
	    if (!curr->parent)
		continue;
	    for (g_it = m->instances->begin(); g_it != m->instances->end(); g_it++) {
		gInstance *cd = g_it->second;
		if (cd->dp == curr->parent && processed.find(cd) == processed.end()) {
		    cd->active_flag = 1;
		    to_flag.push(cd);
		    processed.insert(cd);
		}
	    }
	}
	// For the case of a selection change, emit a layout change signal so
	// the drawBranches call updates the portions of the row colors not
	// handled by the itemDelegate painting.  For expand and close
	// operations on items this is already handled by Qt, but layout
	// updating is not a normal part of the selectio  n process in most
	// tree views so for the customized selection drawing we do we need to
	// call it manually.
	emit m->layoutChanged();
	return;
    }

    // If we're in QgPrimitiveEditMode, we have slightly different work to do -
    // we need to check the gInstances to see if anybody else is using the same
    // dp as sg - if so, they are also directly being altered in this mode
    // since their underlying primitive may change.  We then need to flag all
    // the parent instances of all the activated gInstances (and all their
    // parents) so the tree will know which combs to highlight to indicate
    // there is a relevant object in the subtree.
    if (interaction_mode == QgPrimitiveEditMode) {
	for (g_it = m->instances->begin(); g_it != m->instances->end(); g_it++) {
	    gInstance *cg = g_it->second;
	    if (cg->dp == sg->dp) {
		cg->active_flag = 2;
		to_flag.push(cg);
		processed.insert(cg);
	    }
	}
	// Iterate over parents and parents of parents of any gInstance with
	// active_flag set until all parents have been assigned an active_flag
	// of at least 1 (or two, if they are one of the exact matches from the
	// previous step.)
	while (!to_flag.empty()) {
	    gInstance *curr = to_flag.front();
	    to_flag.pop();
	    if (!curr->parent)
		continue;
	    for (g_it = m->instances->begin(); g_it != m->instances->end(); g_it++) {
		gInstance *cd = g_it->second;
		if (curr->parent == cd->dp && processed.find(cd) == processed.end()) {
		    cd->active_flag = 1;
		    to_flag.push(cd);
		    processed.insert(cd);
		}
	    }
	}
	// For the case of a selection change, emit a layout change signal so
	// the drawBranches call updates the portions of the row colors not
	// handled by the itemDelegate painting.  For expand and close
	// operations on items this is already handled by Qt, but layout
	// updating is not a normal part of the selectio  n process in most
	// tree views so for the customized selection drawing we do we need to
	// call it manually.
	emit m->layoutChanged();
	return;
    }

}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

