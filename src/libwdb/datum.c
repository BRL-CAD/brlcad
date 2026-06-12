/*                         D A T U M . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
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


int
mk_datums(struct rt_wdb *fp, const char *name, struct rt_datum_internal *head)
{
    struct rt_datum_internal *dup_head = NULL;
    struct rt_datum_internal *tail = NULL;
    const struct rt_datum_internal *src;

    RT_CK_WDB(fp);

    if (!name || !head)
	return -1;

    /* Duplicate the caller's chain so the exported internal is
     * well-formed and owns its own nodes.  We do not take ownership of,
     * or mutate, the caller's list.  Each element is classified by the
     * exporter from its pnt/dir/w values:
     *   point: dir == {0,0,0}, w == 0
     *   line:  MAGNITUDE(dir) > 0, w == 0
     *   plane: w != 0 (pnt on plane, dir is normal)
     */
    for (src = head; src; src = src->next) {
	struct rt_datum_internal *datum;

	BU_ALLOC(datum, struct rt_datum_internal);
	VMOVE(datum->pnt, src->pnt);
	VMOVE(datum->dir, src->dir);
	datum->w = src->w;
	datum->next = NULL;
	datum->magic = RT_DATUM_INTERNAL_MAGIC;

	if (tail)
	    tail->next = datum;
	else
	    dup_head = datum;
	tail = datum;
    }

    return wdb_export(fp, name, (void *)dup_head, ID_DATUM, mk_conv2mm);
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
