/*                        Q G T Y P E S . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file QgTypes.h
 *
 * Shared strongly typed libqtcad values.
 */

#ifndef QGTYPES_H
#define QGTYPES_H

#include <QtGlobal>

enum class QgViewUpdateFlag : int {
    Refresh = 0x00000001,
    Drawn = 0x00000002,
    Select = 0x00000004,
    Mode = 0x00000008,
    DB = 0x00000010
};
Q_DECLARE_FLAGS(QgViewUpdateFlags, QgViewUpdateFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(QgViewUpdateFlags)

inline const QgViewUpdateFlags QG_VIEW_REFRESH = QgViewUpdateFlag::Refresh;
inline const QgViewUpdateFlags QG_VIEW_DRAWN = QgViewUpdateFlag::Drawn;
inline const QgViewUpdateFlags QG_VIEW_SELECT = QgViewUpdateFlag::Select;
inline const QgViewUpdateFlags QG_VIEW_MODE = QgViewUpdateFlag::Mode;
inline const QgViewUpdateFlags QG_VIEW_DB = QgViewUpdateFlag::DB;

#endif /* QGTYPES_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
