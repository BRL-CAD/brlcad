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
QgTreeSelectionModel::clear_all()
{
    QgModel *m = treeview->m;
    QgItem *snode = m->root();
    std::queue<QgItem *> get_children;
    if (snode->children.size())
	get_children.push(snode);

    while (!get_children.empty()) {
	QgItem *cnode = get_children.front();
	get_children.pop();
	cnode->select_state = 0;
	for (size_t j = 0; j < cnode->children.size(); j++) {
	    QgItem *ccnode = cnode->children[j];
	    get_children.push(ccnode);
	}
    }
}


void
QgTreeSelectionModel::select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags flags)
{
    QgModel *m = treeview->m;
    struct ged *gedp = m->gedp;

    std::vector<BSelectState *> ssv = gedp->dbi_state->get_selected_states(NULL);

    if (ssv.size() != 1)
	return;

    BSelectState *ss = ssv[0];

    if (flags & QItemSelectionModel::Clear)
	ss->clear();

#if 0
    QModelIndexList dl = selection.indexes();
    for (long int i = 0; i < dl.size(); i++) {
	QgItem *snode = static_cast<QgItem *>(dl.at(i).internalPointer());
#else
    // Above should work (and does on Linux) - using a Windows workaround from
    // https://stackoverflow.com/q/15123109/2037687 for the moment...
    QModelIndexList *dl = new QModelIndexList(selection.indexes());
    for (long int i = 0; i < dl->size(); i++) {
	QgItem *snode = static_cast<QgItem *>(dl->at(i).internalPointer());
#endif

	// If we are selecting an already selected node, clear it
	if (flags & QItemSelectionModel::Select && snode->select_state) {
	    std::vector<unsigned long long> path_hashes = snode->path_items();
	    ss->deselect_hpath(path_hashes);
	    snode->select_state = false;
	    // This toggle is a local operation
	    continue;
	}

	// If a node is being selected, all of its parents and children
	// will be de-selected
	if (!(flags & QItemSelectionModel::Deselect)) {
	    std::vector<unsigned long long> path_hashes = snode->path_items();
	    ss->select_hpath(path_hashes);
	    snode->select_state = true;
	} else {
	    std::vector<unsigned long long> path_hashes = snode->path_items();
	    ss->deselect_hpath(path_hashes);
	    snode->select_state = false;
	}
    }

    emit treeview->view_changed(QTCAD_VIEW_SELECT);
    emit treeview->m->layoutChanged();
}

void
QgTreeSelectionModel::select(const QModelIndex &index, QItemSelectionModel::SelectionFlags flags)
{
    QgModel *m = treeview->m;
    struct ged *gedp = m->gedp;

    std::vector<BSelectState *> ssv = gedp->dbi_state->get_selected_states(NULL);

    if (ssv.size() != 1)
	return;

    BSelectState *ss = ssv[0];

    if (flags & QItemSelectionModel::Clear)
	ss->clear();

    QgItem *snode = static_cast<QgItem *>(index.internalPointer());
    if (!snode) {
	ss->clear();
	return;
    }

    if (!(flags & QItemSelectionModel::Deselect)) {

	// If we are selecting an already selected node, clear it
	if (flags & QItemSelectionModel::Select && snode->select_state) {
	    std::vector<unsigned long long> path_hashes = snode->path_items();
	    ss->deselect_hpath(path_hashes);
	    snode->select_state = false;

	    // This toggle is a local operation
	    emit treeview->view_changed(QTCAD_VIEW_SELECT);
	    emit treeview->m->layoutChanged();
	    return;
	}

	std::vector<unsigned long long> path_hashes = snode->path_items();
	ss->select_hpath(path_hashes);
	snode->select_state = true;

    } else {

	std::vector<unsigned long long> path_hashes = snode->path_items();
	ss->deselect_hpath(path_hashes);
	snode->select_state = false;

    }

    emit treeview->view_changed(QTCAD_VIEW_SELECT);
    emit treeview->m->layoutChanged();
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
QgTreeSelectionModel::ged_selection_sync(QgItem *start, const char *sset)
{
    if (!sset)
	return;
    bu_log("ged_selection_sync\n");

    QgModel *m = treeview->m;
    struct ged *gedp = m->gedp;
    std::vector<BSelectState *> ssv = gedp->dbi_state->get_selected_states(sset);

    if (ssv.size() != 1)
	return;

    BSelectState *ss = ssv[0];

    std::queue<QgItem *> to_check;
    if (start && start != m->root()) {
	to_check.push(start);
    } else {
	clear_all();
	for (size_t i = 0; i < m->root()->children.size(); i++) {
	    to_check.push(m->root()->children[i]);
	}
    }
    while (!to_check.empty()) {
	QgItem *cnode = to_check.front();
	to_check.pop();
	for (size_t i = 0; i < cnode->children.size(); i++) {
	    to_check.push(cnode->children[i]);
	}
	std::vector<unsigned long long> path_hashes = cnode->path_items();
	unsigned long long phash = gedp->dbi_state->path_hash(path_hashes, 0);
	if (ss->is_selected(phash)) {
	    cnode->select_state = 1;
	} else {
	    cnode->select_state = 0;
	}
    }
}

void
QgTreeSelectionModel::ged_drawn_sync(QgItem *start, struct ged *gedp)
{
    if (!gedp)
	return;

    bu_log("ged_drawn_sync\n");

    BViewState *vs = gedp->dbi_state->get_view_state(gedp->ged_gvp);
    if (vs)
	return;

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
    while (!to_check.empty()) {
	QgItem *cnode = to_check.front();
	to_check.pop();
	std::vector<unsigned long long> path_hashes = cnode->path_items();
	unsigned long long phash = gedp->dbi_state->path_hash(path_hashes, 0);
	cnode->draw_state = vs->is_hdrawn(-1, phash);
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
    // Clear all highlighting state
    QgModel *m = treeview->m;
    if (!m)
	return;

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
}

#if 0
    std::vector<unsigned long long> parent_child;
    unsigned long long phash = (snode->parent) ? snode->parent->ihash : 0;
    unsigned long long chash = snode->ihash;
    unsigned long long ihash = m->gedp->dbi_state->path_hash(parent_child);

    std::unordered_set<unsigned long long> processed;
    processed.insert(ihash);

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
	std::unordered_map<unsigned long long, gInstance *>::iterator g_it;
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
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

