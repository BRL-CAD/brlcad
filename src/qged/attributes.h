/*                 C A D A T T R I B U T E S . H
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
/** @file cadattributes.h
 *
 * Brief description
 *
 */

#ifndef CAD_ATTRIBUTES_H
#define CAD_ATTRIBUTES_H

#include <QVariant>
#include <QString>
#include <QList>
#include <QImage>
#include <QAbstractItemModel>
#include <QObject>
#include <QTreeView>
#include <QModelIndex>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QUrl>

#ifndef Q_MOC_RUN
#include "bu/avs.h"
#include "raytrace.h"
#include "ged.h"
#endif

class QKeyValNode
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

// "Building-block" class for making a Key/Value display froma TreeModel+TreeView
class QKeyValModel : public QAbstractItemModel
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

class CADAttributesModel : public QKeyValModel
{
    Q_OBJECT

    public:  // "standard" custom tree model functions
	explicit CADAttributesModel(QObject *parent = 0, struct db_i *dbip = DBI_NULL, struct directory *dp = RT_DIR_NULL, int show_std = 0, int show_user = 0);
	~CADAttributesModel();

	bool hasChildren(const QModelIndex &parent) const;
	int update(struct db_i *new_dbip, struct directory *dp);

    public slots:
	void refresh(const QModelIndex &idx);

    protected:
	bool canFetchMore(const QModelIndex &parent) const;
	void fetchMore(const QModelIndex &parent);

    private:
	void add_Children(const char *name, QKeyValNode *curr_node);
	struct db_i *current_dbip;
	struct directory *current_dp;
	struct bu_attribute_value_set *avs;
	int std_visible;
	int user_visible;
};

class QKeyValDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    public:
	QKeyValDelegate(QWidget *pparent = 0) : QStyledItemDelegate(pparent) {}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

class QKeyValView : public QTreeView
{
    Q_OBJECT

    public:
	QKeyValView(QWidget *pparent, int decorate_tree = 0);
	~QKeyValView() {};
};

#endif /*CAD_ATTRIBUTES_H*/

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

