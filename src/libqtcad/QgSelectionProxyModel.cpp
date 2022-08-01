/*            Q G S E L E C T I O N P R O X Y M O D E L . C P P
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
/** @file QgSelectionProxyModel.cpp
 *
 */

#include "common.h"
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <vector>
#include "qtcad/QgUtil.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgSelectionProxyModel.h"
#include "qtcad/QgTreeView.h"

QModelIndex
QgSelectionProxyModel::NodeIndex(QgItem *node) const
{
    QgModel *m = (QgModel *)sourceModel();
    if (node == m->root()) return QModelIndex();
    return createIndex(NodeRow(node), 0, node);
}

int
QgSelectionProxyModel::NodeRow(QgItem *node) const
{
    if (!node->parent())
	return 0;
    std::vector<QgItem *> &v = node->parent()->children;
    std::vector<QgItem *>::iterator it = std::find(v.begin(), v.end(), node);
    if (it == v.end())
	return 0;
    int ind = it - v.begin();
    return ind;
}

QVariant
QgSelectionProxyModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid()) return QVariant();
    QgItem *curr_node = static_cast<QgItem *>(idx.internalPointer());
    if (role == Qt::DisplayRole) {
	return QVariant(bu_vls_cstr(&curr_node->name));
    }
    if (role == BoolInternalRole) {
	return QVariant(curr_node->op);
    }
    if (role == DirectoryInternalRole)
	return QVariant::fromValue((void *)(curr_node->dp));

    if (role == TypeIconDisplayRole)
	return QVariant(curr_node->icon);
    if (role == HighlightDisplayRole) {
	return curr_node->instance()->active_flag;
    }
    return QVariant();
}

void QgSelectionProxyModel::mode_change(int i)
{
    if (i != interaction_mode && treeview) {
	interaction_mode = i;
	update_selected_node_relationships(treeview->selected());
    }
}

void
QgSelectionProxyModel::item_collapsed(const QModelIndex &index)
{
    QgModel *m = (QgModel *)sourceModel();
    QgItem *itm = m->getItem(index);
    itm->open_itm = false;
}

void
QgSelectionProxyModel::item_expanded(const QModelIndex &index)
{
    QgModel *m = (QgModel *)sourceModel();
    QgItem *itm = m->getItem(index);
    itm->open_itm = true;
}

// TODO - In the new setup much of the old setting here is moot - these
// properties should be looked up from the QgItem or its entries.  The
// implications for setData in a modifiable tree may actually be changing the
// .g contents, which is an altogether different proposition from the old
// logic.  It is not clear to me yet that we will actually change QgItem
// contents via this mechanism.
bool
QgSelectionProxyModel::setData(const QModelIndex & idx, const QVariant &UNUSED(value), int UNUSED(role))
{
    if (!idx.isValid()) return false;
    return false;
}

void
QgSelectionProxyModel::illuminate(const QItemSelection &selected, const QItemSelection &deselected)
{
    // Probably not possible?
    if (!selected.size() && !deselected.size())
	return;

    // TODO - The GED syncing and selected path calculations need to happen before we
    // get to this point.  Probably what is needed is a custom subclassing of
    // QItemSelectionModel with a select that deals with these ged operations.
    QgModel *m = (QgModel *)sourceModel();
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
// TODO - this may be the correct place to also illuminate or de-illuminate
// the drawn solids in response to tree selections, in which case this
// method name should be generalized...
void
QgSelectionProxyModel::update_selected_node_relationships(const QModelIndex &idx)
{
    std::unordered_map<unsigned long long, gInstance *>::iterator g_it;

    // Clear all highlighting state
    QgModel *m = (QgModel *)sourceModel();
    for (g_it = m->instances->begin(); g_it != m->instances->end(); g_it++) {
	g_it->second->active_flag = 0;
    }

    if (!idx.isValid() || interaction_mode == QgViewMode) {
	// For the case of a selection change, emit a layout change signal so the drawBranches call updates
	// the portions of the row colors not handled by the itemDelegate painting.  For expand and close
	// operations on items this is already handled by Qt, but layout updating is not a normal part of the selection
	// process in most tree views so for the customized selection drawing we do we need to call it manually.
	emit layoutChanged();
	return;
    }

    QgItem *snode = static_cast<QgItem *>(idx.internalPointer());

    if (!snode) {
	emit layoutChanged();
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

	// For gInstances where the child dp == the parent (i.e. the comb directly
	// containing the instance), set to 2
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
	// For the case of a selection change, emit a layout change signal so the drawBranches call updates
	// the portions of the row colors not handled by the itemDelegate painting.  For expand and close
	// operations on items this is already handled by Qt, but layout updating is not a normal part of the selection
	// process in most tree views so for the customized selection drawing we do we need to call it manually.
	emit layoutChanged();
	return;
    }

    // If we're in QgPrimitiveEditMode, we have slightly different work to do -
    // we need to check the gInstances to see if anybody else is using the same
    // dp as sg - if so, they are also directly being altered in this mode
    // since their underlying primitive may change.  We then need to flag all
    // the parent instances of all the activated gInstances (and all their
    // parents) so the tree will know which combs to highlight to indicate there
    // is a relevant object in the subtree.
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
	// For the case of a selection change, emit a layout change signal so the drawBranches call updates
	// the portions of the row colors not handled by the itemDelegate painting.  For expand and close
	// operations on items this is already handled by Qt, but layout updating is not a normal part of the selection
	// process in most tree views so for the customized selection drawing we do we need to call it manually.
	emit layoutChanged();
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

