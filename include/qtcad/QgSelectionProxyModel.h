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
 * TODO - I think this needs to be turned into QgTreeSelectionModel
 * (a subclass of QItemSelectionModel) and managed as a selection
 * model for the QgTreeView rather than a proxy model on the QgModel
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

class QTCAD_EXPORT QgSelectionProxyModel : public QIdentityProxyModel
{
    public:

	QgSelectionProxyModel(QObject* parent = 0): QIdentityProxyModel(parent) {}
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

