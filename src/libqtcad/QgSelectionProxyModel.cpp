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

void
QgSelectionProxyModel::refresh(void *p)
{
    if (!p) {
	std::cerr << "Full update according to .g contents\n";
	QgModel *m = (QgModel *)sourceModel();
	m->refresh();
    }
}

QVariant
QgSelectionProxyModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid()) return QVariant();
    QgItem *curr_node = static_cast<QgItem *>(idx.internalPointer());
    QgModel *m = (QgModel *)sourceModel();
    gInstance *g = curr_node->instance();
    if (role == Qt::DisplayRole) {
	if (g->dp)
	    return QVariant(g->dp->d_namep);
	return QVariant();
    }
    if (role == BoolInternalRole) {
	switch (g->op) {
	    case DB_OP_UNION:
		return QVariant(0);
	    case DB_OP_SUBTRACT:
		return QVariant(1);
	    case DB_OP_INTERSECT:
		return QVariant(2);
	    default:
		return QVariant();
	}
    }
    if (role == DirectoryInternalRole)
	return QVariant::fromValue((void *)(g->dp));

    if (role == TypeIconDisplayRole)
	return QVariant(QgIcon(g->dp, (m->gedp) ? m->gedp->dbip : NULL));
#if 0
    if (role == RelatedHighlightDisplayRole) return curr_node->is_highlighted;
    if (role == InstanceHighlightDisplayRole) return curr_node->instance_highlight;
#endif
    return QVariant();
}

void QgSelectionProxyModel::mode_change(int i)
{
    if (i != interaction_mode && treeview) {
	interaction_mode = i;
	update_selected_node_relationships(treeview->selected());
    }
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

// These functions tell the related-object highlighting logic what the current
// status is.  We need to do this whenever the selected object is changed -
// highlighted items may change anywhere (or everywhere) in the tree view with
// any operation, so we have to check everything.
//
// Note that we're setting flags here, not deciding whether to highlight a
// particular QgItem.  That decision will be made later, and is a combination
// of active_flag status and the open status of the QgItem.
void
QgSelectionProxyModel::update_selected_node_relationships(const QModelIndex &idx)
{
    std::unordered_map<unsigned long long, gInstance *>::iterator g_it;

    if (!idx.isValid() || interaction_mode == QgViewMode) {
	// Clear all highlighting state - invalid selection or view mode means no related highlights
	QgModel *m = (QgModel *)sourceModel();
	for (g_it = m->instances->begin(); g_it != m->instances->end(); g_it++) {
	    g_it->second->active_flag = 0;
	}
	return;
    }

    QgModel *m = (QgModel *)sourceModel();
    QgItem *snode = static_cast<QgItem *>(idx.internalPointer());

    gInstance *sg = snode->instance();
    sg->active_flag = 2;

    std::unordered_set<gInstance *> processed;
    processed.insert(sg);

    std::queue<gInstance *> to_flag;
    to_flag.push(sg);

    // If we're in QgInstanceEditMode, we key off of the exact gInstance
    // pointer that corresponds to this instance.  Since that instance is
    // unique, we only need to flag parent instances (and all the parents of
    // those parents) so the parent QgItems know to highlight themselves if
    // their children are closed.
    if (interaction_mode == QgPrimitiveEditMode) {
	while (!to_flag.empty()) {
	    gInstance *curr = to_flag.front();
	    to_flag.pop();
	    for (g_it = m->instances->begin(); g_it != m->instances->end(); g_it++) {
		gInstance *cd = g_it->second;
		if (cd->parent == curr->dp && processed.find(cd) == processed.end()) {
		    cd->active_flag = 1;
		    to_flag.push(cd);
		    processed.insert(cd);
		}
	    }
	}
	return;
    }

    // If we're in QgPrimitiveEditMode, we have more work to do - we need to
    // check the gInstances to see if anybody else is using the same dp as sg -
    // if so, they are also directly being altered in this mode since their
    // underlying primitive may change.  We then need to flag all the parent
    // instances of all the activated gInstances (and all their parents)
    if (interaction_mode == QgPrimitiveEditMode) {
	for (g_it = m->instances->begin(); g_it != m->instances->end(); g_it++) {
	    gInstance *cg = g_it->second;
	    if (cg->dp == sg->dp) {
		g_it->second->active_flag = 2;
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
	    for (g_it = m->instances->begin(); g_it != m->instances->end(); g_it++) {
		gInstance *cd = g_it->second;
		if (cd->parent == curr->dp && processed.find(cd) == processed.end()) {
		    cd->active_flag = 1;
		    to_flag.push(cd);
		    processed.insert(cd);
		}
	    }
	}
	return;
    }

    // For the case of a selection change, emit a layout change signal so the drawBranches call updates
    // the portions of the row colors not handled by the itemDelegate painting.  For expand and close
    // operations on items this is already handled by Qt, but layout updating is not a normal part of the selection
    // process in most tree views so for the customized selection drawing we do we need to call it manually.
    emit layoutChanged();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

