/*                      Q T C A D T R E E . H
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
/** @file cadtreenode.h
 *
 * Qt widgets for viewing a .g geometry hierarchy.
 *
 */

#ifndef QTCADTREE_H
#define QTCADTREE_H

// Getting reports of conflict between MSVC math defines and qmath.h - try idea from
// https://bugreports.qt.io/browse/QTBUG-45935
#undef _USE_MATH_DEFINES

#include <QAbstractItemModel>
#include <QHash>
#include <QHeaderView>
#include <QImage>
#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QResizeEvent>
#include <QString>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QUrl>
#include <QVariant>
#include "qtcad/defines.h"

#ifndef Q_MOC_RUN
#include "bu/file.h"
#include "raytrace.h"
#include "ged.h"
#endif

/* Need to have a sense of how deep to go before bailing - a cyclic
 * path is a possibility and would be infinite recursion, so we need
 * some limit.  If there is a limit in BRL-CAD go with that, but until
 * it is found use this */
#define CADTREE_RECURSION_LIMIT 10000

class QTCAD_EXPORT CADTreeNode
{
public:
    CADTreeNode(struct directory *in_dp = RT_DIR_NULL, CADTreeNode *aParent=0);
    CADTreeNode(QString dp_name, CADTreeNode *aParent=0);
    ~CADTreeNode();

    QString name;
    int boolean;
    int is_highlighted;
    int instance_highlight;
    struct directory *node_dp;
    QVariant icon;

    CADTreeNode *parent;
    QList<CADTreeNode*> children;
};

class QTCAD_EXPORT CADTreeView;

class QTCAD_EXPORT CADTreeModel : public QAbstractItemModel
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

	QHash<struct directory *, struct rt_db_internal *> combinternals;

	// These came from CADApp in qged...
	QModelIndex current_idx;
	int interaction_mode = 0; // 0 = view, 1 = instance edit, 2 = primitive edit
	struct db_i *dbip = NULL;
	CADTreeView *cadtreeview = NULL;

	unsigned long long db_hash(struct db_i *);

    public slots:
	void refresh(void *);
        void mode_change(int);
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

class QTCAD_EXPORT GObjectDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    public:
	GObjectDelegate(QWidget *pparent = 0) : QStyledItemDelegate(pparent) {}
	GObjectDelegate(CADTreeView *tv = NULL, QWidget *pparent = 0) : QStyledItemDelegate(pparent) {cadtreeview = tv;}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

	// Previously from CADApp in qged...
	CADTreeView *cadtreeview = NULL;
};

class QTCAD_EXPORT CADTreeView : public QTreeView
{
    Q_OBJECT

    public:
	CADTreeView(QWidget *pparent, CADTreeModel *treemodel);
	~CADTreeView() {};

	QModelIndex selected();

	void drawBranches(QPainter* painter, const QRect& rrect, const QModelIndex& index) const;
	CADTreeModel *m = NULL;

    protected:
	void resizeEvent(QResizeEvent *pevent);

    public slots:
	void tree_column_size(const QModelIndex &index);
	void context_menu(const QPoint &point);
	void expand_path(QString path);
	void expand_link(const QUrl &link);

    private:
        void header_state();
};

#endif //QTCADTREE_H

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

