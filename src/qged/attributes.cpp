/*               C A D A T T R I B U T E S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file cadattributes.cpp
 *
 * Brief description
 *
 */

#include <QPainter>
#include <QString>

#include "attributes.h"
#include "app.h"
#include "bu/sort.h"
#include "bu/avs.h"
#include "bu/malloc.h"

// *********** Node **************

QKeyValNode::QKeyValNode(QKeyValNode *aParent)
: parent(aParent)
{
    if(parent) {
	parent->children.append(this);
    }
}

QKeyValNode::~QKeyValNode()
{
    qDeleteAll(children);
}

// *********** Model **************

QKeyValModel::QKeyValModel(QObject *aParent)
: QAbstractItemModel(aParent)
{
}

QKeyValModel::~QKeyValModel()
{
}

QVariant
QKeyValModel::headerData(int section, Qt::Orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    if (section == 0) return QString("Property");
    if (section == 1) return QString("Value");
    return QVariant();
}

QVariant
QKeyValModel::data(const QModelIndex & idx, int role) const
{
    if (!idx.isValid()) return QVariant();
    QKeyValNode *curr_node = IndexNode(idx);
    if (role == Qt::DisplayRole && idx.column() == 0) return QVariant(curr_node->name);
    if (role == Qt::DisplayRole && idx.column() == 1) return QVariant(curr_node->value);
    //if (role == DirectoryInternalRole) return QVariant::fromValue((void *)(curr_node->node_dp));
    return QVariant();
}

bool
QKeyValModel::setData(const QModelIndex & idx, const QVariant & value, int role)
{
    if (!idx.isValid()) return false;
    QVector<int> roles;
    bool ret = false;
    QKeyValNode *curr_node = IndexNode(idx);
    if (role == Qt::DisplayRole) {
	curr_node->name = value.toString();
	roles.append(Qt::DisplayRole);
	ret = true;
    }
    if (ret) emit dataChanged(idx, idx, roles);
    return ret;
}

void QKeyValModel::setRootNode(QKeyValNode *root)
{
    m_root = root;
    beginResetModel();
    endResetModel();
}

QModelIndex QKeyValModel::index(int row, int column, const QModelIndex &parent_idx) const
{
    if (hasIndex(row, column, parent_idx)) {
	QKeyValNode *cnode = IndexNode(parent_idx)->children.at(row);
	return createIndex(row, column, cnode);
    }
    return QModelIndex();
}

QModelIndex QKeyValModel::parent(const QModelIndex &child) const
{
    QKeyValNode *pnode = IndexNode(child)->parent;
    if (pnode == m_root) return QModelIndex();
    return createIndex(NodeRow(pnode), 0, pnode);
}

int QKeyValModel::rowCount(const QModelIndex &parent_idx) const
{
    return IndexNode(parent_idx)->children.count();
}

int QKeyValModel::columnCount(const QModelIndex &parent_idx) const
{
    Q_UNUSED(parent_idx);
    return 2;
}

QModelIndex QKeyValModel::NodeIndex(QKeyValNode *node) const
{
    if (node == m_root) return QModelIndex();
    return createIndex(NodeRow(node), 0, node);
}

QKeyValNode * QKeyValModel::IndexNode(const QModelIndex &idx) const
{
    if (idx.isValid()) {
	return static_cast<QKeyValNode *>(idx.internalPointer());
    }
    return m_root;
}

int QKeyValModel::NodeRow(QKeyValNode *node) const
{
    return node->parent->children.indexOf(node);
}

QKeyValNode *
QKeyValModel::add_pair(const char *name, const char *value, QKeyValNode *curr_node, int type)
{
    QKeyValNode *new_node = new QKeyValNode(curr_node);
    new_node->name = name;
    new_node->value = value;
    new_node->attr_type = type;
    return new_node;
}


// *********** View **************
void QKeyValDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QString text = index.data().toString();
    painter->drawText(option.rect, text, QTextOption(Qt::AlignLeft));
}

QSize QKeyValDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize name_size = option.fontMetrics.size(Qt::TextSingleLine, index.data().toString());
    return name_size;
}


QKeyValView::QKeyValView(QWidget *pparent, int tree_decorate) : QTreeView(pparent)
{
    //this->setContextMenuPolicy(Qt::CustomContextMenu);
    if (!tree_decorate) {
	setRootIsDecorated(false);
    }
}

// *********** ATTR **************

CADAttributesModel::CADAttributesModel(QObject *parentobj, struct db_i *dbip, struct directory *dp, int show_standard, int show_user)
    : QKeyValModel(parentobj)
{
    int i = 0;
    current_dbip = dbip;
    current_dp = dp;
    if (show_standard) {
	std_visible = 1;
    } else {
	std_visible = 0;
    }
    if (show_user) {
	user_visible = 1;
    } else {
	user_visible = 0;
    }
    m_root = new QKeyValNode();
    BU_GET(avs, struct bu_attribute_value_set);
    bu_avs_init_empty(avs);
    if (std_visible) {
	while (i != ATTR_NULL) {
	    add_pair(db5_standard_attribute(i), "", m_root, i);
	    i++;
	}
    }
    if (dbip != DBI_NULL && dp != RT_DIR_NULL) {
	update(dbip, dp);
    }
}

CADAttributesModel::~CADAttributesModel()
{
    bu_avs_free(avs);
    BU_PUT(avs, struct bu_attribute_value_set);
}

HIDDEN int
attr_children(const char *attr)
{
    if (BU_STR_EQUAL(attr, "color")) return 3;
    return 0;
}


bool CADAttributesModel::canFetchMore(const QModelIndex &idx) const
{
    QKeyValNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return false;
    if (rowCount(idx)) {
	return false;
    }
    int cnt = attr_children(curr_node->name.toLocal8Bit());
    if (cnt > 0) return true;
    return false;
}

void
CADAttributesModel::add_Children(const char *name, QKeyValNode *curr_node)
{
    if (BU_STR_EQUAL(name, "color")) {
	QString val(bu_avs_get(avs, name));
	QStringList vals = val.split(QRegExp("/"));
	(void)add_pair("r", vals.at(0).toLocal8Bit(), curr_node, db5_standardize_attribute(name));
	(void)add_pair("g", vals.at(1).toLocal8Bit(), curr_node, db5_standardize_attribute(name));
	(void)add_pair("b", vals.at(2).toLocal8Bit(), curr_node, db5_standardize_attribute(name));
	return;
    }
    (void)add_pair(name, bu_avs_get(avs, name), curr_node, db5_standardize_attribute(name));
}


void CADAttributesModel::fetchMore(const QModelIndex &idx)
{
    QKeyValNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return;
    int cnt = attr_children(curr_node->name.toLocal8Bit());
    if (cnt) { // && !idx.child(cnt-1, 0).isValid()) {
	beginInsertRows(idx, 0, cnt);
	add_Children(curr_node->name.toLocal8Bit(),curr_node);
	endInsertRows();
    }
}

bool CADAttributesModel::hasChildren(const QModelIndex &idx) const
{
    QKeyValNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return true;
    if (curr_node->value == QString("")) return false;
    int cnt = attr_children(curr_node->name.toLocal8Bit());
    if (cnt > 0) return true;
    return false;
}

int CADAttributesModel::update(struct db_i *new_dbip, struct directory *new_dp)
{
    current_dp = new_dp;
    current_dbip = new_dbip;
    if (current_dbip != DBI_NULL && current_dp != RT_DIR_NULL) {
	QMap<QString, QKeyValNode*> standard_nodes;
	int i = 0;
	m_root = new QKeyValNode();
	beginResetModel();
	struct bu_attribute_value_pair *avpp;
	for (BU_AVS_FOR(avpp, avs)) {
	    bu_avs_remove(avs, avpp->name);
	}
	(void)db5_get_attributes(current_dbip, avs, current_dp);

	if (std_visible) {
	    while (i != ATTR_NULL) {
		standard_nodes.insert(db5_standard_attribute(i), add_pair(db5_standard_attribute(i), "", m_root, i));
		i++;
	    }
	    for (BU_AVS_FOR(avpp, avs)) {
		if (db5_is_standard_attribute(avpp->name)) {
		    if (standard_nodes.find(avpp->name) != standard_nodes.end()) {
			QString new_value(avpp->value);
			QKeyValNode *snode = standard_nodes.find(avpp->name).value();
			snode->value = new_value;
		    } else {
			add_pair(avpp->name, avpp->value, m_root, db5_standardize_attribute(avpp->name));
		    }
		}
	    }
	}
	if (user_visible) {
	    for (BU_AVS_FOR(avpp, avs)) {
		if (!db5_is_standard_attribute(avpp->name)) {
		    add_pair(avpp->name, avpp->value, m_root, ATTR_NULL);
		}
	    }
	}
	endResetModel();
    } else {
	m_root = new QKeyValNode();
	beginResetModel();
	endResetModel();
    }
    return 0;
}

void
CADAttributesModel::refresh(const QModelIndex &idx)
{
    current_dbip = ((CADApp *)qApp)->dbip();
    current_dp = (struct directory *)(idx.data(DirectoryInternalRole).value<void *>());
    update(current_dbip, current_dp);
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

