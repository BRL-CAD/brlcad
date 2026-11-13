/*             Q G T R E E S E L E C T I O N M O D E L . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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

#include <QItemSelectionModel>

#include "qtcad/defines.h"

class QgTreeView;

class QTCAD_EXPORT QgTreeSelectionModel : public QItemSelectionModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QgTreeSelectionModel)

    public:

	QgTreeSelectionModel(QAbstractItemModel *model, QObject *parent, QgTreeView *tree_view) :
	    QItemSelectionModel(model, parent), m_tree_view(tree_view) {}
	QgTreeSelectionModel(QAbstractItemModel *model = nullptr, QgTreeView *tree_view = nullptr) :
	    QItemSelectionModel(model), m_tree_view(tree_view) {}

	void clear_all();
	QgTreeView *treeView() const { return m_tree_view; }

    public slots:
	void select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags flags) override;
	void select(const QModelIndex &index, QItemSelectionModel::SelectionFlags flags) override;

    private:
	QgTreeView *m_tree_view = nullptr;
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

