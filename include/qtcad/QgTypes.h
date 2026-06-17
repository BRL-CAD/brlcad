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
 * Shared enum-class based libqtcad type definitions.
 */

#ifndef QGTYPES_H
#define QGTYPES_H

#include <QtGlobal>

enum class QgViewType : int {
	Auto = 0,
	SW = 1,
	GL = 2
};

enum class QgQuadrantId : int {
	UpperRight = 0,
	UpperLeft = 1,
	LowerLeft = 2,
	LowerRight = 3
};

enum class QgTreeInteractionMode : int {
	View = 0,
	InstanceEdit = 1,
	PrimitiveEdit = 2
};

enum class QgCombTypeId : int {
	StandardObj = 0,
	Region = 1,
	Air = 2,
	AirRegion = 3,
	Assembly = 4
};

enum class QgViewUpdateFlag : int {
    Refresh = 0x00000001,
    Drawn = 0x00000002,
    Select = 0x00000004,
    Mode = 0x00000008,
    DB = 0x00000010
};
Q_DECLARE_FLAGS(QgViewUpdateFlags, QgViewUpdateFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(QgViewUpdateFlags)

// Compatibility constants (one release transition away from macro defines)
inline constexpr int QgView_AUTO = static_cast<int>(QgViewType::Auto);
inline constexpr int QgView_SW = static_cast<int>(QgViewType::SW);
inline constexpr int QgView_GL = static_cast<int>(QgViewType::GL);

inline constexpr int UPPER_RIGHT_QUADRANT = static_cast<int>(QgQuadrantId::UpperRight);
inline constexpr int UPPER_LEFT_QUADRANT = static_cast<int>(QgQuadrantId::UpperLeft);
inline constexpr int LOWER_LEFT_QUADRANT = static_cast<int>(QgQuadrantId::LowerLeft);
inline constexpr int LOWER_RIGHT_QUADRANT = static_cast<int>(QgQuadrantId::LowerRight);

inline constexpr int QgViewMode = static_cast<int>(QgTreeInteractionMode::View);
inline constexpr int QgInstanceEditMode = static_cast<int>(QgTreeInteractionMode::InstanceEdit);
inline constexpr int QgPrimitiveEditMode = static_cast<int>(QgTreeInteractionMode::PrimitiveEdit);

inline constexpr int G_STANDARD_OBJ = static_cast<int>(QgCombTypeId::StandardObj);
inline constexpr int G_REGION = static_cast<int>(QgCombTypeId::Region);
inline constexpr int G_AIR = static_cast<int>(QgCombTypeId::Air);
inline constexpr int G_AIR_REGION = static_cast<int>(QgCombTypeId::AirRegion);
inline constexpr int G_ASSEMBLY = static_cast<int>(QgCombTypeId::Assembly);

// Canonical QgViewUpdateFlags constants for all view-flag signals and slots.
// Use these in signal emissions and slot flag checks instead of raw integers.
inline const QgViewUpdateFlags QG_VIEW_REFRESH = QgViewUpdateFlag::Refresh;
inline const QgViewUpdateFlags QG_VIEW_DRAWN   = QgViewUpdateFlag::Drawn;
inline const QgViewUpdateFlags QG_VIEW_SELECT  = QgViewUpdateFlag::Select;
inline const QgViewUpdateFlags QG_VIEW_MODE    = QgViewUpdateFlag::Mode;
inline const QgViewUpdateFlags QG_VIEW_DB      = QgViewUpdateFlag::DB;

#endif /* QGTYPES_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
