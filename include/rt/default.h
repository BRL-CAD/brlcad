/*                       D E F A U L T . H
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
/** @addtogroup db_default
 *
 * @brief Deterministic default object selection for geometry databases.
 *
 */
/** @{ */
/** @file rt/default.h */

#ifndef RT_DEFAULT_H
#define RT_DEFAULT_H

#include "common.h"
#include "rt/defines.h"

__BEGIN_DECLS

struct bu_vls;
struct db_i;
struct directory;

/**
 * Default object attribute name stored on the v5 @c _GLOBAL object.
 */
#define DB_DEFAULT_OBJECT_ATTR "default_object"

/**
 * Select a deterministic default geometry object for commands that can operate
 * on one database object but were not given an explicit object name.
 *
 * The selection is intentionally conservative: a successful return means librt
 * found exactly one geometry object according to the default-object rules.  If
 * the database disables default-object selection, if a rule has ambiguous
 * matches, or if no rule matches, no object is selected and a printable
 * explanation is written to @p msg when supplied.
 *
 * Selection order:
 *
 * 1. If the v5 @c _GLOBAL object has @c DB_DEFAULT_OBJECT_ATTR set to a
 *    non-empty, non-negative value, use that value as an exact object name.
 *    Empty/false/negative values disable automatic default-object selection.
 * 2. If there is exactly one non-hidden top-level geometry object, select it.
 * 3. If there is exactly one non-hidden combination in the database, select it.
 * 4. If there is exactly one non-hidden combination whose name without its
 *    final extension matches the database filename without its final extension,
 *    select it.
 * 5. If there is exactly one top-level non-hidden combination whose name
 *    case-insensitively matches @c all* or @c scene*, select it.
 *
 * @param[in] dbip
 * Open database instance to inspect.  The routine updates database reference
 * counts with db_update_nref() before testing top-level status.
 *
 * @param[out] dp
 * Optional output pointer.  On successful selection, @c *dp is set to the
 * selected directory pointer.  On no selection or error, @c *dp is set to
 * @c RT_DIR_NULL when @p dp is non-NULL.
 *
 * @param[in,out] msg
 * Optional caller-owned, initialized @c bu_vls.  On return 0 or -1,
 * explanatory text is appended, including the available top-level objects when
 * known.  On successful selection, the routine leaves @p msg unchanged.
 *
 * @return 1 when a unique default object was selected.
 * @return 0 when no object was selected because selection is disabled,
 * ambiguous, or no deterministic match exists.
 * @return -1 on internal failure.
 */
RT_EXPORT extern int db_default_object(struct db_i *dbip,
				       struct directory **dp,
				       struct bu_vls *msg);

__END_DECLS

#endif /* RT_DEFAULT_H */
/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
