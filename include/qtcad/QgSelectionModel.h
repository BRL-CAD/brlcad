/*                   Q G S E L E C T M O D E L . H
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
/** @file QgSelectModel.h
 *
 * Qt proxy model intended to sit between a QgTreeView and a QgModel
 * in order to store the necessary information to manage selection
 * state.
 *
 * In the original QtCADTree code the selection information ended up being
 * stored with the model.  Instead of replicating that approach, we instead
 * subclass QIdentityProxyModel to create a purpose-built model for managing
 * the selection states. The .g database does not define a notion of "currently
 * selected" so storing that information in QgModel is a poor fit for data.  We
 * don't want QSortFilterProxyModel for a hierarchical tree view, since we
 * don't actually want sorting behavior in tree views (list views may be
 * another matter, but the initial focus is on the tree view.) Rather, we want
 * a "related highlighted" set in response to changing selection and
 * interaction mode settings. A "QgSelectionModel" class will hold and manage
 * that data, as a proxy between the "proper" .g model and the view - that way
 * we don't have to pollute either the .g model or the view implementation
 * managing the extra state.
 *
 * See:
 * https://doc.qt.io/qt-5/qidentityproxymodel.html
 * https://forum.qt.io/topic/120258/adding-data-throught-qidentitymodel-to-qsqltablemodel-fails
 */

#ifndef QGSELECTIONMODEL_H
#define QGSELECTIONMODEL_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <QIdentityProxyModel>
#include <QModelIndex>

#include "qtcad/defines.h"
#include "qtcad/QgModel.h"


class QTCAD_EXPORT QgSelectionModel : public QIdentityProxyModel
{
    Q_OBJECT

    public:
	enum CADDataRoles {
	    BoolInternalRole = Qt::UserRole + 1000,
	    BoolDisplayRole = Qt::UserRole + 1001,
	    DirectoryInternalRole = Qt::UserRole + 1002,
	    TypeIconDisplayRole = Qt::UserRole + 1003,
	    RelatedHighlightDisplayRole = Qt::UserRole + 1004,
	    InstanceHighlightDisplayRole = Qt::UserRole + 1005
	};

	// Activity flags (used for relevance highlighting) need to be updated
	// whenever a selection changes.  We define one flag per gInstance,
	// and update flags accordingly based on current app settings.
	// Highlighting is then applied in the view based on the current value
	// of these flags.
	std::vector<int> active_flags;
};

// The following logic needs to be translated in some fashion into the above
// proxy model...

#if 0
QVariant
CADTreeModel::data(const QModelIndex & idx, int role) const
{
    if (!idx.isValid()) return QVariant();
    CADTreeNode *curr_node = IndexNode(idx);
    if (role == Qt::DisplayRole) return QVariant(curr_node->name);
    if (role == BoolInternalRole) return QVariant(curr_node->boolean);
    if (role == DirectoryInternalRole) return QVariant::fromValue((void *)(curr_node->node_dp));
    if (role == TypeIconDisplayRole) return curr_node->icon;
    if (role == RelatedHighlightDisplayRole) return curr_node->is_highlighted;
    if (role == InstanceHighlightDisplayRole) return curr_node->instance_highlight;
    return QVariant();
}

bool
CADTreeModel::setData(const QModelIndex & idx, const QVariant & value, int role)
{
    if (!idx.isValid()) return false;
    QVector<int> roles;
    bool ret = false;
    CADTreeNode *curr_node = IndexNode(idx);
    if (role == BoolInternalRole) {
        curr_node->boolean = value.toInt();
        roles.append(BoolInternalRole);
        ret = true;
    }
    if (role == DirectoryInternalRole) {
        curr_node->node_dp = (struct directory *)(value.value<void *>());
        roles.append(DirectoryInternalRole);
        ret = true;
    }
    if (role == TypeIconDisplayRole) {
        curr_node->icon = value;
        roles.append(TypeIconDisplayRole);
        ret = true;
    }
    if (role == RelatedHighlightDisplayRole) {
        curr_node->is_highlighted = value.toInt();
        roles.append(RelatedHighlightDisplayRole);
        roles.append(Qt::DisplayRole);
        ret = true;
    }
    if (role == InstanceHighlightDisplayRole) {
        curr_node->instance_highlight = value.toInt();
        roles.append(InstanceHighlightDisplayRole);
        roles.append(Qt::DisplayRole);
        ret = true;
    }
    if (ret) emit dataChanged(idx, idx, roles);
    return ret;
}

// These functions tell the related-object highlighting logic what the current status is.

// This function is needed when the selected object is changed - highlighted items may change
// anywhere in the tree view
void
CADTreeModel::update_selected_node_relationships(const QModelIndex & idx)
{
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

    // For the case of a selection change, emit a layout change signal so the drawBranches call updates
    // the portions of the row colors not handled by the itemDelegate painting.  For expand and close
    // operations on items this is already handled by Qt, but layout updating is not a normal part of the selection
    // process in most tree views so for the customized selection drawing we do we need to call it manually.
    emit layoutChanged();
}

// When an item is expanded but the selection hasn't changed, scope for highlighting changes is more mimimal
void
CADTreeModel::expand_tree_node_relationships(const QModelIndex & idx)
{
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
}


// When an item is closed but the selection hasn't changed, the closed item is highlighted if any of its
// children were highlighted
void
CADTreeModel::close_tree_node_relationships(const QModelIndex & idx)
{
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
}
#endif

#endif //QGSELECTIONMODEL_H

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

