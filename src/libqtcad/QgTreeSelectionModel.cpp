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

    std::vector<BSelectState *> sv = m->gedp->dbi_state->get_selected_states(NULL);
    BSelectState *ss = sv[0];
    ss->clear();
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
	if (flags & QItemSelectionModel::Select && ss->is_selected(snode->path_hash())) {
	    std::vector<unsigned long long> path_hashes = snode->path_items();
	    ss->deselect_hpath(path_hashes);
	    // This toggle is a local operation
	    continue;
	} else {
	    if (flags & QItemSelectionModel::Clear)
		ss->clear();

	    std::vector<unsigned long long> path_hashes = snode->path_items();
	    ss->select_hpath(path_hashes);
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
	if (flags & QItemSelectionModel::Select && ss->is_selected(snode->path_hash())) {
	    std::vector<unsigned long long> path_hashes = snode->path_items();
	    ss->deselect_hpath(path_hashes);
	    emit treeview->view_changed(QTCAD_VIEW_SELECT);
	    emit treeview->m->layoutChanged();
	    return;
	}

	std::vector<unsigned long long> path_hashes = snode->path_items();
	ss->select_hpath(path_hashes);

    } else {

	std::vector<unsigned long long> path_hashes = snode->path_items();
	ss->deselect_hpath(path_hashes);

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

