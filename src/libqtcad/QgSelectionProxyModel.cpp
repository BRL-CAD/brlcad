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
#include "qtcad/QgUtil.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgSelectionProxyModel.h"
#include "qtcad/QgTreeView.h"

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
    if (!idx.isValid() || interaction_mode == QgViewMode) {
	// Clear all highlighting state - invalid selection or view mode means no related highlights
	QgModel *m = (QgModel *)sourceModel();
	std::unordered_map<unsigned long long, gInstance *>::iterator g_it;
	for (g_it = m->instances->begin(); g_it != m->instances->end(); g_it++) {
	    g_it->second->active_flag = 0;
	}
	return;
    }

    QgModel *m = (QgModel *)sourceModel();
    QgItem *snode = static_cast<QgItem *>(idx.internalPointer());
    gInstance *sg = snode->instance();
    sg->active_flag = 2;

    // If we're in QgInstanceEditMode, we key off of the exact gInstance
    // pointer that corresponds to this instance.  Since that instance is
    // unique, we only need to flag parent instances (and all the parents of
    // those parents) so the parent QgItems know to highlight themselves if
    // their children are closed.
    if (interaction_mode == QgPrimitiveEditMode) {
	// TODO - iterate over parents and parents of parents of any gInstance
	// with active_flag == 2 until all parents have been assigned an
	// active_flag of 1
	return;
    }

    // If we're in QgPrimitiveEditMode, we have more work to do - we need to
    // check the gInstances to see if anybody else is using the same dp as sg -
    // if so, they are also directly being altered in this mode since their
    // underlying primitive may change.  We then need to flag all the parent
    // instances of all the activated gInstances (and all their parents)
    if (interaction_mode == QgPrimitiveEditMode) {
	std::unordered_map<unsigned long long, gInstance *>::iterator g_it;
	for (g_it = m->instances->begin(); g_it != m->instances->end(); g_it++) {
	    gInstance *cg = g_it->second;
	    if (cg->dp == sg->dp)
		g_it->second->active_flag = 2;
	}
	// TODO - iterate over parents and parents of parents of any gInstance
	// with active_flag == 2 until all parents have been assigned an
	// active_flag of 1
	return;
    }

#if 0
    struct directory *selected_dp = RT_DIR_NULL;
    struct directory *instance_dp = RT_DIR_NULL;
    current_idx = idx;
    if (interaction_mode && idx.isValid()) {
	if (interaction_mode == 1) {
	    selected_dp = (struct directory *)(idx.parent().data(DirectoryInternalRole).value<void *>());
	    instance_dp = (struct directory *)(idx.data(DirectoryInternalRole).value<void *>());
	}
	if (interaction_mode == 2)
	    selected_dp = (struct directory *)(idx.data(DirectoryInternalRole).value<void *>());
    }

    if (selected_dp != RT_DIR_NULL) {
	foreach (CADTreeNode *test_node, all_nodes) {
	    QModelIndex test_index = NodeIndex(test_node);
	    int hs = test_index.data(RelatedHighlightDisplayRole).toInt();
	    int is = test_index.data(InstanceHighlightDisplayRole).toInt();
	    if (selected_dp != test_node->node_dp && instance_dp != test_node->node_dp) {
		if (is) setData(test_index, QVariant(0), InstanceHighlightDisplayRole);
		if (test_node->node_dp != RT_DIR_NULL && test_node->node_dp->d_flags & RT_DIR_COMB) {
		    if (!cadtreeview->isExpanded(test_index)) {
			int depth = 0;
			int search_results = 0;
			db_find_obj(&search_results, selected_dp->d_namep, test_node->node_dp, dbip, &depth, CADTREE_RECURSION_LIMIT, &combinternals);
			if (search_results && !hs) setData(test_index, QVariant(1), RelatedHighlightDisplayRole);
			if (!search_results && hs) setData(test_index, QVariant(0), RelatedHighlightDisplayRole);
		    } else {
			if (hs) setData(test_index, QVariant(0), RelatedHighlightDisplayRole);
		    }
		} else {
		    if (hs) setData(test_index, QVariant(0), RelatedHighlightDisplayRole);
		}
	    } else {
		if (interaction_mode == 1) {
		    int node_state = 0;
		    if (hs) setData(test_index, QVariant(0), RelatedHighlightDisplayRole);
		    if (instance_dp == test_node->node_dp && IndexNode(test_index.parent())->node_dp == selected_dp) node_state = 1;
		    if (selected_dp == test_node->node_dp) node_state = 1;
		    if (test_index == idx) node_state = 0;
		    if (node_state && instance_dp == test_node->node_dp && test_index.row() != idx.row()) node_state = 0;
		    if (node_state && !is) setData(test_index, QVariant(1), InstanceHighlightDisplayRole);
		    if (!node_state && is) setData(test_index, QVariant(0), InstanceHighlightDisplayRole);
		}
		if (interaction_mode == 2) {
		    if (is) setData(test_index, QVariant(0), InstanceHighlightDisplayRole);
		    if (test_index != idx) {
			if (!hs) setData(test_index, QVariant(1), RelatedHighlightDisplayRole);
		    } else {
			if (hs) setData(test_index, QVariant(0), RelatedHighlightDisplayRole);
		    }
		}
	    }
	}
    } else {
	foreach (CADTreeNode *test_node, all_nodes) {
	    QModelIndex test_index = NodeIndex(test_node);
	    int hs = test_index.data(RelatedHighlightDisplayRole).toInt();
	    if (hs) setData(test_index, QVariant(0), RelatedHighlightDisplayRole);
	    int is = test_index.data(InstanceHighlightDisplayRole).toInt();
	    if (is) setData(test_index, QVariant(0), InstanceHighlightDisplayRole);
	}
    }
#endif

    // For the case of a selection change, emit a layout change signal so the drawBranches call updates
    // the portions of the row colors not handled by the itemDelegate painting.  For expand and close
    // operations on items this is already handled by Qt, but layout updating is not a normal part of the selection
    // process in most tree views so for the customized selection drawing we do we need to call it manually.
    emit layoutChanged();
}

// When an item is expanded but the selection hasn't changed, scope for highlighting changes is more mimimal
void
QgSelectionProxyModel::expand_tree_node_relationships(const QModelIndex &UNUSED(idx))
{
#if 0
    struct directory *selected_dp = RT_DIR_NULL;
    struct directory *instance_dp = RT_DIR_NULL;
    if (current_idx.isValid()) {
	if (interaction_mode == 1) {
	    selected_dp = (struct directory *)(current_idx.parent().data(DirectoryInternalRole).value<void *>());
	    instance_dp = (struct directory *)(current_idx.data(DirectoryInternalRole).value<void *>());
	}
	if (interaction_mode == 2) {
	    selected_dp = (struct directory *)(current_idx.data(DirectoryInternalRole).value<void *>());
	}
    }

    if (selected_dp != RT_DIR_NULL) {
	CADTreeNode *expanded_node = IndexNode(idx);

	QQueue<CADTreeNode *> test_nodes;
	foreach (CADTreeNode *test_node, expanded_node->children) {
	    test_nodes.enqueue(test_node);
	}

	while (!test_nodes.isEmpty()) {
	    CADTreeNode *test_node = test_nodes.dequeue();
	    QModelIndex test_index = NodeIndex(test_node);
	    int hs = test_index.data(RelatedHighlightDisplayRole).toInt();
	    int is = test_index.data(InstanceHighlightDisplayRole).toInt();
	    if (selected_dp != test_node->node_dp && instance_dp != test_node->node_dp) {
		if (test_node->node_dp != RT_DIR_NULL && test_node->node_dp->d_flags & RT_DIR_COMB) {
		    if (!cadtreeview->isExpanded(test_index)) {
			int depth = 0;
			int search_results = 0;
			db_find_obj(&search_results, selected_dp->d_namep, test_node->node_dp, dbip, &depth, CADTREE_RECURSION_LIMIT, &combinternals);
			if (search_results && !hs) setData(test_index, QVariant(1), RelatedHighlightDisplayRole);
			if (!search_results && hs) setData(test_index, QVariant(0), RelatedHighlightDisplayRole);
		    } else {
			if (hs) setData(test_index, QVariant(0), RelatedHighlightDisplayRole);
		    }
		} else {
		    foreach (CADTreeNode *new_node, test_node->children) {
			test_nodes.enqueue(new_node);
		    }
		    if (hs) setData(test_index, QVariant(0), RelatedHighlightDisplayRole);
		}
	    } else {
		if (interaction_mode == 1) {
		    int node_state = 0;

		    if (instance_dp == test_node->node_dp && IndexNode(test_index.parent())->node_dp == selected_dp) node_state = 1;
		    if (selected_dp == test_node->node_dp) node_state = 1;
		    if (node_state && instance_dp == test_node->node_dp && test_index.row() != current_idx.row()) node_state = 0;
		    if (node_state && !is) setData(test_index, QVariant(1), InstanceHighlightDisplayRole);
		    if (!node_state && is) setData(test_index, QVariant(0), InstanceHighlightDisplayRole);
		    if (node_state && hs) setData(test_index, QVariant(0), RelatedHighlightDisplayRole);
		}
		if (interaction_mode == 2) {
		    if (is) setData(test_index, QVariant(0), InstanceHighlightDisplayRole);
		    if (!hs) setData(test_index, QVariant(1), RelatedHighlightDisplayRole);
		}
	    }
	}
    } else {
	CADTreeNode *expanded_node = IndexNode(idx);

	QQueue<CADTreeNode *> test_nodes;
	foreach (CADTreeNode *test_node, expanded_node->children) {
	    test_nodes.enqueue(test_node);
	}

	while (!test_nodes.isEmpty()) {
	    CADTreeNode *test_node = test_nodes.dequeue();
	    QModelIndex test_index = NodeIndex(test_node);
	    int hs = test_index.data(RelatedHighlightDisplayRole).toInt();
	    if (hs) setData(test_index, QVariant(0), RelatedHighlightDisplayRole);
	}
    }
#endif
}


// When an item is closed but the selection hasn't changed, the closed item is highlighted if any of its
// children were highlighted
void
QgSelectionProxyModel::close_tree_node_relationships(const QModelIndex &UNUSED(idx))
{
#if 0
    struct directory *selected_dp = RT_DIR_NULL;
    if (interaction_mode && current_idx.isValid()) {
	if (interaction_mode == 1)
	    selected_dp = (struct directory *)(current_idx.parent().data(DirectoryInternalRole).value<void *>());
	if (interaction_mode == 2)
	    selected_dp = (struct directory *)(current_idx.data(DirectoryInternalRole).value<void *>());
    }


    if (selected_dp != RT_DIR_NULL) {
	CADTreeNode *closed_node = IndexNode(idx);

	QQueue<CADTreeNode *> test_nodes;
	foreach (CADTreeNode *test_node, closed_node->children) {
	    test_nodes.enqueue(test_node);
	}

	while (!test_nodes.isEmpty()) {
	    CADTreeNode *test_node = test_nodes.dequeue();
	    QModelIndex test_index = NodeIndex(test_node);
	    int hs = test_index.data(RelatedHighlightDisplayRole).toInt();
	    if (hs || selected_dp == test_node->node_dp) {
		setData(idx, QVariant(1), RelatedHighlightDisplayRole);
		return;
	    } else {
		foreach (CADTreeNode *new_node, test_node->children) {
		    test_nodes.enqueue(new_node);
		}
	    }
	}
    }
#endif
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

