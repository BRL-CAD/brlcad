/*             Q G T R E E S E L E C T I O N M O D E L . H
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
/** @file QgTreeSelectionModel.h
*/

#ifndef QGTREESELECTIONMODEL_H
#define QGTREESELECTIONMODEL_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QObject>
#include "ged.h"
#include "qtcad/defines.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgTreeView.h"

#define QgViewMode 0
#define QgInstanceEditMode 1
#define QgPrimitiveEditMode 2

class QTCAD_EXPORT QgTreeSelectionModel : public QItemSelectionModel
{
    Q_OBJECT

    public:

	QgTreeSelectionModel(QAbstractItemModel *model, QObject* parent, QgTreeView *tv): QItemSelectionModel(model, parent) {treeview = tv;}
	QgTreeSelectionModel(QAbstractItemModel *model = nullptr, QgTreeView *tv = nullptr): QItemSelectionModel(model) {treeview = tv;}

    public slots:
	void select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags flags) override;
        void select(const QModelIndex &index, QItemSelectionModel::SelectionFlags flags) override;
	void mode_change(int i);
	void update_selected_node_relationships(const QModelIndex & index);

	// If we only need to check part of the tree (i.e. when opening a comb)
	// we can supply a non-null start QgItem
	void ged_selection_sync(QgItem *start, struct ged_selection_set *gs);
	// The selection sync won't clear deselected items from GED, since it
	// doesn't know about them - we need to handle those separately
	void ged_deselect(const QItemSelection &selected, const QItemSelection &deselected);

	// Sync GED drawn object information to QgItems, so the tree view can
	// display the drawn state.  If we only need to check part of the tree
	// (i.e. when opening a comb) we can supply a non-null start QgItem
	void ged_drawn_sync(QgItem *start, struct ged *gedp);

    public:
	// There are a number of relationships which can be used for related
	// node highlighting - this allows a client application to select one.
	int interaction_mode = 0;

	QgTreeView *treeview;

    private:
	bool ged_doing_sync = false;
};

#endif //QGTREESELECTIONMODEL_H

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

