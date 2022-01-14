/*                        Q K E Y V A L . H
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
/** @file QKeyVal.h
 *
 */

#ifndef QKEYVAL_H
#define QKEYVAL_H

#include "common.h"

#include <QAbstractItemModel>
#include <QHeaderView>
#include <QObject>
#include <QString>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QVariant>

#include "qtcad/defines.h"

class QTCAD_EXPORT QKeyValNode
  {
  public:
      QKeyValNode(QKeyValNode *aParent=0);
      ~QKeyValNode();

      QString name;
      QString value;
      int attr_type;

      QKeyValNode *parent;
      QList<QKeyValNode*> children;
};

class QTCAD_EXPORT QKeyValModel : public QAbstractItemModel
{
    Q_OBJECT

    public:
	QKeyValModel(QObject *p = NULL);
	~QKeyValModel();

	QModelIndex index(int row, int column, const QModelIndex &parent) const;
	QModelIndex parent(const QModelIndex &child) const;
	int rowCount(const QModelIndex &child) const;
	int columnCount(const QModelIndex &child) const;

	void setRootNode(QKeyValNode *root);
	QKeyValNode* rootNode();

	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

	QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
	bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);

	QKeyValNode *m_root;
	QModelIndex NodeIndex(QKeyValNode *node) const;
	QKeyValNode *IndexNode(const QModelIndex &index) const;

	QKeyValNode *add_pair(const char *name, const char *value, QKeyValNode *curr_node, int type);

	int NodeRow(QKeyValNode *node) const;
};

class QTCAD_EXPORT QKeyValDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    public:
	QKeyValDelegate(QWidget *pparent = 0) : QStyledItemDelegate(pparent) {}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

class QTCAD_EXPORT QKeyValView : public QTreeView
{
    Q_OBJECT

    public:
	QKeyValView(QWidget *pparent, int decorate_tree = 0);
	~QKeyValView() {};
};


#endif /* QKEYVAL_H */


