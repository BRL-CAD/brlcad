/*                         M A T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2022 United States Government as represented by
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

/** @file libwdb/mater.c
 *
 * Interface for writing region-id-based color tables to the database.
 *
 */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bn.h"
#include "rt/geom.h"
#include "rt/db4.h"
#include "raytrace.h"
#include "wdb.h"

int
mk_write_color_table(struct rt_wdb *ofp)
{
    RT_CK_WDB(ofp);
    if (db_version(ofp->dbip) < 5) {
	BU_ASSERT(mk_version == 4);

	bu_log("mk_write_color_table(): not implemented for v4 database\n");
    } else {
	return db5_put_color_table(ofp->dbip);
    }
    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
