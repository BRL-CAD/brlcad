/*                    Q T C A D T R E E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file QtCADTree.cpp
 *
 * BRL-CAD Qt tree widgets.
 *
 */

#include "common.h"
#include <iostream>
#include <QPainter>
#include <qmath.h>
#include <QAction>
#include <QMenu>
#include <QQueue>

#include "bu/sort.h"
#include "raytrace.h"
#include "qtcad/QtCADTree.h"

enum CADDataRoles {
    BoolInternalRole = Qt::UserRole + 1000,
    BoolDisplayRole = Qt::UserRole + 1001,
    DirectoryInternalRole = Qt::UserRole + 1002,
    TypeIconDisplayRole = Qt::UserRole + 1003,
    RelatedHighlightDisplayRole = Qt::UserRole + 1004,
    InstanceHighlightDisplayRole = Qt::UserRole + 1005
};

CADTreeNode::CADTreeNode(struct directory *in_dp, CADTreeNode *aParent)
: node_dp(in_dp), parent(aParent)
{
    if(parent) {
	parent->children.append(this);
    }
    is_highlighted = 0;
    instance_highlight = 0;
}

CADTreeNode::CADTreeNode(QString dp_name, CADTreeNode *aParent)
: name(dp_name), parent(aParent)
{
    node_dp = RT_DIR_NULL;
    if(parent) {
	parent->children.append(this);
    }
}

CADTreeNode::~CADTreeNode()
{
    qDeleteAll(children);
}


CADTreeModel::CADTreeModel(QObject *parentobj)
    : QAbstractItemModel(parentobj)
{
    Q_INIT_RESOURCE(qtcad_resources);
    current_idx = QModelIndex();
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


static int
get_arb_type(struct directory *dp, struct db_i *dbip)
{
    int type;
    const struct bn_tol arb_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1.0e-6, 1.0 - 1.0e-6 };
    struct rt_db_internal intern;
    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) return 0;
    type = rt_arb_std_type(&intern, &arb_tol);
    rt_db_free_internal(&intern);
    return type;
}


static void
db_find_subregion(int *ret, union tree *tp, struct db_i *dbip, int *depth, int max_depth,
	void (*traverse_func) (int *ret, struct directory *, struct db_i *, int *, int))
{
    struct directory *dp;
    if (!tp) return;
    if (*ret) return;
    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);
    (*depth)++;
    if (*depth > max_depth) {
	(*depth)--;
	return;
    }
    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    db_find_subregion(ret, tp->tr_b.tb_right, dbip, depth, max_depth, traverse_func);
	    break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    db_find_subregion(ret, tp->tr_b.tb_left, dbip, depth, max_depth, traverse_func);
	    (*depth)--;
	    break;
	case OP_DB_LEAF:
	    if ((dp=db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		(*depth)--;
		return;
	    } else {
		if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		    traverse_func(ret, dp, dbip, depth, max_depth);
		}
		(*depth)--;
		break;
	    }

	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }
    return;
}

static void
db_find_region(int *ret, struct directory *search, struct db_i *dbip, int *depth, int max_depth)
{

    /* If we have a match, we need look no further */
    if (search->d_flags & RT_DIR_REGION) {
	(*ret)++;
	return;
    }

   /* If we have a comb, open it up.  Otherwise, we're done */
    if (search->d_flags & RT_DIR_COMB) {
	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal(&in, search, dbip, NULL, &rt_uniresource) < 0) return;

	comb = (struct rt_comb_internal *)in.idb_ptr;
	db_find_subregion(ret, comb->tree, dbip, depth, max_depth, db_find_region);
	rt_db_free_internal(&in);
    }
    return;
}

static int
get_comb_type(struct directory *dp, struct db_i *dbip)
{
    struct bu_attribute_value_set avs;
    int region_flag = 0;
    int air_flag = 0;
    int region_id_flag = 0;
    int assembly_flag = 0;
    if (dp->d_flags & RT_DIR_REGION) {
	region_flag = 1;
    }

    bu_avs_init_empty(&avs);
    (void)db5_get_attributes(dbip, &avs, dp);
    const char *airval = bu_avs_get(&avs, "aircode");
    if (airval && !BU_STR_EQUAL(airval, "0")) {
	air_flag = 1;
    }
    const char *region_id = bu_avs_get(&avs, "region_id");
    if (region_id && !BU_STR_EQUAL(region_id, "0")) {
	region_id_flag = 1;
    }


    if (!region_flag && !air_flag) {
	int search_results = 0;
	int depth = 0;
	db_find_region(&search_results, dp, dbip, &depth, CADTREE_RECURSION_LIMIT);
	if (search_results) assembly_flag = 1;
    }

    if (region_flag && !air_flag) return 2;
    if (!region_id_flag && air_flag) return 3;
    if (region_id_flag && air_flag) return 4;
    if (assembly_flag) return 5;

    return 0;
}

static QImage
get_type_icon(struct directory *dp, struct db_i *dbip)
{
    int type = 0;
    QImage raw_type_icon;
    if (dbip != DBI_NULL && dp != RT_DIR_NULL) {
	switch(dp->d_minor_type) {
	    case DB5_MINORTYPE_BRLCAD_TOR:
		raw_type_icon.load(":/images/primitives/tor.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_TGC:
		raw_type_icon.load(":/images/primitives/tgc.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_ELL:
		raw_type_icon.load(":/images/primitives/ell.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_ARB8:
		type = get_arb_type(dp, dbip);
		switch (type) {
		    case 4:
			raw_type_icon.load(":/images/primitives/arb4.png");
			break;
		    case 5:
			raw_type_icon.load(":/images/primitives/arb5.png");
			break;
		    case 6:
			raw_type_icon.load(":/images/primitives/arb6.png");
			break;
		    case 7:
			raw_type_icon.load(":/images/primitives/arb7.png");
			break;
		    case 8:
			raw_type_icon.load(":/images/primitives/arb8.png");
			break;
		    default:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		}
		break;
	    case DB5_MINORTYPE_BRLCAD_ARS:
		raw_type_icon.load(":/images/primitives/ars.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_HALF:
		raw_type_icon.load(":/images/primitives/half.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_REC:
		raw_type_icon.load(":/images/primitives/tgc.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_POLY:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_BSPLINE:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_SPH:
		raw_type_icon.load(":/images/primitives/sph.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_NMG:
		raw_type_icon.load(":/images/primitives/nmg.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_EBM:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_VOL:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_ARBN:
		raw_type_icon.load(":/images/primitives/arbn.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_PIPE:
		raw_type_icon.load(":/images/primitives/pipe.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_PARTICLE:
		raw_type_icon.load(":/images/primitives/part.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_RPC:
		raw_type_icon.load(":/images/primitives/rpc.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_RHC:
		raw_type_icon.load(":/images/primitives/rhc.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_EPA:
		raw_type_icon.load(":/images/primitives/epa.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_EHY:
		raw_type_icon.load(":/images/primitives/ehy.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_ETO:
		raw_type_icon.load(":/images/primitives/eto.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_GRIP:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_JOINT:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_HF:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_DSP:
		raw_type_icon.load(":/images/primitives/dsp.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_SKETCH:
		raw_type_icon.load(":/images/primitives/sketch.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_EXTRUDE:
		raw_type_icon.load(":/images/primitives/extrude.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_SUBMODEL:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_CLINE:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_BOT:
		raw_type_icon.load(":/images/primitives/bot.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_COMBINATION:
		type = get_comb_type(dp, dbip);
		switch (type) {
		    case 2:
			raw_type_icon.load(":/images/primitives/region.png");
			break;
		    case 3:
			raw_type_icon.load(":/images/primitives/air.png");
			break;
		    case 4:
			raw_type_icon.load(":/images/primitives/airregion.png");
			break;
		    case 5:
			raw_type_icon.load(":/images/primitives/assembly.png");
			break;
		    default:
			raw_type_icon.load(":/images/primitives/comb.png");
			break;
		}
		break;
	    case DB5_MINORTYPE_BRLCAD_SUPERELL:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_METABALL:
		raw_type_icon.load(":/images/primitives/metaball.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_BREP:
		raw_type_icon.load(":/images/primitives/brep.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_HYP:
		raw_type_icon.load(":/images/primitives/hyp.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_CONSTRAINT:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_REVOLVE:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_ANNOT:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    case DB5_MINORTYPE_BRLCAD_HRT:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	    default:
		raw_type_icon.load(":/images/primitives/other.png");
		break;
	}
    }

    return raw_type_icon;
}


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

static void
db_find_subtree(int *ret, const char *name, union tree *tp, struct db_i *dbip, int *depth, int max_depth, QHash<struct directory *, struct rt_db_internal *> *combinternals,
	void (*traverse_func) (int *ret, const char *, struct directory *, struct db_i *, int *, int, QHash<struct directory *, struct rt_db_internal *> *))
{
    struct directory *dp;
    if (!tp) return;
    if (*ret) return;
    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);
    (*depth)++;
    if (*depth > max_depth) {
	(*depth)--;
	return;
    }
    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    db_find_subtree(ret, name, tp->tr_b.tb_right, dbip, depth, max_depth, combinternals, traverse_func);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    db_find_subtree(ret, name, tp->tr_b.tb_left, dbip, depth, max_depth, combinternals, traverse_func);
	    (*depth)--;
	    break;
	case OP_DB_LEAF:
	    if ((dp=db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		(*depth)--;
		return;
	    } else {
		if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		    traverse_func(ret, name, dp, dbip, depth, max_depth, combinternals);
		}
		(*depth)--;
		break;
	    }

	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }
    return;
}

static void
db_find_obj(int *ret, const char *name, struct directory *search, struct db_i *dbip, int *depth, int max_depth, QHash<struct directory *, struct rt_db_internal *> *combinternals)
{
    /* If we have a match, we need look no further */
    if (BU_STR_EQUAL(search->d_namep, name)) {
	(*ret)++;
	return;
    }

   /* If we have a comb, open it up.  Otherwise, we're done */
    if (search->d_flags & RT_DIR_COMB) {
	struct rt_db_internal *in = combinternals->find(search).value();
	if (in) {
	    struct rt_comb_internal *comb = (struct rt_comb_internal *)in->idb_ptr;
	    db_find_subtree(ret, name, comb->tree, dbip, depth, max_depth, combinternals, db_find_obj);
	}
    }
    return;
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
    if (rowCount(idx)) {
	return false;
    }
    if (curr_node->node_dp->d_flags & RT_DIR_COMB) {
	return true;
    }
    return false;
}

void
cad_count_children(union tree *tp, int *cnt)
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
	    cad_count_children(tp->tr_b.tb_left, cnt);
	    cad_count_children(tp->tr_b.tb_right, cnt);
	    return;
	default:
	    bu_log("qtcad_cnt_children: bad op %d\n", tp->tr_op);
	    bu_bomb("qtcad_cnt_children\n");
    }
    return;
}

void
CADTreeModel::cad_add_child(const char *name, CADTreeNode *curr_node, int op)
{
    CADTreeNode *new_node = new CADTreeNode(QString(name), curr_node);
    QModelIndex idx = NodeIndex(new_node);
    struct directory *dp = db_lookup(current_dbip, new_node->name.toLocal8Bit(), LOOKUP_QUIET);
    setData(idx, QVariant(op), BoolInternalRole);
    setData(idx, QVariant::fromValue((void *)dp), DirectoryInternalRole);
    setData(idx, QVariant(get_type_icon(dp, current_dbip)), TypeIconDisplayRole);
    setData(idx, QVariant(0), RelatedHighlightDisplayRole);
    setData(idx, QVariant(0), InstanceHighlightDisplayRole);
    all_nodes.insert(new_node);
}

void
CADTreeModel::cad_add_children(union tree *tp, int op, CADTreeNode *curr_node)
{
    if (!tp) return;
    RT_CK_TREE(tp);
    switch (tp->tr_op) {
	case OP_DB_LEAF:
	    cad_add_child(tp->tr_l.tl_name, curr_node, op);
	    return;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    /* This node is known to be a binary op */
	    cad_add_children(tp->tr_b.tb_left, op, curr_node);
	    cad_add_children(tp->tr_b.tb_right, tp->tr_op, curr_node);
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
    cad_count_children(comb->tree, &cnt);
    /* TODO - need to actually check whether the child list matches the current search results,
     * and clear/rebuild things if there has been a change - the below check works only for a
     * static tree */
    if (cnt) { // TODO - .child is obsolete, need to revisit this test:  && !idx.child(cnt-1, 0).isValid()) {
	beginInsertRows(idx, 0, cnt);
	cad_add_children(comb->tree, OP_UNION, curr_node);
	endInsertRows();
    }
    rt_db_free_internal(&intern);
}

bool CADTreeModel::hasChildren(const QModelIndex &idx) const
{
    CADTreeNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return true;
    if (curr_node->node_dp == RT_DIR_NULL && current_dbip != DBI_NULL)
	return false;
    if (curr_node->node_dp != RT_DIR_NULL && curr_node->node_dp->d_flags & RT_DIR_COMB) {
	int cnt = 0;
	struct rt_db_internal intern;
	if (rt_db_get_internal(&intern, curr_node->node_dp, current_dbip, (fastf_t *)NULL, &rt_uniresource) < 0) return false;
	struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
	cad_count_children(comb->tree, &cnt);
	rt_db_free_internal(&intern);
	if (cnt > 0) return true;
    }
    return false;
}

void CADTreeModel::refresh()
{
    all_nodes.clear();
    populate(dbip);
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
    struct directory *cdp;
    current_dbip = new_dbip;
    m_root = new CADTreeNode();

    if (current_dbip != DBI_NULL) {
	beginResetModel();
	struct directory **db_objects = NULL;
	int path_cnt = db_ls(current_dbip, DB_LS_TOPS, NULL, &db_objects);
	if (path_cnt) {
	    bu_sort(db_objects, path_cnt, sizeof(struct directory *), dp_cmp, NULL);
	    for (int i = 0; i < path_cnt; i++) {
		struct directory *curr_dp = db_objects[i];
		cad_add_child(curr_dp->d_namep, m_root, OP_UNION);
	    }
	}
	endResetModel();

	/* build a map with all the comb internals */
	combinternals.clear();
	for (int i = 0; i < RT_DBNHASH; i++) {
	    for (cdp = new_dbip->dbi_Head[i]; cdp != RT_DIR_NULL; cdp = cdp->d_forw) {
		if (cdp->d_flags & RT_DIR_COMB) {
		    struct rt_db_internal *intern;
		    BU_GET(intern, struct rt_db_internal);
		    if (rt_db_get_internal(intern, cdp, new_dbip, (fastf_t *)NULL, &rt_uniresource) >= 0) {
			combinternals.insert(cdp, intern);
		    }
		}
	    }
	}
    }

    return 0;
}

/*       C A D T R E E V I E W    */

void GObjectDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    struct directory *cdp = RT_DIR_NULL;
    if (!cadtreeview)
	return;
    QModelIndex selected = cadtreeview->selected();
    if (selected.isValid())
	cdp = (struct directory *)(selected.data(DirectoryInternalRole).value<void *>());
    struct directory *pdp = (struct directory *)(index.data(DirectoryInternalRole).value<void *>());
    if (option.state & QStyle::State_Selected) {
	painter->fillRect(option.rect, option.palette.highlight());
	goto text_string;
    }
    if ((!cadtreeview->isExpanded(index) && index.data(RelatedHighlightDisplayRole).toInt())
	    || (cdp == pdp && index.data(RelatedHighlightDisplayRole).toInt())) {
	painter->fillRect(option.rect, QBrush(QColor(220, 200, 30)));
	goto text_string;
    }
    if (index.data(InstanceHighlightDisplayRole).toInt()) {
	painter->fillRect(option.rect, QBrush(QColor(10, 10, 50)));
	goto text_string;
    }

text_string:
    QString text = index.data().toString();
    int bool_op = index.data(BoolInternalRole).toInt();
    switch (bool_op) {
	case OP_INTERSECT:
	    text.prepend("  + ");
	    break;
	case OP_SUBTRACT:
	    text.prepend("  - ");
	    break;
    }
    text.prepend(" ");

    // rect with width proportional to value
    //QRect rect(option.rect.adjusted(4,4,-4,-4));

    // draw the value bar
    //painter->fillRect(rect, QBrush(QColor("steelblue")));
    //
    // draw text label

    QImage type_icon = index.data(TypeIconDisplayRole).value<QImage>().scaledToHeight(option.rect.height()-2);
    QRect image_rect = type_icon.rect();
    image_rect.moveTo(option.rect.topLeft());
    QRect text_rect(type_icon.rect().topRight(), option.rect.bottomRight());
    text_rect.moveTo(image_rect.topRight());
    image_rect.translate(0, 1);
    painter->drawImage(image_rect, type_icon);

    painter->drawText(text_rect, text, QTextOption(Qt::AlignLeft));
}

QSize GObjectDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize name_size = option.fontMetrics.size(Qt::TextSingleLine, index.data().toString());
    QSize bool_size;
    int bool_op = index.data(BoolInternalRole).toInt();
    switch (bool_op) {
	case OP_UNION:
	    bool_size = option.fontMetrics.size(Qt::TextSingleLine, " ");
	    break;
	case OP_INTERSECT:
	    bool_size = option.fontMetrics.size(Qt::TextSingleLine, "   + ");
	    break;
	case OP_SUBTRACT:
	    bool_size = option.fontMetrics.size(Qt::TextSingleLine, "   - ");
	    break;
    }
    int item_width = name_size.rwidth() + bool_size.rwidth() + 18;
    int item_height = name_size.rheight();
    return QSize(item_width, item_height);
}


CADTreeView::CADTreeView(QWidget *pparent) : QTreeView(pparent)
{
    this->setContextMenuPolicy(Qt::CustomContextMenu);
}

void
CADTreeView::drawBranches(QPainter* painter, const QRect& rrect, const QModelIndex& index) const
{
    struct directory *cdp = RT_DIR_NULL;
    QModelIndex selected_idx = ((CADTreeView *)this)->selected();
    if (selected_idx.isValid())
	cdp = (struct directory *)(selected_idx.data(DirectoryInternalRole).value<void *>());
    struct directory *pdp = (struct directory *)(index.data(DirectoryInternalRole).value<void *>());
    if (!(index == selected_idx)) {
	if (!(CADTreeView *)this->isExpanded(index) && index.data(RelatedHighlightDisplayRole).toInt()) {
	    painter->fillRect(rrect, QBrush(QColor(220, 200, 30)));
	}
	if (cdp == pdp && index.data(RelatedHighlightDisplayRole).toInt()) {
	    painter->fillRect(rrect, QBrush(QColor(220, 200, 30)));
	}
	if (index.data(InstanceHighlightDisplayRole).toInt()) {
	    painter->fillRect(rrect, QBrush(QColor(10, 10, 50)));
	}
    }
    QTreeView::drawBranches(painter, rrect, index);
}

void CADTreeView::header_state()
{
    //header()->setStretchLastSection(true);
    //int header_length = header()->length();
    //header()->setSectionResizeMode(0, QHeaderView::Fixed);
    //resizeColumnToContents(0);
    //int column_width = columnWidth(0);

    // TODO - when the dock widget is resized, there is a missing
    // update somewhere - the scrollbar doesn't appear until the tree
    // is selected...

    header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    header()->setStretchLastSection(false);
    if (header()->count() == 1) {
	header()->setStretchLastSection(false);
    } else {
	header()->setStretchLastSection(true);
    }
}

void CADTreeView::resizeEvent(QResizeEvent *)
{
    header_state();
}

void CADTreeView::tree_column_size(const QModelIndex &)
{
    header_state();
}

void CADTreeView::context_menu(const QPoint &point)
{
    QModelIndex index = indexAt(point);
    QAction* act = new QAction(index.data().toString(), NULL);
    QMenu *menu = new QMenu("Object Actions", NULL);
    menu->addAction(act);
    menu->exec(mapToGlobal(point));
}

QModelIndex CADTreeView::selected()
{
    if (selectedIndexes().count() == 1) {
	return selectedIndexes().first();
    } else {
	return QModelIndex();
    }
}

void CADTreeView::expand_path(QString path)
{
    int i = 0;
    QStringList path_items = path.split("/", QString::SkipEmptyParts);
    CADTreeModel *view_model = (CADTreeModel *)model();
    QList<CADTreeNode*> *tree_children = &(view_model->m_root->children);
    while (i < path_items.size()) {
	for (int j = 0; j < tree_children->size(); ++j) {
	    CADTreeNode *test_node = tree_children->at(j);
	    if (test_node->name == path_items.at(i)) {
		QModelIndex path_component = view_model->NodeIndex(test_node);
		if (i == path_items.size() - 1) {
		    selectionModel()->clearSelection();
		    selectionModel()->select(path_component, QItemSelectionModel::Select | QItemSelectionModel::Rows);
		    emit (clicked(path_component));
		} else {
		    expand(path_component);
		    tree_children = &(test_node->children);
		}
		break;
	    }
	}
	i++;
    }
}

void CADTreeView::expand_link(const QUrl &link)
{
    expand_path(link.path());
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

