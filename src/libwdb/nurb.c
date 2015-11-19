/*                          N U R B . C
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

/** @file libwdb/nurb.c
 *
 * Library for writing NURB objects into
 * MGED databases from arbitrary procedures.
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>
#include "bio.h"

#include "rt/db4.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rt/nurb.h"
#include "rt/geom.h"
#include "wdb.h"


int
mk_bspline(struct rt_wdb *wdbp, const char *name, struct face_g_snurb **surfs)
{
    struct rt_nurb_internal *ni;

    BU_ALLOC(ni, struct rt_nurb_internal);
    ni->magic = RT_NURB_INTERNAL_MAGIC;
    ni->srfs = surfs;

    for (ni->nsrf = 0; ni->srfs[ni->nsrf] != NULL; ni->nsrf++)
	; /* NIL */

    return wdb_export(wdbp, name, (void *)ni, ID_BSPLINE, mk_conv2mm);
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
