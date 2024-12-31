/*                         E D D A T U M . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2024 United States Government as represented by
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
/** @file mged/primitives/eddatum.c
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../mged.h"
#include "../sedit.h"
#include "../mged_dm.h"
#include "./eddatum.h"


#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_datum_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_datum_internal *datum = (struct rt_datum_internal *)ip->idb_ptr;
    RT_DATUM_CK_MAGIC(datum);

    do {
	if (!ZERO(datum->w))
	    bu_vls_printf(p, "Plane: %.9f %.9f %.9f (pnt) %.9f %.9f %.9f (dir) %.9f (scale)\n", V3BASE2LOCAL(datum->pnt), V3BASE2LOCAL(datum->dir), datum->w);
	else if (!ZERO(MAGNITUDE(datum->dir)))
	    bu_vls_printf(p, "Line: %.9f %.9f %.9f (pnt) %.9f %.9f %.9f (dir)\n", V3BASE2LOCAL(datum->pnt), V3BASE2LOCAL(datum->dir));
	else
	    bu_vls_printf(p, "Point: %.9f %.9f %.9f\n", V3BASE2LOCAL(datum->pnt));
    } while ((datum = datum->next));
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
