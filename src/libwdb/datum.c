/*                         D A T U M . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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
/** @file libwdb/arbn.c
 *
 * libwdb support for writing an ARBN.
 *
 */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "bu/malloc.h"
#include "vmath.h"
#include "bn.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"


/* TODO: this is no good, needs a more flexible ordered way to specify N datums */
int
mk_datum(struct rt_wdb *wdbp, const char *name, const point_t point, const vect_t direction, fastf_t plane_factor)
{
    struct rt_datum_internal *datum;

    RT_CK_WDB(wdbp);

    if (!name)
	return -1;

    BU_ALLOC(datum, struct rt_datum_internal);
    VMOVE(datum->pnt, point);
    VMOVE(datum->dir, direction);
    datum->w = plane_factor;
    datum->next = NULL;

    datum->magic = RT_DATUM_INTERNAL_MAGIC;

    return wdb_export(wdbp, name, (void *)datum, ID_DATUM, mk_conv2mm);
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
