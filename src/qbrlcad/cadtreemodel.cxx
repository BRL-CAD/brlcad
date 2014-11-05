/*                C A D T R E E M O D E L . C X X
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file cadtreemodel.cxx
 *
 * Implementation of the data supply for the BRL-CAD Qt tree view.
 * It is this file which does the core work of translating from
 * .g information and structures into Qt tree elements.
 *
 */

#include "cadtreemodel.h"
#include "cadtreenode.h"
#include "cadapp.h"
#include "bu/sort.h"

CADTreeModel::CADTreeModel(QObject *parentobj)
    : QAbstractItemModel(parentobj)
{
}

CADTreeModel::~CADTreeModel()
{
    delete m_root;
}

QVariant
CADTreeModel::headerData(int section, Qt::Orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    if (section == 0) return QString("Object Names");
    return QVariant();
}

void CADTreeModel::setRootNode(CADTreeNode *root)
{
    m_root = root;
    beginResetModel();
    endResetModel();
}

QModelIndex CADTreeModel::index(int row, int column, const QModelIndex &parent_idx) const
{
    if (hasIndex(row, column, parent_idx)) {
	CADTreeNode *cnode = IndexNode(parent_idx)->children.at(row);
	return createIndex(row, column, cnode);
    }
    return QModelIndex();
}

QModelIndex CADTreeModel::parent(const QModelIndex &child) const
{
    CADTreeNode *pnode = IndexNode(child)->parent;
    if (pnode == m_root) return QModelIndex();
    return createIndex(NodeRow(pnode), 0, pnode);
}

int CADTreeModel::rowCount(const QModelIndex &parent_idx) const
{
    return IndexNode(parent_idx)->children.count();
}

int CADTreeModel::columnCount(const QModelIndex &parent_idx) const
{
    Q_UNUSED(parent_idx);
    return 1;
}

QVariant CADTreeModel::data(const QModelIndex &idx, int role) const
{
    if (idx.isValid() && role == Qt:: DisplayRole) {
	return IndexNode(idx)->data();
    }
    return QVariant();
}

QModelIndex CADTreeModel::NodeIndex(CADTreeNode *node) const
{
    if (node == m_root) return QModelIndex();
    return createIndex(NodeRow(node), 0, node);
}

CADTreeNode * CADTreeModel::IndexNode(const QModelIndex &idx) const
{
    if (idx.isValid()) {
	return static_cast<CADTreeNode *>(idx.internalPointer());
    }
    return m_root;
}

int CADTreeModel::NodeRow(CADTreeNode *node) const
{
    return node->parent->children.indexOf(node);
}

bool CADTreeModel::canFetchMore(const QModelIndex &idx) const
{
    CADTreeNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return false;
    if (curr_node->node_dp->d_flags & RT_DIR_COMB) {
	return true;
    }
    return false;
}

void
qtcad_count_children(union tree *tp, int *cnt)
{
    if (!tp) return;
    RT_CK_TREE(tp);
    switch (tp->tr_op) {
	case OP_DB_LEAF:
	    (*cnt)++;
	    return;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    /* This node is known to be a binary op */
	    qtcad_count_children(tp->tr_b.tb_left, cnt);
	    qtcad_count_children(tp->tr_b.tb_right, cnt);
	    return;
	default:
	    bu_log("qtcad_cnt_children: bad op %d\n", tp->tr_op);
	    bu_bomb("qtcad_cnt_children\n");
    }
    return;
}

void
qtcad_add_children(union tree *tp, /*int op,*/ CADTreeNode *curr_node)
{
    if (!tp) return;
    RT_CK_TREE(tp);
    switch (tp->tr_op) {
	case OP_DB_LEAF:
	    new CADTreeNode(QString(tp->tr_l.tl_name), curr_node);
	    //rt_tree_array->tl_op = op;
	    //rt_tree_array->tl_tree = tp;
	    return;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    /* This node is known to be a binary op */
	    qtcad_add_children(tp->tr_b.tb_left, /*op,*/ curr_node);
	    qtcad_add_children(tp->tr_b.tb_right, /*tp->tr_op,*/ curr_node);
	    return;
	default:
	    bu_log("qtcad_add_children: bad op %d\n", tp->tr_op);
	    bu_bomb("qtcad_add_children\n");
    }

    return;
}

void CADTreeModel::fetchMore(const QModelIndex &idx)
{
    CADTreeNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return;

    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int cnt = 0;
    if (rt_db_get_internal(&intern, curr_node->node_dp, current_dbip, (fastf_t *)NULL, &rt_uniresource) < 0) return;

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    qtcad_count_children(comb->tree, &cnt);
    /* TODO - need to actually check whether the child list matches the current search results,
     * and clear/rebuild things if there has been a change - the below check works only for a
     * static tree */
    if (cnt && !idx.child(cnt-1, 0).isValid()) {
	beginInsertRows(idx, 0, cnt);
	qtcad_add_children(comb->tree, curr_node);
	endInsertRows();
    }
    rt_db_free_internal(&intern);
}

bool CADTreeModel::hasChildren(const QModelIndex &idx) const
{
    CADTreeNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return true;
    if (curr_node->node_dp == RT_DIR_NULL && current_dbip != DBI_NULL)
	curr_node->node_dp = db_lookup(current_dbip, curr_node->name.toLocal8Bit(), LOOKUP_QUIET);
    if (curr_node->node_dp != RT_DIR_NULL && curr_node->node_dp->d_flags & RT_DIR_COMB) {
	return true;
    }
    return false;
}

void CADTreeModel::refresh()
{
    populate(((CADApp *)qApp)->dbip());
}

HIDDEN int
dp_cmp(const void *d1, const void *d2, void *UNUSED(arg))
{
    struct directory *dp1 = *(struct directory **)d1;
    struct directory *dp2 = *(struct directory **)d2;
    return bu_strcmp(dp1->d_namep, dp2->d_namep);
}

int CADTreeModel::populate(struct db_i *new_dbip)
{
    current_dbip = new_dbip;
    m_root = new CADTreeNode();

    if (current_dbip != DBI_NULL) {
	beginResetModel();
	struct directory **db_objects = NULL;
	int path_cnt = db_ls(current_dbip, DB_LS_TOPS, &db_objects);
	if (path_cnt) {
	    bu_sort(db_objects, path_cnt, sizeof(struct directory *), dp_cmp, NULL);
	    for (int i = 0; i < path_cnt; i++) {
		struct directory *curr_dp = db_objects[i];
		new CADTreeNode(QString(curr_dp->d_namep), m_root);
	    }
	}
	endResetModel();
    }
    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

