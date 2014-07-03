/*                            I D . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2014 United States Government as represented by
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
/** @file libwdb/id.c
 *
 * An ID record must be the first granule in the database.
 *
 * Return codes of 0 are OK, -1 signal an error.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "db.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"


int
mk_id(struct rt_wdb *fp, const char *title)
{
    return mk_id_editunits(fp, title, 1.0);
}


int
mk_id_units(struct rt_wdb *fp, const char *title, const char *units)
{
    return mk_id_editunits(fp, title, bu_units_conversion(units));
}


int
mk_id_editunits(
    struct rt_wdb *wdbp,
    const char *title,
    double local2mm)
{
    RT_CK_WDB(wdbp);
    return db_update_ident(wdbp->dbip, title, local2mm);
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
