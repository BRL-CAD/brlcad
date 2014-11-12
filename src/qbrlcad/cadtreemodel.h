/*                  C A D T R E E M O D E L . H
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
/** @file cadtreemodel.h
 *
 * Defines a Qt tree model for BRL-CAD's geometry database format.
 *
 */

#ifndef CAD_TREEMODEL_H
#define CAD_TREEMODEL_H

#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 6) && !defined(__clang__)
#  pragma message "Disabling GCC float equality comparison warnings via pragma due to Qt headers..."
#endif
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#undef Success
#include <QtCore>
#undef Success
#include <QAbstractItemModel>
#undef Success
#include <QObject>
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif


#ifndef Q_MOC_RUN
#include "bu/file.h"
#include "raytrace.h"
#include "ged.h"
#endif

#define CADTREE_RECURSION_LIMIT 100000

class CADTreeNode;

class CADTreeModel : public QAbstractItemModel
{
    Q_OBJECT

    public:
	explicit CADTreeModel(QObject *parent = 0);
	~CADTreeModel();

	QModelIndex index(int row, int column, const QModelIndex &parent) const;
	QModelIndex parent(const QModelIndex &child) const;
	int rowCount(const QModelIndex &child) const;
	int columnCount(const QModelIndex &child) const;

	void setRootNode(CADTreeNode *root);
	CADTreeNode* rootNode();

	int populate(struct db_i *new_dbip);
	bool hasChildren(const QModelIndex &parent) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

	QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
	bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);

	CADTreeNode *m_root;
	QModelIndex NodeIndex(CADTreeNode *node) const;
	CADTreeNode *IndexNode(const QModelIndex &index) const;

	QMap<struct directory *, struct rt_db_internal *> combinternals;

    public slots:
	void refresh();
	void update_selected_node_relationships(const QModelIndex & index);
	void expand_tree_node_relationships(const QModelIndex&);
	void close_tree_node_relationships(const QModelIndex&);

    protected:

	int NodeRow(CADTreeNode *node) const;
	bool canFetchMore(const QModelIndex &parent) const;
	void fetchMore(const QModelIndex &parent);
	void cad_add_children(union tree *tp, int op, CADTreeNode *curr_node);
	void cad_add_child(const char *name, CADTreeNode *curr_node, int op);

    private:
	struct db_i *current_dbip;
	QSet<CADTreeNode *> all_nodes;
};

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

