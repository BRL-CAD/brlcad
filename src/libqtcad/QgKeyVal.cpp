/*                    Q G K E Y V A L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
: QAbstractItemModel(aParent), m_root(nullptr)
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
    if (!curr_node) return QVariant();
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
    if (!curr_node) return false;
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
    beginResetModel();
    m_root = root;
    endResetModel();
}

QModelIndex QgKeyValModel::index(int row, int column, const QModelIndex &parent_idx) const
{
    if (!hasIndex(row, column, parent_idx)) return QModelIndex();
    QgKeyValNode *parent_node = IndexNode(parent_idx);
    if (!parent_node || row >= parent_node->children.count())
	return QModelIndex();
    return createIndex(row, column, parent_node->children.at(row));
}

QModelIndex QgKeyValModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || child.model() != this) return QModelIndex();
    QgKeyValNode *child_node = IndexNode(child);
    if (!child_node || child_node == m_root || !child_node->parent)
	return QModelIndex();
    QgKeyValNode *parent_node = child_node->parent;
    if (parent_node == m_root) return QModelIndex();
    const int parent_row = NodeRow(parent_node);
    return parent_row >= 0 ? createIndex(parent_row, 0, parent_node) :
	QModelIndex();
}

int QgKeyValModel::rowCount(const QModelIndex &parent_idx) const
{
    if (parent_idx.isValid() && parent_idx.column() != 0) return 0;
    QgKeyValNode *parent_node = IndexNode(parent_idx);
    return parent_node ? parent_node->children.count() : 0;
}

int QgKeyValModel::columnCount(const QModelIndex &parent_idx) const
{
    Q_UNUSED(parent_idx);
    return 2;
}

QModelIndex QgKeyValModel::NodeIndex(QgKeyValNode *node) const
{
    if (!node || node == m_root) return QModelIndex();
    const int row = NodeRow(node);
    return row >= 0 ? createIndex(row, 0, node) : QModelIndex();
}

QgKeyValNode * QgKeyValModel::IndexNode(const QModelIndex &idx) const
{
    if (idx.isValid()) {
	if (idx.model() != this) return nullptr;
	return static_cast<QgKeyValNode *>(idx.internalPointer());
    }
    return m_root;
}

int QgKeyValModel::NodeRow(QgKeyValNode *node) const
{
    if (!node || !node->parent) return -1;
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


