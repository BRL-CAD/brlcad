/*                         C L I N E . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2011 United States Government as represented by
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
/** @file libwdb/cline.c
 *
 * Support for cline solids (kludges from FASTGEN)
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

int
mk_cline(
    struct rt_wdb *fp,
    const char *name,
    const point_t V,
    const vect_t height,
    fastf_t radius,
    fastf_t thickness)
{
    struct rt_cline_internal *cli;

    BU_GET(cli, struct rt_cline_internal);
    cli->magic = RT_CLINE_INTERNAL_MAGIC;
    VMOVE(cli->v, V);
    VMOVE(cli->h, height);
    cli->thickness = thickness;
    cli->radius = radius;

    return wdb_export(fp, name, (genptr_t)cli, ID_CLINE, mk_conv2mm);
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
