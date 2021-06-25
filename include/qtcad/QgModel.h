/*                        Q G M O D E L . H
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
/** @file QgModel.h
 *
 * Abstract Qt model of a BRL-CAD .g database
 *
 * NOTE - the QtCADTree model has many of the pieces we will need,
 * but has limitations.  The intent here is to start "from the
 * ground up" and be guided by the Qt editable example:
 *
 * https://doc.qt.io/qt-5/qtwidgets-itemviews-editabletreemodel-example.html
 *
 */

#ifndef QGMODEL_H
#define QGMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>

#include "qtcad/defines.h"

#ifndef Q_MOC_RUN
#include "raytrace.h"
#endif

class QTCAD_EXPORT QgModel : public QAbstractItemModel
{
    Q_OBJECT

    public:
	QgModel(QObject *p = NULL);
	~QgModel();

	Qt::ItemFlags flags(const QModelIndex &index);
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole);
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole);
	int rowCount(const QModelIndex &parent = QModelIndex());
	int columnCount(const QModelIndex &parent = QModelIndex());

	bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
	bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role = Qt::EditRole);

	bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex());
	bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
	bool insertColumns(int column, int count, const QModelIndex &parent = QModelIndex());
	bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex());

	struct db_i *dbip;
};

#endif //QGMODEL_H

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

