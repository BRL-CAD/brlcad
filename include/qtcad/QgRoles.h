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
 * Centralised Qt model-data role IDs for BRL-CAD libqtcad models.
 *
 * These role constants extend Qt::UserRole and are used by QgModel
 * (and any downstream QAbstractItemModel subclass) to carry
 * BRL-CAD-specific data through the Qt model/view machinery.  Keeping
 * them in a single header means consumers (tree delegates, attribute
 * models, tests) can obtain the IDs without including the heavier
 * QgModel.h.
 *
 * Role IDs (Qt::UserRole offsets):
 *   +1000  BoolInternalRole       — boolean operator (int, matches db_op_t)
 *   +1001  BoolDisplayRole        — display label for the boolean operator
 *   +1002  DirectoryInternalRole  — opaque struct directory * (as void *)
 *   +1003  TypeIconDisplayRole    — primitive-type icon (QImage)
 *   +1004  HighlightDisplayRole   — highlight flag (int)
 *   +1005  DrawnDisplayRole       — drawn-in-view flag (int)
 *   +1006  SelectDisplayRole      — selected flag (int)
 */

#ifndef QGROLES_H
#define QGROLES_H

#include <QtGlobal>

/**
 * Data-role IDs understood by QgModel and its delegates.
 *
 * All values are anchored to Qt::UserRole so they stay out of Qt's
 * reserved ranges.
 */
enum QgDataRoles {
    BoolInternalRole      = Qt::UserRole + 1000,  /**< boolean operator (int) */
    BoolDisplayRole       = Qt::UserRole + 1001,  /**< display text for bool op */
    DirectoryInternalRole = Qt::UserRole + 1002,  /**< struct directory* as void* */
    TypeIconDisplayRole   = Qt::UserRole + 1003,  /**< primitive type icon (QImage) */
    HighlightDisplayRole  = Qt::UserRole + 1004,  /**< highlight flag (int) */
    DrawnDisplayRole      = Qt::UserRole + 1005,  /**< drawn-in-view flag (int) */
    SelectDisplayRole     = Qt::UserRole + 1006   /**< selected flag (int) */
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
