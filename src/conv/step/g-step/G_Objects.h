/*                    G _ O B J E C T S . H
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file G_Objects.h
 *
 */

#ifndef G_OBJECTS_H
#define G_OBJECTS_H

#include "AP_Common.h"
#include <string>

bool Object_To_STEP(struct directory *dp, struct rt_db_internal *intern,
	struct rt_wdb *wdbp, AP203_Contents *sc,
	std::string *diagnostic = NULL);

/** Emit only the geometric representation for a BRL-CAD object.  Imported
 * product containers use this path so representation items do not acquire
 * synthetic PRODUCT identities of their own. */
bool Object_Geometry_To_STEP(struct directory *dp,
	struct rt_db_internal *intern, struct rt_wdb *wdbp,
	AP203_Contents *sc, std::string *diagnostic = NULL);

#endif /* G_OBJECTS_H */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
