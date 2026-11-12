/*                       Q G R O L E S . H
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
/** @file QgRoles.h
 *
 * BRL-CAD-specific Qt model data roles shared by libqtcad models and
 * views.  Keeping these identifiers independent of QgModel lets reusable
 * consumers interpret compatible models without including QgModel.h.
 */

#ifndef QGROLES_H
#define QGROLES_H

#include <Qt>

enum QgDataRoles {
    BoolInternalRole = Qt::UserRole + 1000,
    BoolDisplayRole = Qt::UserRole + 1001,
    DirectoryInternalRole = Qt::UserRole + 1002,
    TypeIconDisplayRole = Qt::UserRole + 1003,
    HighlightDisplayRole = Qt::UserRole + 1004,
    DrawnDisplayRole = Qt::UserRole + 1005,
    SelectDisplayRole = Qt::UserRole + 1006
};

#endif /* QGROLES_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
