/*                    Q G K E Y V A L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2023 United States Government as represented by
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
/** @file QgKeyVal.cpp
 *
 */

#include <QPainter>

#include "qtcad/QgKeyVal.h"

// *********** Node **************

QgKeyValNode::QgKeyValNode(QgKeyValNode *aParent)
: parent(aParent)
{
    if(parent) {
	parent->children.append(this);
    }
}

QgKeyValNode::~QgKeyValNode()
{
    qDeleteAll(children);
}

// *********** Model **************

QgKeyValModel::QgKeyValModel(QObject *aParent)
: QAbstractItemModel(aParent)
{
}

QgKeyValModel::~QgKeyValModel()
{
}

QVariant
QgKeyValModel::headerData(int section, Qt::Orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    if (section == 0) return QString("Property");
    if (section == 1) return QString("Value");
    return QVariant();
}

QVariant
QgKeyValModel::data(const QModelIndex & idx, int role) const
{
    if (!idx.isValid()) return QVariant();
    QgKeyValNode *curr_node = IndexNode(idx);
    if (role == Qt::DisplayRole && idx.column() == 0) return QVariant(curr_node->name);
    if (role == Qt::DisplayRole && idx.column() == 1) return QVariant(curr_node->value);
    return QVariant();
}

bool
QgKeyValModel::setData(const QModelIndex & idx, const QVariant & value, int role)
{
    if (!idx.isValid()) return false;
    QVector<int> roles;
    bool ret = false;
    QgKeyValNode *curr_node = IndexNode(idx);
    if (role == Qt::DisplayRole) {
	curr_node->name = value.toString();
	roles.append(Qt::DisplayRole);
	ret = true;
    }
    if (ret) emit dataChanged(idx, idx, roles);
    return ret;
}

void QgKeyValModel::setRootNode(QgKeyValNode *root)
{
    m_root = root;
    beginResetModel();
    endResetModel();
}

QModelIndex QgKeyValModel::index(int row, int column, const QModelIndex &parent_idx) const
{
    if (hasIndex(row, column, parent_idx)) {
	QgKeyValNode *cnode = IndexNode(parent_idx)->children.at(row);
	return createIndex(row, column, cnode);
    }
    return QModelIndex();
}

QModelIndex QgKeyValModel::parent(const QModelIndex &child) const
{
    QgKeyValNode *pnode = IndexNode(child)->parent;
    if (pnode == m_root) return QModelIndex();
    return createIndex(NodeRow(pnode), 0, pnode);
}

int QgKeyValModel::rowCount(const QModelIndex &parent_idx) const
{
    return IndexNode(parent_idx)->children.count();
}

int QgKeyValModel::columnCount(const QModelIndex &parent_idx) const
{
    Q_UNUSED(parent_idx);
    return 2;
}

QModelIndex QgKeyValModel::NodeIndex(QgKeyValNode *node) const
{
    if (node == m_root) return QModelIndex();
    return createIndex(NodeRow(node), 0, node);
}

QgKeyValNode * QgKeyValModel::IndexNode(const QModelIndex &idx) const
{
    if (idx.isValid()) {
	return static_cast<QgKeyValNode *>(idx.internalPointer());
    }
    return m_root;
}

int QgKeyValModel::NodeRow(QgKeyValNode *node) const
{
    return node->parent->children.indexOf(node);
}

QgKeyValNode *
QgKeyValModel::add_pair(const char *name, const char *value, QgKeyValNode *curr_node, int type)
{
    QgKeyValNode *new_node = new QgKeyValNode(curr_node);
    new_node->name = name;
    new_node->value = value;
    new_node->attr_type = type;
    return new_node;
}


// *********** View **************
void QgKeyValDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QString text = index.data().toString();
    painter->drawText(option.rect, text, QTextOption(Qt::AlignLeft));
}

QSize QgKeyValDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize name_size = option.fontMetrics.size(Qt::TextSingleLine, index.data().toString());
    return name_size;
}


QgKeyValView::QgKeyValView(QWidget *pparent, int tree_decorate) : QTreeView(pparent)
{
    //this->setContextMenuPolicy(Qt::CustomContextMenu);
    if (!tree_decorate) {
	setRootIsDecorated(false);
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


