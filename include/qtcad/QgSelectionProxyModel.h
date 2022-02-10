/*             Q G S E L E C T I O N P R O X Y M O D E L . H
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
/** @file QgSelectionProxyModel.h
 *
 * The Qg Selection Proxy model intended to sit between a QgTreeView and a
 * QgModel in order to store the necessary information to manage selection
 * state.
 *
 * In the original QtCADTree code the selection information ended up being
 * stored with the model.  Instead of replicating that approach, we instead
 * subclass QIdentityProxyModel to create a purpose-built model for managing
 * the selection states. The .g database does not define a notion of "currently
 * selected" so storing that information in QgModel is a poor fit for data.  We
 * don't want QSortFilterProxyModel for a hierarchical tree view, since we
 * don't actually want sorting behavior in tree views (list views may be
 * another matter, but the initial focus is on the tree view.) Rather, we want
 * a "related highlighted" set in response to changing selection and
 * interaction mode settings. A "QgSelectionModel" class will hold and manage
 * that data, as a proxy between the "proper" .g model and the view - that way
 * we don't have to pollute either the .g model or the view implementation
 * managing the extra state.
 *
 * See:
 * https://doc.qt.io/qt-5/qidentityproxymodel.html
 * https://forum.qt.io/topic/120258/adding-data-throught-qidentitymodel-to-qsqltablemodel-fails
 */

#ifndef QGSELECTIONMODEL_H
#define QGSELECTIONMODEL_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <QIdentityProxyModel>
#include <QModelIndex>

#include "qtcad/defines.h"
#include "qtcad/QgModel.h"

class QgTreeView;

#define QgViewMode 0
#define QgInstanceEditMode 1
#define QgPrimitiveEditMode 2

class QTCAD_EXPORT QgSelectionProxyModel : public QIdentityProxyModel
{
    public:

	QgSelectionProxyModel(QObject* parent = 0): QIdentityProxyModel(parent) {}

	enum CADDataRoles {
	    BoolInternalRole = Qt::UserRole + 1000,
	    BoolDisplayRole = Qt::UserRole + 1001,
	    DirectoryInternalRole = Qt::UserRole + 1002,
	    TypeIconDisplayRole = Qt::UserRole + 1003,
	    RelatedHighlightDisplayRole = Qt::UserRole + 1004,
	    InstanceHighlightDisplayRole = Qt::UserRole + 1005
	};

	QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
	bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);

	QgTreeView *treeview = NULL;

    public slots:
	//void refresh(void *);
	void mode_change(int i);
        void update_selected_node_relationships(const QModelIndex & index);
        void expand_tree_node_relationships(const QModelIndex&);
        void close_tree_node_relationships(const QModelIndex&);

    private:
	// There are a number of relationships which can be used for related
	// node highlighting - this allows a client application to select one.
	int interaction_mode = 0;

	// Activity flags (used for relevance highlighting) need to be updated
	// whenever a selection changes.  We define one flag per gInstance,
	// and update flags accordingly based on current app settings.
	// Highlighting is then applied in the view based on the current value
	// of these flags.
	std::vector<int> active_flags;
};

#endif //QGSELECTIONMODEL_H

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

