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
#include <vector>
#include "qtcad/QgUtil.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgTreeSelectionModel.h"

void
QgTreeSelectionModel::select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags flags)
{
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
    if (!ged_doing_sync) {
	void (*tmp)(struct ged_selection_set *) = gedp->ged_select_callback;
	gedp->ged_select_callback = NULL;
	if (!(flags & QItemSelectionModel::Deselect)) {
	    QModelIndexList dl = selection.indexes();
	    std::queue<QgItem *> to_process;
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
		QgItem *itm = to_process.front();
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
	}

	gedp->ged_select_callback = tmp;
    }

    QItemSelectionModel::select(selection, flags);
}

void
QgTreeSelectionModel::select(const QModelIndex &index, QItemSelectionModel::SelectionFlags flags)
{

    if (!ged_doing_sync && !(flags & QItemSelectionModel::Deselect)) {
	std::queue<QgItem *> to_process;
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
		QgItem *itm = to_process.front();
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
QgTreeSelectionModel::ged_sync(QgItem *start, struct ged_selection_set *gs)
{
    if (!gs)
	return;

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
	    bu_log("ged sync select\n");
	    select(pind, QItemSelectionModel::Select);
	} else {
	    select(pind, QItemSelectionModel::Deselect);
	}
    }

    ged_doing_sync = false;
}

void
QgTreeSelectionModel::illuminate(const QItemSelection &selected, const QItemSelection &deselected)
{
    // Probably not possible?
    if (!selected.size() && !deselected.size())
	return;
    // TODO - The GED syncing and selected path calculations need to happen
    // before we get to this point.  Probably what is needed is a custom
    // subclassing of QItemSelectionModel with a select that deals with these
    // ged operations.
    QgModel *m = treeview->m;
    struct ged *gedp = m->gedp;
#if 0
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
	std::cout << "deselected: " << bu_vls_cstr(&tpath) << "\n";
	ged_selection_remove(gs, bu_vls_cstr(&tpath));
    }
    std::cout << "TODO: update any illumination flags that need unsetting on already drawn objects\n";

    QModelIndexList sl = selected.indexes();
    for (long int i = 0; i < sl.size(); i++) {
	QgItem *snode = static_cast<QgItem *>(sl.at(i).internalPointer());
	QString nstr = snode->toString();
	bu_vls_sprintf(&tpath, "%s", nstr.toLocal8Bit().data());
	std::cout << "selected: " << bu_vls_cstr(&tpath) << "\n";
	ged_selection_insert(gs, bu_vls_cstr(&tpath));
    }
    std::cout << "TODO: update any illumination flags that need setting on already drawn objects\n";
#endif
    emit m->view_change(&gedp->ged_gvp);
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

