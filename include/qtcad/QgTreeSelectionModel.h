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

#include "qtcad/defines.h"
#include "qtcad/QgModel.h"

#define QgViewMode 0
#define QgInstanceEditMode 1
#define QgPrimitiveEditMode 2

class QTCAD_EXPORT QgTreeSelectionModel : public QItemSelectionModel
{
    Q_OBJECT

    public:

	QgTreeSelectionModel(QAbstractItemModel *model, QObject* parent): QItemSelectionModel(model, parent) {}
	QgTreeSelectionModel(QAbstractItemModel *model = nullptr): QItemSelectionModel(model) {}

    public slots:
	void select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags flags) override;
        void select(const QModelIndex &index, QItemSelectionModel::SelectionFlags flags) override;

    public:
	// There are a number of relationships which can be used for related
	// node highlighting - this allows a client application to select one.
	int interaction_mode = 0;
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

