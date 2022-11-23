/*                        Q G U T I L . H
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
/** @file QgUtil.h
 *
 * General purpose .g related Qt utility functions
 */

#ifndef QGUTIL_H
#define QGUTIL_H

#include "common.h"

#include <QImage>
#include "qtcad/defines.h"

#ifndef Q_MOC_RUN
#include "raytrace.h"
#include "ged.h"
#endif


#define G_STANDARD_OBJ 0 /** @brief no region or air flags set, or not a comb */
#define G_REGION 1       /** @brief region flag set, no air flag set */
#define G_AIR 2          /** @brief non-zero aircode set, no non-zero region_id set */
#define G_AIR_REGION 3   /** @brief both non-zero aircode and non-zero region_id set (error case) */
#define G_ASSEMBLY 4     /** @brief a comb object above one or more regions */

/**
 * Given a dp and its parent dbip, report its comb type (non-combs will
 * return G_STANDARD_OBJ)
 */
extern QTCAD_EXPORT int
QgCombType(struct directory *dp, struct db_i *dbip);

/* Given a dp and its parent dbip, return an appropriate icon for the object
 * type.  For combs uses the above QgCombType scheme, for primitives returns a
 * representative icon depicting a characteristic primitive shape. */
extern QTCAD_EXPORT QImage
QgIcon(struct directory *dp, struct db_i *dbip);


#endif //QGUTIL_H

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

